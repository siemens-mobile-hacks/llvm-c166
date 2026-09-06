//===-- floatsidf_direct.c - C166 direct signed integer to binary64 -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"
#include "fp64.h"
#include "fp64_int_impl.h"

COMPILER_RT_ABI void __c166_floatsidf(c166_stack_word_ptr destination,
                                      si_int value) {
  su_int magnitude = (su_int)value;
  uint16_t sign = 0;
  if (value < 0) {
    magnitude = -magnitude;
    sign = UINT16_C(0x8000);
  }

  c166_df_int_encoding result = c166_df_encode_int(magnitude, sign);
  destination[0] = result.top;
  destination[1] = result.second;
  destination[2] = result.third;
  destination[3] = 0;
}
