//===-- udivsi3.c - C166 unsigned 32-bit division builtin ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "int32_impl.h"

COMPILER_RT_ABI su_int __udivsi3(su_int dividend, su_int divisor) {
  c166_u32 lhs = {.all = dividend};
  c166_u32 rhs = {.all = divisor};
  return c166_udivmod(lhs, rhs).quotient.all;
}
