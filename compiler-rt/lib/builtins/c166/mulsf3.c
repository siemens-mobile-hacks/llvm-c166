//===-- mulsf3.c - C166 binary32 multiplication builtin ------------------===//
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

static inline ALWAYS_INLINE void
c166_sf_multiply_significands(uint16_t product[3], const c166_sf_limbs *left,
                              const c166_sf_limbs *right) {
  uint32_t low_product = (uint32_t)left->low * right->low;
  uint32_t middle = (uint32_t)left->low * right->high +
                    (uint32_t)left->high * right->low + (low_product >> 16);
  product[0] = (uint16_t)low_product;
  product[1] = (uint16_t)middle;
  uint16_t high_product = (uint16_t)((uint32_t)left->high * right->high);
  product[2] = high_product + (uint16_t)(middle >> 16);
}

COMPILER_RT_ABI rep_t __mulsf3(rep_t left, rep_t right) {
  const uint16_t sign_bit = UINT16_C(0x8000);
  const uint16_t fraction_mask = UINT16_C(0x007f);
  const uint16_t implicit_bit = UINT16_C(0x0080);

  c166_sf_limbs a = c166_sf_unpack(left);
  c166_sf_limbs b = c166_sf_unpack(right);
  uint16_t result_sign = (a.high ^ b.high) & sign_bit;
  unsigned int a_exponent = (a.high >> 7) & 0xff;
  unsigned int b_exponent = (b.high >> 7) & 0xff;
  bool a_fraction_zero = (a.low | (a.high & fraction_mask)) == 0;
  bool b_fraction_zero = (b.low | (b.high & fraction_mask)) == 0;

  if (a_exponent == 0xff) {
    if (!a_fraction_zero) {
      a.high |= UINT16_C(0x0040);
      return c166_sf_pack(a);
    }
    if (b_exponent == 0xff) {
      if (!b_fraction_zero) {
        b.high |= UINT16_C(0x0040);
        return c166_sf_pack(b);
      }
    } else if (b_exponent == 0 && b_fraction_zero) {
      return UINT32_C(0x7fc00000);
    }
    return c166_sf_pack(
        (c166_sf_limbs){0, (uint16_t)(result_sign | UINT16_C(0x7f80))});
  }
  if (b_exponent == 0xff) {
    if (!b_fraction_zero) {
      b.high |= UINT16_C(0x0040);
      return c166_sf_pack(b);
    }
    if (a_exponent == 0 && a_fraction_zero)
      return UINT32_C(0x7fc00000);
    return c166_sf_pack(
        (c166_sf_limbs){0, (uint16_t)(result_sign | UINT16_C(0x7f80))});
  }

  if ((a_exponent == 0 && a_fraction_zero) ||
      (b_exponent == 0 && b_fraction_zero))
    return c166_sf_pack((c166_sf_limbs){0, result_sign});

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

  uint16_t product[3];
  c166_sf_multiply_significands(product, &a, &b);

  // Retain the 24-bit significand plus guard, round, and sticky bits.
  int result_exponent = a_effective_exponent + b_effective_exponent - 127;
  bool high_product = (product[2] & UINT16_C(0x8000)) != 0;
  result_exponent += high_product;

  if (result_exponent >= 0xff)
    return c166_sf_pack(
        (c166_sf_limbs){0, (uint16_t)(result_sign | UINT16_C(0x7f80))});
  if (result_exponent <= -31)
    return (rep_t)result_sign << 16;

  c166_sf_limbs result;
  unsigned int bit_shift = 4 + high_product;
  unsigned int inverse_shift = 16 - bit_shift;
  result.low =
      (product[1] >> bit_shift) | (uint16_t)(product[2] << inverse_shift);
  result.high = product[2] >> bit_shift;
  uint16_t discarded_mask = (UINT16_C(1) << bit_shift) - 1;
  if ((product[0] | (product[1] & discarded_mask)) != 0)
    result.low |= 1;

  return __c166_sf_round_pack_finite(result.low, result.high, result_exponent,
                                     result_sign);
}
