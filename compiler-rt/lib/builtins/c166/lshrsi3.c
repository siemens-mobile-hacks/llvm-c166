//===-- lshrsi3.c - C166 32-bit logical-right-shift builtin --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "int32_impl.h"

COMPILER_RT_ABI su_int __lshrsi3(su_int value, int amount) {
  c166_u32 result = {.all = value};
  uint16_t count = (uint16_t)amount;
  if (count >= 32)
    return 0;
  while (count-- != 0)
    result = c166_lshr1(result);
  return result.all;
}
