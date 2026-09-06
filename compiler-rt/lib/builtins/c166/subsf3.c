//===-- subsf3.c - C166 binary32 subtraction builtin ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define SINGLE_PRECISION
#include "../fp_lib.h"

// The shared core reads this entry point's sign mask from caller-saved R10.
COMPILER_RT_ABI rep_t __c166_addsubsf3(rep_t left, rep_t right);

COMPILER_RT_ABI rep_t __subsf3(rep_t left, rep_t right) {
  register uint16_t right_sign __asm__("r10") = UINT16_C(0x8000);
  __asm__ volatile("" : : "r"(right_sign));
  return __c166_addsubsf3(left, right);
}
