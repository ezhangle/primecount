///
/// @file  S2_hard.cpp
/// @brief Calculate the contribution of the hard special leaves which
///        require use of a sieve (Deleglise-Rivat algorithm).
///        This is a parallel implementation which uses compression
///        (PiTable & FactorTable) to reduce the memory usage by
///        about 10x.
///
/// Copyright (C) 2015 Kim Walisch, <kim.walisch@gmail.com>
///
/// This file is distributed under the BSD License. See the COPYING
/// file in the top level directory.
///

#include <PiTable.hpp>
#include <FactorTable.hpp>
#include <primecount-internal.hpp>
#include <BitSieve.hpp>
#include <int128.hpp>
#include <min_max.hpp>
#include <pmath.hpp>
#include <generate.hpp>
#include <S2LoadBalancer.hpp>
#include <S2Status.hpp>
#include <tos_counters.hpp>
#include <fast_div.hpp>

#include <stdint.h>
#include <algorithm>
#include <vector>

#ifdef _OPENMP
  #include <omp.h>
#endif

/// Count the number of unsieved elements inside
/// [start, stop] from the sieve array.
///
#define COUNT_POPCNT(start, stop) \
    sieve.count(start, stop, low, high, count, count_low_high)

using namespace std;
using namespace primecount;

namespace {
namespace S2_hard {

/// Cross-off the multiples of prime in the sieve array.
/// @return  Count of crossed-off multiples.
///
int64_t cross_off(int64_t prime,
                  int64_t low,
                  int64_t high,
                  int64_t& next_multiple,
                  BitSieve& sieve)
{
  int64_t unset = 0;
  int64_t k = next_multiple;

  for (; k < high; k += prime * 2)
  {
    // +1 if k is unset the first time
    unset += sieve[k - low];
    sieve.unset(k - low);
  }

  next_multiple = k;
  return unset;
}

/// Cross-off the multiples of prime in the sieve array.
/// For each element that is unmarked the first time update
/// the special counters tree data structure.
///
template <typename T>
void cross_off(int64_t prime,
               int64_t low,
               int64_t high,
               int64_t& next_multiple,
               BitSieve& sieve,
               T& counters)
{
  int64_t segment_size = sieve.size();
  int64_t k = next_multiple;

  for (; k < high; k += prime * 2)
  {
    if (sieve[k - low])
    {
      sieve.unset(k - low);
      cnt_update(counters, k - low, segment_size);
    }
  }

  next_multiple = k;
}

/// @return  vector with multiples >= low.
template <typename T>
vector<int64_t> generate_next_multiples(int64_t low, int64_t size, vector<T>& primes)
{
  vector<int64_t> next;
  next.reserve(size);
  next.push_back(0);

  for (int64_t b = 1; b < size; b++)
  {
    int64_t prime = primes[b];
    int64_t next_multiple = ceil_div(low, prime) * prime;
    next_multiple += prime * (~next_multiple & 1);
    next.push_back(next_multiple);
  }

  return next;
}

/// @return  true if the interval [low, high] contains
///          few hard special leaves.
///
bool is_popcnt(int64_t low,
               int64_t high,
               int64_t y,
               double alpha)
{
  return (high < y || low > y * alpha);
}

/// Compute the S2 contribution of the hard special leaves which
/// require use of a sieve. Each thread processes the interval
/// [low_thread, low_thread + segments * segment_size[
/// and the missing special leaf contributions for the interval
/// [1, low_process[ are later reconstructed and added in
/// the parent S2_hard() function.
///
template <typename T, typename P, typename F>
T S2_hard_thread(T x,
                 int64_t y,
                 int64_t z,
                 int64_t c,
                 int64_t segment_size,
                 int64_t segments_per_thread,
                 int64_t thread_num,
                 int64_t low,
                 int64_t limit,
                 double alpha,
                 FactorTable<F>& factors,
                 PiTable& pi,
                 vector<P>& primes,
                 vector<int64_t>& mu_sum,
                 vector<int64_t>& phi)
{
  low += segment_size * segments_per_thread * thread_num;
  limit = min(low + segment_size * segments_per_thread, limit);
  int64_t max_b = pi[min3(isqrt(x / low), isqrt(z), y)];
  int64_t pi_sqrty = pi[isqrt(y)];
  T s2_hard = 0;

  if (c > max_b)
    return s2_hard;

  BitSieve sieve(segment_size);
  vector<int32_t> counters;
  vector<int64_t> next = generate_next_multiples(low, max_b + 1, primes);
  phi.resize(max_b + 1, 0);
  mu_sum.resize(max_b + 1, 0);

  // Segmented sieve of Eratosthenes
  for (; low < limit; low += segment_size)
  {
    // Current segment = interval [low, high[
    int64_t high = min(low + segment_size, limit);
    int64_t b = c + 1;

    sieve.fill(low, high);

    // phi(y, i) nodes with i <= c do not contribute to S2, so we
    // simply sieve out the multiples of the first c primes.
    for (int64_t i = 2; i <= c; i++)
    {
      int64_t k = next[i];
      for (int64_t prime = primes[i]; k < high; k += prime * 2)
        sieve.unset(k - low);
      next[i] = k;
    }

    // Calculate the contribution of the hard special leaves using the
    // POPCNT algorithm. If there are relatively few special leaves
    // per segment we count the number of unsieved elements directly
    // from the sieve array using the POPCNT instruction.
    if (is_popcnt(low, high, y, alpha))
    {
      int64_t count_low_high = sieve.count((high - 1) - low);

      // For c + 1 <= b <= pi_sqrty
      // Find all special leaves: n = primes[b] * m
      // which satisfy: mu[m] != 0 && primes[b] < lpf[m] && low <= (x / n) < high
      for (int64_t end = min(pi_sqrty, max_b); b <= end; b++)
      {
        int64_t prime = primes[b];
        T x2 = x / prime;
        int64_t x2_div_low = min(fast_div(x2, low), y);
        int64_t x2_div_high = min(fast_div(x2, high), y);
        int64_t min_m = max(x2_div_high, y / prime);
        int64_t max_m = x2_div_low;
        int64_t count = 0;
        int64_t start = 0;

        if (prime >= max_m)
          goto next_segment;

        factors.to_index(&min_m);
        factors.to_index(&max_m);

        for (int64_t m = max_m; m > min_m; m--)
        {
          if (prime < factors.lpf(m))
          {
            int64_t xn = (int64_t) fast_div(x2, factors.get_number(m));
            int64_t stop = xn - low;
            count += COUNT_POPCNT(start, stop);
            start = stop + 1;
            int64_t phi_xn = phi[b] + count;
            int64_t mu_m = factors.mu(m);
            s2_hard -= mu_m * phi_xn;
            mu_sum[b] -= mu_m;
          }
        }

        phi[b] += count_low_high;
        count_low_high -= cross_off(prime, low, high, next[b], sieve);
      }

      // For pi_sqrty <= b <= pi_sqrtz
      // Find all hard special leaves: n = primes[b] * primes[l]
      // which satisfy: low <= (x / n) < high
      for (; b <= max_b; b++)
      {
        int64_t prime = primes[b];
        T x2 = x / prime;
        int64_t x2_div_low = min(fast_div(x2, low), y);
        int64_t x2_div_high = min(fast_div(x2, high), y);
        int64_t l = pi[min(x2_div_low, z / prime)];
        int64_t min_hard_leaf = max3(x2_div_high, y / prime, prime);
        int64_t count = 0;
        int64_t start = 0;

        if (prime >= primes[l])
          goto next_segment;

        for (; primes[l] > min_hard_leaf; l--)
        {
          int64_t xn = (int64_t) fast_div(x2, primes[l]);
          int64_t stop = xn - low;
          count += COUNT_POPCNT(start, stop);
          start = stop + 1;
          int64_t phi_xn = phi[b] + count;
          s2_hard += phi_xn;
          mu_sum[b]++;
        }

        phi[b] += count_low_high;
        count_low_high -= cross_off(prime, low, high, next[b], sieve);
      }
    }
    else
    {
      // Calculate the contribution of the hard special leaves using
      // Tomás Oliveira's O(log(N)) special tree data structure
      // for counting the number of unsieved elements. This algorithm
      // runs fastest if there are many special leaves per segment.

      // allocate memory upon first usage
      counters.resize(segment_size);

      // Initialize special tree data structure from sieve
      cnt_finit(sieve, counters, segment_size);

      // For c + 1 <= b <= pi_sqrty
      // Find all special leaves: n = primes[b] * m
      // which satisfy: mu[m] != 0 && primes[b] < lpf[m] && low <= (x / n) < high
      for (int64_t end = min(pi_sqrty, max_b); b <= end; b++)
      {
        int64_t prime = primes[b];
        T x2 = x / prime;
        int64_t x2_div_low = min(fast_div(x2, low), y);
        int64_t x2_div_high = min(fast_div(x2, high), y);
        int64_t min_m = max(x2_div_high, y / prime);
        int64_t max_m = x2_div_low;

        if (prime >= max_m)
          goto next_segment;

        factors.to_index(&min_m);
        factors.to_index(&max_m);

        for (int64_t m = max_m; m > min_m; m--)
        {
          if (prime < factors.lpf(m))
          {
            int64_t xn = (int64_t) fast_div(x2, factors.get_number(m));
            int64_t count = cnt_query(counters, xn - low);
            int64_t phi_xn = phi[b] + count;
            int64_t mu_m = factors.mu(m);
            s2_hard -= mu_m * phi_xn;
            mu_sum[b] -= mu_m;
          }
        }

        phi[b] += cnt_query(counters, (high - 1) - low);
        cross_off(prime, low, high, next[b], sieve, counters);
      }

      // For pi_sqrty <= b <= pi_sqrtz
      // Find all hard special leaves: n = primes[b] * primes[l]
      // which satisfy: low <= (x / n) < high
      for (; b <= max_b; b++)
      {
        int64_t prime = primes[b];
        T x2 = x / prime;
        int64_t x2_div_low = min(fast_div(x2, low), y);
        int64_t x2_div_high = min(fast_div(x2, high), y);
        int64_t l = pi[min(x2_div_low, z / prime)];
        int64_t min_hard_leaf = max3(x2_div_high, y / prime, prime);

        if (prime >= primes[l])
          goto next_segment;

        for (; primes[l] > min_hard_leaf; l--)
        {
          int64_t xn = (int64_t) fast_div(x2, primes[l]);
          int64_t count = cnt_query(counters, xn - low);
          int64_t phi_xn = phi[b] + count;
          s2_hard += phi_xn;
          mu_sum[b]++;
        }

        phi[b] += cnt_query(counters, (high - 1) - low);
        cross_off(prime, low, high, next[b], sieve, counters);
      }
    }

    next_segment:;
  }

  return s2_hard;
}

/// Calculate the contribution of the hard special leaves which
/// require use of a sieve (to reduce the memory usage).
/// This is a parallel implementation with advanced load balancing.
/// As most special leaves tend to be in the first segments we
/// start off with a small segment size and few segments
/// per thread, after each iteration we dynamically increase
/// the segment size and the segments per thread.
///
template <typename T, typename P, typename F>
T S2_hard(T x,
          int64_t y,
          int64_t z,
          int64_t c,
          T s2_hard_approx,
          vector<P>& primes,
          FactorTable<F>& factors,
          int threads)
{
  threads = validate_threads(threads, z);

  T s2_hard = 0;
  int64_t low = 1;
  int64_t limit = z + 1;
  int64_t max_prime = z / isqrt(y);

  S2Status status;
  S2LoadBalancer loadBalancer(x, y, z, threads);
  int64_t segment_size = loadBalancer.get_min_segment_size();
  int64_t segments_per_thread = 1;

  PiTable pi(max_prime);
  vector<int64_t> phi_total(pi[isqrt(z)] + 1, 0);
  double alpha = get_alpha(x, y);

  while (low < limit)
  {
    int64_t segments = ceil_div(limit - low, segment_size);
    threads = in_between(1, threads, segments);
    segments_per_thread = in_between(1, segments_per_thread, ceil_div(segments, threads));

    aligned_vector<vector<int64_t> > phi(threads);
    aligned_vector<vector<int64_t> > mu_sum(threads);
    aligned_vector<double> timings(threads);

    #pragma omp parallel for num_threads(threads) reduction(+: s2_hard)
    for (int i = 0; i < threads; i++)
    {
      timings[i] = get_wtime();
      s2_hard += S2_hard_thread(x, y, z, c, segment_size, segments_per_thread, i,
          low, limit, alpha, factors, pi, primes, mu_sum[i], phi[i]);
      timings[i] = get_wtime() - timings[i];
    }

    // Once all threads have finished reconstruct and add the 
    // missing contribution of all special leaves. This must
    // be done in order as each thread (i) requires the sum of
    // the phi values from the previous threads.
    //
    for (int i = 0; i < threads; i++)
    {
      for (size_t j = 1; j < phi[i].size(); j++)
      {
        s2_hard += phi_total[j] * (T) mu_sum[i][j];
        phi_total[j] += phi[i][j];
      }
    }

    low += segments_per_thread * threads * segment_size;
    loadBalancer.update(low, threads, &segment_size, &segments_per_thread, timings);

    if (print_status())
      status.print(s2_hard, s2_hard_approx, loadBalancer.get_rsd());
  }

  return s2_hard;
}

} // namespace S2_hard
} // namespace

