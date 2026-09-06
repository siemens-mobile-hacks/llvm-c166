//===-- fixdfsi.c - C166 binary64 to signed integer conversion ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"

COMPILER_RT_ABI si_int __fixdfsi(fp_t value) {
  rep_t representation = toRep(value);
  uint16_t top = representation >> 48;
  bool negative = (top & UINT16_C(0x8000)) != 0;
  unsigned int biased_exponent = (top >> 4) & 0x7ff;

  if (biased_exponent < 1023)
    return 0;

  unsigned int exponent = biased_exponent - 1023;
  if (exponent >= 32)
    return negative ? (si_int)UINT32_C(0x80000000)
                    : (si_int)UINT32_C(0x7fffffff);

  uint16_t first = UINT16_C(0x10) | (top & UINT16_C(0x000f));
  uint16_t second = representation >> 32;
  uint16_t high = 0;
  uint16_t low;

  if (exponent <= 4) {
    low = first >> (4 - exponent);
  } else if (exponent < 20) {
    unsigned int shift = 20 - exponent;
    high = first >> shift;
    low = (uint16_t)(first << (16 - shift)) | (second >> shift);
  } else if (exponent == 20) {
    high = first;
    low = second;
  } else {
    unsigned int shift = exponent - 20;
    high = (uint16_t)(first << shift) | (second >> (16 - shift));
    uint16_t third = representation >> 16;
    low = (uint16_t)(second << shift) | (third >> (16 - shift));
  }

  su_int magnitude = (su_int)high << 16 | low;
  if (negative)
    magnitude = -magnitude;
  return (si_int)magnitude;
}
