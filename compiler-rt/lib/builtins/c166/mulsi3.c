//===-- mulsi3.c - C166 32-bit multiplication builtin --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "int32_impl.h"

COMPILER_RT_ABI si_int __mulsi3(si_int lhs, si_int rhs) {
  c166_u32 left = {.all = (su_int)lhs};
  c166_u32 right = {.all = (su_int)rhs};
  su_int low_product =
      (su_int)left.words.low * (su_int)right.words.low;
  c166_u32 result = {.all = low_product};
  result.words.high =
      (uint16_t)(result.words.high + left.words.low * right.words.high +
                 left.words.high * right.words.low);
  return (si_int)result.all;
}
