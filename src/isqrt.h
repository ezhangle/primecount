///
/// @file  isqrt.h
///
/// Copyright (C) 2013 Kim Walisch, <kim.walisch@gmail.com>
///
/// This file is distributed under the BSD License. See the COPYING
/// file in the top level directory.
///

#ifndef ISQRT_PRIMECOUNT_H
#define ISQRT_PRIMECOUNT_H

#include <stdint.h>
#include <cmath>

namespace primecount {

inline int32_t isqrt(int64_t x)
{
  int64_t root = static_cast<int64_t>(std::sqrt(static_cast<double>(x)));
  // correct rounding errors
  while (root * root > x)
    root--;
  while ((root + 1) * (root + 1) <= x)
    root++;
  return static_cast<int32_t>(root);
}

inline int32_t isqrt3(int64_t x)
{
  int64_t root = static_cast<int64_t>(std::pow(static_cast<double>(x), 1.0 / 3.0));
  // correct rounding errors
  while (root * root * root > x)
    root--;
  while ((root + 1) * (root + 1) * (root + 1) <= x)
    root++;
  return static_cast<int32_t>(root);
}

inline int32_t isqrt4(int64_t x)
{
  int64_t root = static_cast<int64_t>(std::pow(static_cast<double>(x), 1.0 / 4.0));
  // correct rounding errors
  while (root * root * root * root > x)
    root--;
  while ((root + 1) * (root + 1) * (root + 1) * (root + 1) <= x)
    root++;
  return static_cast<int32_t>(root);
}

} // namespace primecount

#endif