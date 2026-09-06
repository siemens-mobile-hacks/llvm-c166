//===-- fixsfsi.c - C166 binary32-to-signed-integer builtin --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define SINGLE_PRECISION
#include "../fp_lib.h"
#include "fp32_impl.h"
typedef si_int fixint_t;
typedef su_int fixuint_t;

COMPILER_RT_ABI si_int __fixsfsi(rep_t value) {
  const fixuint_t fixint_sign_bit = (fixuint_t)1 << 31;
  const fixuint_t fixint_max = fixint_sign_bit - 1;
  const rep_t absolute_value = value & absMask;
  const bool negative = (value & signBit) != 0;
  const int exponent = (absolute_value >> significandBits) - exponentBias;
  const rep_t significand = (absolute_value & significandMask) | implicitBit;

  if (exponent < 0)
    return 0;
  if ((unsigned int)exponent >= 32)
    return (si_int)(negative ? fixint_sign_bit : fixint_max);

  fixuint_t magnitude;
  if (exponent < significandBits) {
    magnitude = significand >> (significandBits - exponent);
  } else {
    unsigned int shift = exponent - significandBits;
    if (shift == 0) {
      magnitude = significand;
    } else {
      c166_u32 significand_words = {.all = (fixuint_t)significand};
      c166_u32 magnitude_words;
      magnitude_words.words.high =
          (uint16_t)(significand_words.words.high << shift) |
          (uint16_t)(significand_words.words.low >> (16 - shift));
      magnitude_words.words.low = significand_words.words.low << shift;
      magnitude = magnitude_words.all;
    }
  }

  if (negative)
    magnitude = (fixuint_t)0 - magnitude;
  return (si_int)magnitude;
}
