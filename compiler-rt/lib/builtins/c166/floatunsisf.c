//===-- floatunsisf.c - C166 unsigned-integer-to-binary32 builtin --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define SINGLE_PRECISION
#include "../fp_lib.h"

#include "../int_lib.h"
#include "fp32_impl.h"

COMPILER_RT_ABI rep_t __floatunsisf(su_int value) {
  return c166_float_unsigned(value);
}
