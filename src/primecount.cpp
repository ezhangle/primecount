///
/// @file  primecount.cpp
///
/// Copyright (C) 2013 Kim Walisch, <kim.walisch@gmail.com>
///
/// This file is distributed under the BSD License. See the COPYING
/// file in the top level directory.
///

#include "cmdoptions.h"

#include <primecount.h>
#include <iostream>
#include <stdint.h>

using namespace primecount;

int main (int argc, char* argv[])
{
  PrimeCountOptions pco = parseOptions(argc, argv);
  int64_t res = 0;

  switch (pco.option)
  {      
    case OPTION_PRIMESIEVE: res = pi_primesieve(pco.x, pco.threads); break;
    case OPTION_LEGENDRE:   res = pi_legendre  (pco.x, pco.threads); break;
    case OPTION_MEISSEL:    res = pi_meissel   (pco.x, pco.threads); break;
    case OPTION_LEHMER:     res = pi_lehmer    (pco.x, pco.threads); break;
    case OPTION_LI:         res = Li           (pco.x); break;
    case OPTION_LIINV:      res = Li_inverse   (pco.x); break;
    case OPTION_NTHPRIME:   res = nth_prime    (pco.x, pco.threads); break;
  }

  std::cout << res << std::endl;
  return 0;
}