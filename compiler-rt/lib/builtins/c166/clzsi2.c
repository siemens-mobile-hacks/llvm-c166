//===-- clzsi2.c - C166 32-bit count-leading-zeros builtin ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "int32_impl.h"

// Unlike fixed-width arithmetic builtins, this function returns C int in one
// 16-bit register.
COMPILER_RT_ABI int __clzsi2(si_int value) {
  c166_u32 bits = {.all = (su_int)value};
  if (bits.words.high != 0)
    return __builtin_clz((unsigned int)bits.words.high);
  if (bits.words.low != 0)
    return 16 + __builtin_clz((unsigned int)bits.words.low);
  return 32;
}
