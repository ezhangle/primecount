///
/// @file  PhiCache.h
///
/// Copyright (C) 2013 Kim Walisch, <kim.walisch@gmail.com>
///
/// This file is distributed under the BSD License. See the COPYING
/// file in the top level directory.
///

#ifndef PHICACHE_H
#define PHICACHE_H

#include "PhiTiny.h"
#include "pi_bsearch.h"
#include "imath.h"

#include <stdint.h>
#include <vector>
#include <limits>
#include <cstddef>
#include <cassert>

/// Keep the cache size below CACHE_BYTES_LIMIT per thread
#define CACHE_BYTES_LIMIT (16 << 20)

/// Cache phi(x, a) results if a <= CACHE_A_LIMIT
#define CACHE_A_LIMIT 500

/// Avoid slow 64-bit division if possible
#define FAST_DIV(x, y) ((x <= std::numeric_limits<uint32_t>::max()) \
    ? static_cast<uint32_t>(x) / (y) : (x) / (y))

namespace primecount {

/// This class calculates phi(x, a) using the recursive formula:
/// phi(x, a) = phi(x, a - 1) - phi(x / primes_[a], a - 1).
/// This implementation is based on an algorithm from Tomas Oliveira e
/// Silva [1]. I have added a cache to my implementation in which
/// results of phi(x, a) are stored if x < 2^16 and a <= 500.
/// The cache speeds up the calculations by at least 3 orders of
/// magnitude near 10^15.
///
/// [1] Tomas Oliveira e Silva, "Computing pi(x): the combinatorial method",
///     Revista do DETUA, vol. 4, no. 6, pp. 759-768, March 2006
///
class PhiCache {
public:
  /// @param primes  A vector of primes with:
  ///                primes[0] = 0, primes[1] = 2, primes[3] ...
  /// 
  PhiCache(const std::vector<int32_t>& primes, const PhiTiny& phiTiny) :
    primes_(primes),
    phiTiny_(phiTiny),
    bytes_(0)
  {
    assert(primes_[0] == 0);
    std::size_t max_size = CACHE_A_LIMIT + 1;
    cache_.resize(std::min(primes.size(), max_size));
  }

  /// Partial sieve function (a.k.a. Legendre-sum).
  /// phi(x, a) counts the numbers <= x that are not divisible
  /// by any of the first a primes.
  ///
  int64_t phi(int64_t x, int64_t a)
  {
    if (x < 1) return 0;
    if (a > x) return 1;
    if (a < 1) return x;

    if (primes_.at(a) >= x)
      return 1;

    return phi(x, a, 1);
  }

  /// Calculate phi(x, a) using the recursive formula:
  /// phi(x, a) = phi(x, a - 1) - phi(x / primes_[a], a - 1)
  ///
  int64_t phi(int64_t x, int64_t a, int sign)
  {
    if (is_cached(x, a))
      return cache_[a][x] * sign;

    int64_t sum;

    if (x < primes_[a])
      sum = sign;
    else if (phiTiny_.is_cached(a))
      sum = phiTiny_.phi(x, a) * sign;
    else if (is_phi_bsearch(x, a))
      sum = phi_bsearch(x, a) * sign;
    else
    {
      int64_t iters = pi_bsearch(primes_, a, isqrt(x));
      int64_t c = (iters > 6) ? 6 : iters;
      sum = (a - iters) * -sign;
      sum += phiTiny_.phi(x, c) * sign;

      for (int64_t a2 = c; a2 < iters; a2++)
        sum += phi(FAST_DIV(x, primes_[a2 + 1]), a2, -sign);
    }

    if (write_to_cache(x, a))
      cache_[a][x] = static_cast<uint16_t>(sum * sign);

    return sum;
  }
private:
  /// Cache for phi(x, a) results
  std::vector<std::vector<uint16_t> > cache_;
  const std::vector<int32_t>& primes_;
  const PhiTiny& phiTiny_;
  int64_t bytes_;

  /// Binary search phi(x, a)
  int64_t phi_bsearch(int64_t x, int64_t a) const
  {
    int64_t pix = pi_bsearch(primes_, x);
    return pix - a + 1;
  }

  bool is_phi_bsearch(int64_t x, int64_t a) const
  {
    return x <= primes_.back() && x < isquare(primes_[a + 1]);
  }

  int64_t cache_size(int64_t a) const
  {
    return static_cast<int64_t>(cache_[a].size());
  }

  bool is_cached(int64_t x, int64_t a) const
  {
    return a <= CACHE_A_LIMIT && x < cache_size(a) &&
        cache_[a][x] != 0;
  }

  bool write_to_cache(int64_t x, int64_t a)
  {
    if (a > CACHE_A_LIMIT || x > std::numeric_limits<uint16_t>::max())
      return false;
    // resize and initialize cache_ if necessary
    if (x >= cache_size(a))
    {
      if (bytes_ > CACHE_BYTES_LIMIT)
        return false;
      bytes_ += (x + 1 - cache_[a].size()) * 2;
      cache_[a].resize(x + 1, 0);
    }
    return true;
  }
};

} // namespace primecount

#endif