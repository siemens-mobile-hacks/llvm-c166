//===-- fp64_sub.c - C166 binary64 subtraction runtime -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"
#include "fp64.h"

COMPILER_RT_ABI void __c166_subdf3(c166_stack_word_ptr destination,
                                   c166_stack_const_word_ptr left,
                                   c166_stack_const_word_ptr right) {
  __c166_addsubdf3(destination, left, right, 0x8000);
}
