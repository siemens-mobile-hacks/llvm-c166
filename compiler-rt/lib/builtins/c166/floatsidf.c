//===-- floatsidf.c - C166 signed integer to binary64 -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"
#include "fp64_int_impl.h"

COMPILER_RT_ABI fp_t __floatsidf(si_int value) {
  su_int magnitude = (su_int)value;
  uint16_t sign = 0;
  if (value < 0) {
    magnitude = -magnitude;
    sign = UINT16_C(0x8000);
  }
  return fromRep(c166_df_pack_int(c166_df_encode_int(magnitude, sign)));
}
