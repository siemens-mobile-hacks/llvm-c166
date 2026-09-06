//===-- addsf3.c - C166 binary32 addition builtin ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define SINGLE_PRECISION
#include "../fp_lib.h"
#include "fp32_impl.h"

static inline ALWAYS_INLINE void c166_sf_shift_left_one(c166_sf_limbs *value) {
  uint16_t carry = value->low >> 15;
  value->low <<= 1;
  value->high = (uint16_t)(value->high << 1) | carry;
}

static inline ALWAYS_INLINE void
c166_sf_shift_left_three(c166_sf_limbs *value) {
  value->high = (uint16_t)(value->high << 3) | (uint16_t)(value->low >> 13);
  value->low <<= 3;
}

static unsigned int c166_sf_normalize(c166_sf_limbs *value, uint16_t top_bit) {
  unsigned int shift = 0;
  while ((value->high & top_bit) == 0) {
    c166_sf_shift_left_one(value);
    ++shift;
  }
  return shift;
}

static inline ALWAYS_INLINE void
c166_sf_shift_right_sticky(c166_sf_limbs *value, unsigned int count) {
  bool sticky = false;
  if (count >= 32) {
    sticky = (value->low | value->high) != 0;
    value->low = 0;
    value->high = 0;
  } else if (count != 0) {
    // Count upward from -count so the update and zero test can be combined.
    count = 0 - count;
    do {
      sticky |= (value->low & 1) != 0;
      value->low = (value->low >> 1) | (value->high << 15);
      value->high >>= 1;
    } while (++count != 0);
  }
  value->low |= sticky;
}

static inline ALWAYS_INLINE int c166_sf_compare(const c166_sf_limbs *left,
                                                const c166_sf_limbs *right) {
  if (left->high != right->high)
    return left->high > right->high ? 1 : -1;
  if (left->low != right->low)
    return left->low > right->low ? 1 : -1;
  return 0;
}

static inline ALWAYS_INLINE void c166_sf_add(c166_sf_limbs *left,
                                             const c166_sf_limbs *right) {
  uint16_t low = left->low + right->low;
  left->high = left->high + right->high + (low < left->low);
  left->low = low;
}

static inline ALWAYS_INLINE void c166_sf_sub(c166_sf_limbs *left,
                                             const c166_sf_limbs *right) {
  uint16_t low = left->low - right->low;
  left->high = left->high - right->high - (left->low < right->low);
  left->low = low;
}

COMPILER_RT_ABI rep_t __c166_addsubsf3(rep_t left, rep_t right) {
  const uint16_t sign_bit = UINT16_C(0x8000);
  const uint16_t fraction_mask = UINT16_C(0x007f);
  const uint16_t implicit_bit = UINT16_C(0x0080);
  // The public entry points tail-dispatch here with the operation in the
  // caller-saved R10 register.
  register uint16_t right_sign __asm__("r10");
  __asm__("" : "=r"(right_sign));

  c166_sf_limbs a = c166_sf_unpack(left);
  c166_sf_limbs b = c166_sf_unpack(right);
  uint16_t a_sign = a.high & sign_bit;
  uint16_t b_input_sign = b.high & sign_bit;
  uint16_t b_sign = b_input_sign ^ right_sign;
  a.high &= ~sign_bit;
  b.high &= ~sign_bit;

  int a_input_exponent = a.high >> 7;
  int b_input_exponent = b.high >> 7;
  // Shifting discards the sign and exponent without changing whether the
  // fraction is zero.
  uint16_t a_fraction = a.low | (uint16_t)(a.high << 9);
  uint16_t b_fraction = b.low | (uint16_t)(b.high << 9);
  uint16_t a_zero_bits = a_input_exponent | a_fraction;
  uint16_t b_zero_bits = b_input_exponent | b_fraction;

  if (a_input_exponent == 0xff) {
    if (a_fraction != 0) {
      a.high |= a_sign | UINT16_C(0x0040);
      return c166_sf_pack(a);
    }
    if (b_input_exponent == 0xff) {
      if (b_fraction != 0) {
        b.high |= b_input_sign | UINT16_C(0x0040);
        return c166_sf_pack(b);
      }
      if (a_sign != b_sign)
        return UINT32_C(0x7fc00000);
    }
    a.high |= a_sign;
    return c166_sf_pack(a);
  }
  if (b_input_exponent == 0xff) {
    if (b_fraction != 0) {
      b.high |= b_input_sign | UINT16_C(0x0040);
      return c166_sf_pack(b);
    }
    b.high |= b_sign;
    return c166_sf_pack(b);
  }

  if (b_zero_bits == 0) {
    if (a_zero_bits == 0) {
      b_sign &= a_sign;
      b.high |= b_sign;
      return c166_sf_pack(b);
    }
    a.high |= a_sign;
    return c166_sf_pack(a);
  }
  if (a_zero_bits == 0) {
    b.high |= b_sign;
    return c166_sf_pack(b);
  }

  if (c166_sf_compare(&b, &a) > 0) {
    c166_sf_limbs temporary = a;
    a = b;
    b = temporary;
    uint16_t temporary_sign = a_sign;
    a_sign = b_sign;
    b_sign = temporary_sign;
  }

  int a_exponent = a.high >> 7;
  int b_exponent = b.high >> 7;

  a.high &= fraction_mask;
  b.high &= fraction_mask;
  if (a_exponent == 0)
    a_exponent = 1 - c166_sf_normalize(&a, implicit_bit);
  if (b_exponent == 0)
    b_exponent = 1 - c166_sf_normalize(&b, implicit_bit);
  a.high |= implicit_bit;
  b.high |= implicit_bit;
  c166_sf_shift_left_three(&a);
  c166_sf_shift_left_three(&b);

  unsigned int align = (unsigned int)(a_exponent - b_exponent);
  if (align != 0)
    c166_sf_shift_right_sticky(&b, align);

  if (a_sign != b_sign) {
    c166_sf_sub(&a, &b);
    if ((a.low | a.high) == 0)
      return 0;
    if ((a.high & (implicit_bit << 3)) == 0) {
      unsigned int shift = c166_sf_normalize(&a, implicit_bit << 3);
      a_exponent -= shift;
    }
  } else {
    c166_sf_add(&a, &b);
    if ((a.high & (implicit_bit << 4)) != 0) {
      c166_sf_shift_right_sticky(&a, 1);
      ++a_exponent;
    }
  }

  if (a_exponent >= 0xff)
    return c166_sf_pack(
        (c166_sf_limbs){0, (uint16_t)(a_sign | UINT16_C(0x7f80))});
  if (a_exponent <= -31)
    return (rep_t)a_sign << 16;
  return __c166_sf_round_pack_finite(a.low, a.high, a_exponent, a_sign);
}

COMPILER_RT_ABI rep_t __addsf3(rep_t left, rep_t right) {
  register uint16_t right_sign __asm__("r10") = 0;
  __asm__ volatile("" : : "r"(right_sign));
  return __c166_addsubsf3(left, right);
}
