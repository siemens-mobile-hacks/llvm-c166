//===-- divsf3.c - C166 binary32 division builtin ------------------------===//
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

static unsigned int c166_sf_normalize(c166_sf_limbs *value) {
  unsigned int shift = 0;
  while ((value->high & UINT16_C(0x0080)) == 0) {
    c166_sf_shift_left_one(value);
    ++shift;
  }
  return shift;
}

static inline ALWAYS_INLINE int c166_sf_compare(const c166_sf_limbs *left,
                                                const c166_sf_limbs *right) {
  if (left->high != right->high)
    return left->high > right->high ? 1 : -1;
  if (left->low != right->low)
    return left->low > right->low ? 1 : -1;
  return 0;
}

static inline ALWAYS_INLINE void c166_sf_sub(c166_sf_limbs *left,
                                             const c166_sf_limbs *right) {
  uint16_t low = left->low - right->low;
  left->high = left->high - right->high - (left->low < right->low);
  left->low = low;
}

static inline ALWAYS_INLINE void c166_sf_increment(c166_sf_limbs *value) {
  if (++value->low == 0)
    ++value->high;
}

static inline ALWAYS_INLINE void
c166_sf_divide_shifted(c166_sf_limbs *quotient, c166_sf_limbs *numerator,
                       const c166_sf_limbs *denominator, unsigned int shift) {
  quotient->low = 0;
  quotient->high = 0;

  for (unsigned int index = 0;; ++index) {
    if (numerator->high > denominator->high ||
        (numerator->high == denominator->high &&
         numerator->low >= denominator->low)) {
      c166_sf_sub(numerator, denominator);
      quotient->low |= 1;
    }
    if (index == shift)
      break;
    c166_sf_shift_left_one(quotient);
    c166_sf_shift_left_one(numerator);
  }
}

static inline ALWAYS_INLINE bool
c166_sf_round_up(const c166_sf_limbs *quotient, c166_sf_limbs *remainder,
                 const c166_sf_limbs *denominator) {
  c166_sf_shift_left_one(remainder);
  int comparison = c166_sf_compare(remainder, denominator);
  return comparison > 0 || (comparison == 0 && (quotient->low & 1) != 0);
}

COMPILER_RT_ABI rep_t __divsf3(rep_t left, rep_t right) {
  const uint16_t sign_bit = UINT16_C(0x8000);
  const uint16_t fraction_mask = UINT16_C(0x007f);
  const uint16_t implicit_bit = UINT16_C(0x0080);

  c166_sf_limbs a = c166_sf_unpack(left);
  c166_sf_limbs b = c166_sf_unpack(right);
  uint16_t result_sign = (a.high ^ b.high) & sign_bit;
  unsigned int a_exponent = (a.high >> 7) & 0xff;
  unsigned int b_exponent = (b.high >> 7) & 0xff;
  uint16_t a_fraction = a.low | (a.high & fraction_mask);
  uint16_t b_fraction = b.low | (b.high & fraction_mask);
  uint16_t a_zero_bits = a_exponent | a_fraction;
  uint16_t b_zero_bits = b_exponent | b_fraction;

  if (a_exponent == 0xff) {
    if (a_fraction != 0) {
      a.high |= UINT16_C(0x0040);
      return c166_sf_pack(a);
    }
    if (b_exponent == 0xff) {
      if (b_fraction != 0) {
        b.high |= UINT16_C(0x0040);
        return c166_sf_pack(b);
      }
      return UINT32_C(0x7fc00000);
    }
    return c166_sf_pack(
        (c166_sf_limbs){0, (uint16_t)(result_sign | UINT16_C(0x7f80))});
  }
  if (b_exponent == 0xff) {
    if (b_fraction != 0) {
      b.high |= UINT16_C(0x0040);
      return c166_sf_pack(b);
    }
    return c166_sf_pack((c166_sf_limbs){0, result_sign});
  }

  if (a_zero_bits == 0) {
    if (b_zero_bits == 0)
      return UINT32_C(0x7fc00000);
    return c166_sf_pack((c166_sf_limbs){0, result_sign});
  }
  if (b_zero_bits == 0)
    return c166_sf_pack(
        (c166_sf_limbs){0, (uint16_t)(result_sign | UINT16_C(0x7f80))});

  a.high &= fraction_mask;
  b.high &= fraction_mask;
  int a_effective_exponent = a_exponent;
  int b_effective_exponent = b_exponent;
  if (a_exponent == 0)
    a_effective_exponent = 1 - c166_sf_normalize(&a);
  if (b_exponent == 0)
    b_effective_exponent = 1 - c166_sf_normalize(&b);
  a.high |= implicit_bit;
  b.high |= implicit_bit;

  int exponent_difference = a_effective_exponent - b_effective_exponent;
  int result_exponent = exponent_difference + 127;
  if (c166_sf_compare(&a, &b) < 0)
    --result_exponent;

  if (result_exponent >= 0xff)
    return c166_sf_pack(
        (c166_sf_limbs){0, (uint16_t)(result_sign | UINT16_C(0x7f80))});

  unsigned int shift;
  if (result_exponent > 0) {
    shift = exponent_difference + 150 - result_exponent;
  } else {
    int subnormal_shift = exponent_difference + 149;
    if (subnormal_shift < 0) {
      c166_sf_limbs result = {0, result_sign};
      if (subnormal_shift == -1 && c166_sf_compare(&a, &b) > 0)
        result.low = 1;
      return c166_sf_pack(result);
    }
    shift = subnormal_shift;
    result_exponent = 0;
  }

  c166_sf_limbs result;
  c166_sf_divide_shifted(&result, &a, &b, shift);
  // The quotient has at most 24 bits, so only its implicit bit needs removal.
  result.high &= ~implicit_bit;
  result.high |= (uint16_t)(result_exponent << 7);
  result.high |= result_sign;
  if (c166_sf_round_up(&result, &a, &b))
    c166_sf_increment(&result);
  return c166_sf_pack(result);
}
