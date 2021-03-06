///
/// @file    popcount.hpp
/// @brief   Count the number of 1 bits inside a 64-bit word.
///          If HAVE_POPCNT is defined then popcount64(x) uses the
///          POPCNT instruction (requires SSE4.2 for x86 & x64).
///
/// Copyright (C) 2014 Kim Walisch, <kim.walisch@gmail.com>
///
/// This file is distributed under the BSD License. See the COPYING
/// file in the top level directory.
///

#ifndef POPCOUNT_HPP
#define POPCOUNT_HPP

#if !defined(__STDC_CONSTANT_MACROS)
  #define __STDC_CONSTANT_MACROS
#endif

#include <stdint.h>

#if defined(HAVE_POPCNT)

#if defined(_MSC_VER) && defined(_WIN64)

#include <nmmintrin.h>

inline uint64_t popcount64(uint64_t x)
{
  return _mm_popcnt_u64(x);
}

#elif defined(_MSC_VER) && defined(_WIN32)

#include <nmmintrin.h>

inline uint64_t popcount64(uint64_t x)
{
  return _mm_popcnt_u32((uint32_t) x) + 
         _mm_popcnt_u32((uint32_t)(x >> 32));
}

#elif defined(HAVE___BUILTIN_POPCOUNT) && defined(__i386__)

inline uint64_t popcount64(uint64_t x)
{
  return __builtin_popcount((uint32_t) x) +
         __builtin_popcount((uint32_t)(x >> 32));
}

#elif defined(HAVE___BUILTIN_POPCOUNTLL)

inline uint64_t popcount64(uint64_t x)
{
  return __builtin_popcountll(x);
}

#else /* Error */

#if defined(_MSC_VER)
  #pragma error( "POPCNT not supported, remove \"/D HAVE_POPCNT\" from Makefile.msvc" )
#else
  #error "POPCNT not supported, use --disable-popcnt"
#endif

#endif /* HAVE_POPCNT */

#elif !defined(HAVE_POPCNT)

/// Count the number of 1 bits in x.
/// This implementation uses only 12 arithmetic operations.
/// http://en.wikipedia.org/wiki/Hamming_weight#Efficient_implementation
/// 
inline uint64_t popcount64(uint64_t x)
{
  const uint64_t m1  = UINT64_C(0x5555555555555555);
  const uint64_t m2  = UINT64_C(0x3333333333333333);
  const uint64_t m4  = UINT64_C(0x0f0f0f0f0f0f0f0f);
  const uint64_t h01 = UINT64_C(0x0101010101010101);

  x -= (x >> 1) & m1;
  x = (x & m2) + ((x >> 2) & m2);
  x = (x + (x >> 4)) & m4;
  return (x * h01) >> 56;
}

#endif

#endif /* POPCOUNT64_HPP */
