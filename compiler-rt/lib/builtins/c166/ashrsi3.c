//===-- ashrsi3.c - C166 32-bit arithmetic-right-shift builtin -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "int32_impl.h"

COMPILER_RT_ABI si_int __ashrsi3(si_int value, int amount) {
  c166_u32 result = {.all = (su_int)value};
  uint16_t count = (uint16_t)amount;
  if (count >= 32)
    count = 31;
  while (count-- != 0)
    result = c166_ashr1(result);
  return (si_int)result.all;
}
