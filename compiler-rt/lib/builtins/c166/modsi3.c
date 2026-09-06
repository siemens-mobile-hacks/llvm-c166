//===-- modsi3.c - C166 signed 32-bit remainder builtin ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "int32_impl.h"

COMPILER_RT_ABI si_int __modsi3(si_int dividend, si_int divisor) {
  c166_u32 lhs = {.all = (su_int)dividend};
  c166_u32 rhs = {.all = (su_int)divisor};
  uint16_t lhs_negative = (uint16_t)(lhs.words.high & UINT16_C(0x8000));
  if (lhs_negative)
    lhs.all = (su_int)(0 - lhs.all);
  if (rhs.words.high & UINT16_C(0x8000))
    rhs.all = (su_int)(0 - rhs.all);

  c166_u32 remainder = c166_udivmod(lhs, rhs).remainder;
  if (lhs_negative)
    remainder.all = (su_int)(0 - remainder.all);
  return (si_int)remainder.all;
}
