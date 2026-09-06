//===-- divsi3.c - C166 signed 32-bit division builtin -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "int32_impl.h"

COMPILER_RT_ABI si_int __divsi3(si_int dividend, si_int divisor) {
  c166_u32 lhs = {.all = (su_int)dividend};
  c166_u32 rhs = {.all = (su_int)divisor};
  uint16_t lhs_negative = (uint16_t)(lhs.words.high & UINT16_C(0x8000));
  uint16_t rhs_negative = (uint16_t)(rhs.words.high & UINT16_C(0x8000));
  if (lhs_negative)
    lhs = c166_negate(lhs);
  if (rhs_negative)
    rhs = c166_negate(rhs);

  c166_u32 quotient = {.all = __udivsi3(lhs.all, rhs.all)};
  if (lhs_negative != rhs_negative)
    quotient.all = (su_int)(0 - quotient.all);
  return (si_int)quotient.all;
}
