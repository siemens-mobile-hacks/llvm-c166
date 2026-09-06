//===-- comparesf2.c - C166 binary32 comparison builtins -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define SINGLE_PRECISION
#include "../fp_compare_impl.inc"
#include "../fp_lib.h"

static NOINLINE int c166_sf_compare_words(uint16_t left_bottom,
                                          uint16_t left_top,
                                          uint16_t right_bottom,
                                          uint16_t right_top) {
  // The public entry points select their required unordered result through
  // the caller-saved R10 register.
  register int unordered_result __asm__("r10");
  __asm__("" : "=r"(unordered_result));
  uint16_t left_abs_top = left_top & UINT16_C(0x7fff);
  uint16_t right_abs_top = right_top & UINT16_C(0x7fff);
  if (left_abs_top == UINT16_C(0x7f80)) {
    if (left_bottom != 0)
      return unordered_result;
  } else if (left_abs_top > UINT16_C(0x7f80)) {
    return unordered_result;
  }
  if (right_abs_top == UINT16_C(0x7f80)) {
    if (right_bottom != 0)
      return unordered_result;
  } else if (right_abs_top > UINT16_C(0x7f80)) {
    return unordered_result;
  }

  bool negative = (left_top & UINT16_C(0x8000)) != 0;
  if (((left_top ^ right_top) & UINT16_C(0x8000)) != 0) {
    uint16_t magnitude =
        left_abs_top | right_abs_top | left_bottom | right_bottom;
    if (magnitude == 0)
      return 0;
    return negative ? -1 : 1;
  }

  if (left_top < right_top)
    goto magnitude_less;
  if (left_top > right_top)
    goto magnitude_greater;
  if (left_bottom < right_bottom)
    goto magnitude_less;
  if (left_bottom > right_bottom)
    goto magnitude_greater;
  return 0;

magnitude_less:
  return negative ? 1 : -1;
magnitude_greater:
  return negative ? -1 : 1;
}

static inline ALWAYS_INLINE int c166_sf_compare(rep_t left, rep_t right) {
  return c166_sf_compare_words((uint16_t)left, (uint16_t)(left >> 16),
                               (uint16_t)right, (uint16_t)(right >> 16));
}

COMPILER_RT_ABI CMP_RESULT __lesf2(rep_t left, rep_t right) {
  register int unordered_result __asm__("r10") = 1;
  __asm__ volatile("" : : "r"(unordered_result));
  return c166_sf_compare(left, right);
}

COMPILER_RT_ALIAS(__lesf2, __eqsf2)
COMPILER_RT_ALIAS(__lesf2, __ltsf2)
COMPILER_RT_ALIAS(__lesf2, __nesf2)

COMPILER_RT_ABI CMP_RESULT __gesf2(rep_t left, rep_t right) {
  register int unordered_result __asm__("r10") = -1;
  __asm__ volatile("" : : "r"(unordered_result));
  return c166_sf_compare(left, right);
}

COMPILER_RT_ALIAS(__gesf2, __gtsf2)

COMPILER_RT_ABI CMP_RESULT __unordsf2(rep_t left, rep_t right) {
  return __unordXf2__(fromRep(left), fromRep(right));
}
