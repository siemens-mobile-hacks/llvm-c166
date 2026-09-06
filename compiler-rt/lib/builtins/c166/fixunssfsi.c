//===-- fixunssfsi.c - C166 binary32-to-unsigned-integer builtin ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define SINGLE_PRECISION
#include "../fp_lib.h"
typedef su_int fixuint_t;
#include "../fp_fixuint_impl.inc"

COMPILER_RT_ABI su_int __fixunssfsi(rep_t value) {
  return __fixuint(fromRep(value));
}
