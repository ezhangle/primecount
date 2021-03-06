///
/// @file  help.cpp
///
/// Copyright (C) 2015 Kim Walisch, <kim.walisch@gmail.com>
///
/// This file is distributed under the BSD License. See the COPYING
/// file in the top level directory.
///

#include <primecount.hpp>

#include <iostream>
#include <cstdlib>
#include <string>

using namespace std;

namespace {

const string helpMenu(
  "Usage: primecount x [OPTION]...\n"
  "Count the primes below x <= 10^27 using the prime counting function,\n"
  "by default the Deleglise-Rivat algorithm (-d) is used.\n"
  "\n"
  "Options:\n"
  "\n"
  "  -d,    --deleglise_rivat  Count primes using Deleglise-Rivat algorithm\n"
  "         --legendre         Count primes using Legendre's formula\n"
  "         --lehmer           Count primes using Lehmer's formula\n"
  "  -l,    --lmo              Count primes using Lagarias-Miller-Odlyzko\n"
  "  -m,    --meissel          Count primes using Meissel's formula\n"
  "         --Li               Approximate pi(x) using the logarithmic integral\n"
  "         --Li_inverse       Approximate the nth prime using Li^-1(x)\n"
  "  -n,    --nthprime         Calculate the nth prime\n"
  "  -p,    --primesieve       Count primes using the sieve of Eratosthenes\n"
  "  -s,    --status           Print status info during computation\n"
  "         --test             Run various correctness tests and exit\n"
  "         --time             Print the time elapsed in seconds\n"
  "  -t<N>, --threads=<N>      Set the number of threads, 1 <= N <= CPU cores\n"
  "  -v,    --version          Print version and license information\n"
  "  -h,    --help             Print this help menu\n"
  "\n"
  "Advanced Deleglise-Rivat options:\n"
  "\n"
  "  -a<N>, --alpha=<N>        Tuning factor, 1 <= alpha <= x^(1/6)\n"
  "         --p2               Only compute the 2nd partial sieve function\n"
  "         --s1               Only compute the ordinary leaves\n"
  "         --s2_trivial       Only compute the trivial special leaves\n"
  "         --s2_easy          Only compute the easy special leaves\n"
  "         --s2_hard          Only compute the hard special leaves\n"
  "\n"
  "Examples:\n"
  "\n"
  "  primecount 10**13\n"
  "  primecount 10**13 --nthprime --threads=4"
);

const string versionInfo(
  "primecount " PRIMECOUNT_VERSION ", <https://github.com/kimwalisch/primecount>\n"
  "Copyright (C) 2015 Kim Walisch\n"
  "BSD 2-Clause License <http://opensource.org/licenses/BSD-2-Clause>"
);

} // end namespace

namespace primecount {

void help()
{
  cout << helpMenu << endl;
  exit(1);
}

void version()
{
  cout << versionInfo << endl;
  exit(1);
}

} // namespace primecount
