//===-- int32.c - C166 32-bit integer builtins ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// C166 has a 16-bit C int and a 32-bit C long. LLVM's `si`
// libcalls still describe their fixed IR width, so these entry points operate
// on C long values and use the ordinary C166 calling convention.
//
// Keep every operation below word based.  Expressing the algorithms with
// 32-bit multiply, divide, remainder, or variable shifts would make the C166
// backend lower the builtin implementation to a call to itself.
//
//===----------------------------------------------------------------------===//

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
  if (lhs.words.high != rhs.words.high)
    return lhs.words.high > rhs.words.high;
  return lhs.words.low >= rhs.words.low;
}

// Unlike the fixed-width arithmetic entry points, the GCC/LLVM bit-counting
// ABI returns C int in a single 16-bit register.
COMPILER_RT_ABI int __clzsi2(si_int value) {
  c166_u32 bits = {.all = (su_int)value};
  uint16_t word;
  int count;

  if (bits.words.high != 0) {
    word = bits.words.high;
    count = 0;
  } else {
    word = bits.words.low;
    count = 16;
  }

  if (word == 0)
    return 32;
  while ((word & UINT16_C(0x8000)) == 0) {
    word = (uint16_t)(word << 1);
    ++count;
  }
  return count;
}

static inline ALWAYS_INLINE c166_div_result c166_udivmod(c166_u32 dividend,
                                                         c166_u32 divisor) {
  c166_div_result result;
  result.quotient = c166_zero();
  result.remainder = c166_zero();

  // Division by zero is undefined at the C boundary.  Use the conventional
  // all-ones quotient and unchanged remainder so the helper remains finite
  // and deterministic if it is nevertheless called directly.
  if (divisor.words.low == 0 && divisor.words.high == 0) {
    result.quotient = c166_ones();
    result.remainder = dividend;
    return result;
  }

  for (uint16_t bit_index = 0; bit_index != 32; ++bit_index) {
    uint16_t next_bit = (uint16_t)(dividend.words.high >> 15);
    dividend = c166_shl1(dividend);

    // The extra carry is the 33rd remainder bit.  If it is set, the logical
    // shifted remainder is necessarily at least the 32-bit divisor.  The
    // subtraction then fits back into 32 bits because the old remainder was
    // strictly less than the divisor.
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

COMPILER_RT_ABI si_int __mulsi3(si_int lhs, si_int rhs) {
  c166_u32 multiplicand = {.all = (su_int)lhs};
  c166_u32 multiplier = {.all = (su_int)rhs};
  c166_u32 result = c166_zero();

  for (uint16_t bit_index = 0; bit_index != 32; ++bit_index) {
    if (multiplier.words.low & 1)
      result = c166_add(result, multiplicand);
    multiplicand = c166_shl1(multiplicand);
    multiplier = c166_lshr1(multiplier);
  }
  return (si_int)result.all;
}

COMPILER_RT_ABI si_int __ashlsi3(si_int value, int amount) {
  c166_u32 result = {.all = (su_int)value};
  uint16_t count = (uint16_t)amount;
  if (count >= 32)
    return 0;
  while (count-- != 0)
    result = c166_shl1(result);
  return (si_int)result.all;
}

COMPILER_RT_ABI su_int __lshrsi3(su_int value, int amount) {
  c166_u32 result = {.all = value};
  uint16_t count = (uint16_t)amount;
  if (count >= 32)
    return 0;
  while (count-- != 0)
    result = c166_lshr1(result);
  return result.all;
}

COMPILER_RT_ABI si_int __ashrsi3(si_int value, int amount) {
  c166_u32 result = {.all = (su_int)value};
  uint16_t count = (uint16_t)amount;
  if (count >= 32)
    count = 31;
  while (count-- != 0)
    result = c166_ashr1(result);
  return (si_int)result.all;
}

COMPILER_RT_ABI su_int __udivsi3(su_int dividend, su_int divisor) {
  c166_u32 lhs = {.all = dividend};
  c166_u32 rhs = {.all = divisor};
  return c166_udivmod(lhs, rhs).quotient.all;
}

COMPILER_RT_ABI su_int __umodsi3(su_int dividend, su_int divisor) {
  c166_u32 lhs = {.all = dividend};
  c166_u32 rhs = {.all = divisor};
  return c166_udivmod(lhs, rhs).remainder.all;
}

COMPILER_RT_ABI si_int __divsi3(si_int dividend, si_int divisor) {
  c166_u32 lhs = {.all = (su_int)dividend};
  c166_u32 rhs = {.all = (su_int)divisor};
  // Keep the sign as a word mask.  Canonicalizing `high >> 15` into a signed
  // 32-bit comparison makes this leaf runtime depend on the very long-compare
  // lowering it is meant to support and used to corrupt positive division.
  uint16_t lhs_negative = (uint16_t)(lhs.words.high & UINT16_C(0x8000));
  uint16_t rhs_negative = (uint16_t)(rhs.words.high & UINT16_C(0x8000));
  if (lhs_negative)
    lhs = c166_negate(lhs);
  if (rhs_negative)
    rhs = c166_negate(rhs);

  c166_u32 quotient = c166_udivmod(lhs, rhs).quotient;
  if (lhs_negative != rhs_negative)
    quotient = c166_negate(quotient);
  return (si_int)quotient.all;
}

COMPILER_RT_ABI si_int __modsi3(si_int dividend, si_int divisor) {
  c166_u32 lhs = {.all = (su_int)dividend};
  c166_u32 rhs = {.all = (su_int)divisor};
  uint16_t lhs_negative = (uint16_t)(lhs.words.high & UINT16_C(0x8000));
  if (lhs_negative)
    lhs = c166_negate(lhs);
  if (rhs.words.high & UINT16_C(0x8000))
    rhs = c166_negate(rhs);

  c166_u32 remainder = c166_udivmod(lhs, rhs).remainder;
  if (lhs_negative)
    remainder = c166_negate(remainder);
  return (si_int)remainder.all;
}
