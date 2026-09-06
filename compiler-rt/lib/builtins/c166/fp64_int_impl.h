//===-- fp64_int_impl.h - C166 integer to binary64 support ------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef COMPILER_RT_LIB_BUILTINS_C166_FP64_INT_IMPL_H
#define COMPILER_RT_LIB_BUILTINS_C166_FP64_INT_IMPL_H

#include "int32_impl.h"

typedef struct {
  uint16_t top;
  uint16_t second;
  uint16_t third;
} c166_df_int_encoding;

static inline ALWAYS_INLINE c166_df_int_encoding
c166_df_encode_int(su_int value, uint16_t sign) {
  c166_df_int_encoding result = {0, 0, 0};
  if (value == 0)
    return result;

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
      normalized_high =
          (uint16_t)(magnitude.words.high << shift) |
          (uint16_t)(magnitude.words.low >> (16 - shift));
      normalized_low = (uint16_t)(magnitude.words.low << shift);
    }
  } else {
    unsigned int shift = __builtin_clz((unsigned int)magnitude.words.low);
    exponent = 15 - shift;
    normalized_high = (uint16_t)(magnitude.words.low << shift);
    normalized_low = 0;
  }

  result.top = sign | (uint16_t)((exponent + 1023) << 4) |
               (uint16_t)((normalized_high >> 11) & UINT16_C(0x000f));
  result.second = (uint16_t)(normalized_high << 5) |
                  (uint16_t)(normalized_low >> 11);
  result.third = (uint16_t)(normalized_low << 5);
  return result;
}

static inline ALWAYS_INLINE rep_t c166_df_pack_int(c166_df_int_encoding value) {
  return (rep_t)value.top << 48 | (rep_t)value.second << 32 |
         (rep_t)value.third << 16;
}

#endif
