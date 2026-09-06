//===-- divdf3.c - C166 double-precision division ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../int_lib.h"

COMPILER_RT_ABI double __divdf3(double left, double right) {
  return left / right;
}