namespace primecount {

int64_t S2_hard(int64_t x,
                int64_t y,
                int64_t z,
                int64_t c,
                int64_t s2_hard_approx,
                int threads)
{
  print("");
  print("=== S2_hard(x, y) ===");
  print("Computation of the hard special leaves");
  print(x, y, c, threads);

  double time = get_wtime();
  FactorTable<uint16_t> factors(y);
  int64_t max_prime = z / isqrt(y);
  vector<int32_t> primes = generate_primes(max_prime);

  int64_t s2_hard = S2_hard::S2_hard((intfast64_t) x, y, z, c, (intfast64_t) s2_hard_approx, primes, factors, threads);

  print("S2_hard", s2_hard, time);
  return s2_hard;
}

#ifdef HAVE_INT128_T

int128_t S2_hard(int128_t x,
                 int64_t y,
                 int64_t z,
                 int64_t c,
                 int128_t s2_hard_approx,
                 int threads)
{
  print("");
  print("=== S2_hard(x, y) ===");
  print("Computation of the hard special leaves");
  print(x, y, c, threads);

  double time = get_wtime();
  int128_t s2_hard;

  // uses less memory
  if (y <= FactorTable<uint16_t>::max())
  {
    FactorTable<uint16_t> factors(y);
    int64_t max_prime = z / isqrt(y);
    vector<uint32_t> primes = generate_primes<uint32_t>(max_prime);

    s2_hard = S2_hard::S2_hard((intfast128_t) x, y, z, c, (intfast128_t) s2_hard_approx, primes, factors, threads);
  }
  else
  {
    FactorTable<uint32_t> factors(y);
    int64_t max_prime = z / isqrt(y);
    vector<int64_t> primes = generate_primes<int64_t>(max_prime);

    s2_hard = S2_hard::S2_hard((intfast128_t) x, y, z, c, (intfast128_t) s2_hard_approx, primes, factors, threads);
  }

  print("S2_hard", s2_hard, time);
  return s2_hard;
}

#endif

} // namespace primecount
