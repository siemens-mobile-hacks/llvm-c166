//===-- fp32_round.c - C166 binary32 rounding and packing ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define SINGLE_PRECISION
#include "../fp_lib.h"
#include "fp32_impl.h"

static inline ALWAYS_INLINE void
c166_sf_shift_right_sticky(c166_sf_limbs *value, unsigned int count) {
  bool sticky = false;
  if (count != 0) {
    count = 0 - count;
    do {
      sticky |= (value->low & 1) != 0;
      value->low = (value->low >> 1) | (value->high << 15);
      value->high >>= 1;
    } while (++count != 0);
  }
  value->low |= sticky;
}

COMPILER_RT_ABI rep_t __c166_sf_round_pack_finite(uint16_t low, uint16_t high,
                                                  int exponent, uint16_t sign) {
  const uint16_t fraction_mask = UINT16_C(0x007f);
  c166_sf_limbs value = {low, high};

  if (exponent <= 0) {
    // Callers exclude shifts that discard the complete significand.
    c166_sf_shift_right_sticky(&value, 1 - exponent);
    exponent = 0;
  }

  unsigned int round_guard_sticky = value.low & 7;
  value.low = (value.low >> 3) | (uint16_t)(value.high << 13);
  value.high >>= 3;
  value.high &= fraction_mask;
  value.high |= (uint16_t)(exponent << 7) | sign;

  uint16_t round_up = (uint16_t)(round_guard_sticky + (value.low & 1) + 3) >> 3;
  uint16_t unrounded_low = value.low;
  value.low += round_up;
  value.high += value.low < unrounded_low;
  return c166_sf_pack(value);
}
