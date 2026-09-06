//===-- fp32_impl.h - C166 binary32 builtin support --------------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef COMPILER_RT_LIB_BUILTINS_C166_FP32_IMPL_H
#define COMPILER_RT_LIB_BUILTINS_C166_FP32_IMPL_H

#include "int32_impl.h"

typedef struct {
  uint16_t low;
  uint16_t high;
} c166_sf_limbs;

rep_t __c166_sf_round_pack_finite(uint16_t low, uint16_t high, int exponent,
                                  uint16_t sign);

static inline ALWAYS_INLINE c166_sf_limbs c166_sf_unpack(rep_t value) {
  c166_sf_limbs result = {(uint16_t)value, (uint16_t)(value >> 16)};
  return result;
}

static inline ALWAYS_INLINE rep_t c166_sf_pack(c166_sf_limbs value) {
  return (rep_t)value.high << 16 | value.low;
}

static inline rep_t c166_float_unsigned(su_int value) {
  if (value == 0)
    return 0;

  c166_u32 magnitude = {.all = value};
  uint16_t normalized_high;
  uint16_t normalized_low;
  unsigned int exponent;

  if (magnitude.words.high != 0) {
    unsigned int shift = __builtin_clz((unsigned int)magnitude.words.high);
    exponent = 31 - shift;
    if (shift == 0) {
      normalized_high = magnitude.words.high;
      normalized_low = magnitude.words.low;
    } else {
      normalized_high = (uint16_t)(magnitude.words.high << shift) |
                        (uint16_t)(magnitude.words.low >> (16 - shift));
      normalized_low = (uint16_t)(magnitude.words.low << shift);
    }
  } else {
    unsigned int shift = __builtin_clz((unsigned int)magnitude.words.low);
    exponent = 15 - shift;
    normalized_high = (uint16_t)(magnitude.words.low << shift);
    normalized_low = 0;
  }

  c166_sf_limbs result = {
      (uint16_t)(normalized_low >> 8) | (uint16_t)(normalized_high << 8),
      (uint16_t)((normalized_high >> 8) & UINT16_C(0x007f)) |
          (uint16_t)((exponent + exponentBias) << 7)};

  uint16_t round = normalized_low & UINT16_C(0x00ff);
  if (round > UINT16_C(0x0080) ||
      (round == UINT16_C(0x0080) && (result.low & 1) != 0)) {
    if (++result.low == 0)
      ++result.high;
  }
  return c166_sf_pack(result);
}

#endif
