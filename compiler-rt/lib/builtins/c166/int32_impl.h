//===-- int32_impl.h - C166 32-bit integer builtin support -------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef COMPILER_RT_LIB_BUILTINS_C166_INT32_IMPL_H
#define COMPILER_RT_LIB_BUILTINS_C166_INT32_IMPL_H

#include "../int_lib.h"

typedef struct {
  uint16_t low;
  uint16_t high;
} c166_words;

typedef union {
  su_int all;
  c166_words words;
} c166_u32;

typedef struct {
  c166_u32 quotient;
  c166_u32 remainder;
} c166_div_result;

static inline ALWAYS_INLINE c166_u32 c166_zero(void) {
  c166_u32 value;
  value.words.low = 0;
  value.words.high = 0;
  return value;
}

static inline ALWAYS_INLINE c166_u32 c166_ones(void) {
  c166_u32 value;
  value.words.low = UINT16_MAX;
  value.words.high = UINT16_MAX;
  return value;
}

static inline ALWAYS_INLINE c166_u32 c166_add(c166_u32 lhs, c166_u32 rhs) {
  c166_u32 result;
  result.words.low = (uint16_t)(lhs.words.low + rhs.words.low);
  uint16_t carry = result.words.low < lhs.words.low;
  result.words.high = (uint16_t)(lhs.words.high + rhs.words.high + carry);
  return result;
}

static inline ALWAYS_INLINE c166_u32 c166_sub(c166_u32 lhs, c166_u32 rhs) {
  c166_u32 result;
  uint16_t borrow = lhs.words.low < rhs.words.low;
  result.words.low = (uint16_t)(lhs.words.low - rhs.words.low);
  result.words.high = (uint16_t)(lhs.words.high - rhs.words.high - borrow);
  return result;
}

static inline ALWAYS_INLINE c166_u32 c166_negate(c166_u32 value) {
  c166_u32 result;
  result.words.low = (uint16_t)~value.words.low;
  result.words.high = (uint16_t)~value.words.high;
  c166_u32 one = c166_zero();
  one.words.low = 1;
  return c166_add(result, one);
}

static inline ALWAYS_INLINE c166_u32 c166_shl1(c166_u32 value) {
  uint16_t carry = (uint16_t)(value.words.low >> 15);
  value.words.low = (uint16_t)(value.words.low << 1);
  value.words.high = (uint16_t)((value.words.high << 1) | carry);
  return value;
}

static inline ALWAYS_INLINE c166_u32 c166_lshr1(c166_u32 value) {
  uint16_t carry = (uint16_t)(value.words.high << 15);
  value.words.high = (uint16_t)(value.words.high >> 1);
  value.words.low = (uint16_t)((value.words.low >> 1) | carry);
  return value;
}

static inline ALWAYS_INLINE c166_u32 c166_ashr1(c166_u32 value) {
  uint16_t carry = (uint16_t)(value.words.high << 15);
  int16_t signed_high = (int16_t)value.words.high;
  value.words.high = (uint16_t)(signed_high >> 1);
  value.words.low = (uint16_t)((value.words.low >> 1) | carry);
  return value;
}

static inline ALWAYS_INLINE int c166_unsigned_ge(c166_u32 lhs, c166_u32 rhs) {
  return lhs.all >= rhs.all;
}

static inline ALWAYS_INLINE c166_div_result c166_udivmod(c166_u32 dividend,
                                                         c166_u32 divisor) {
  c166_div_result result;
  result.quotient = c166_zero();
  result.remainder = c166_zero();

  if (divisor.words.low == 0 && divisor.words.high == 0) {
    result.quotient = c166_ones();
    result.remainder = dividend;
    return result;
  }

  for (uint16_t bit_index = 0; bit_index != 32; ++bit_index) {
    uint16_t next_bit = (uint16_t)(dividend.words.high >> 15);
    dividend = c166_shl1(dividend);

    uint16_t remainder_carry = (uint16_t)(result.remainder.words.high >> 15);
    result.remainder = c166_shl1(result.remainder);
    result.remainder.words.low |= next_bit;
    result.quotient = c166_shl1(result.quotient);

    if (remainder_carry || c166_unsigned_ge(result.remainder, divisor)) {
      result.remainder = c166_sub(result.remainder, divisor);
      result.quotient.words.low |= 1;
    }
  }

  return result;
}

#endif
