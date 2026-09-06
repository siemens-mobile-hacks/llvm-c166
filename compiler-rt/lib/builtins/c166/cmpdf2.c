//===-- cmpdf2.c - C166 direct binary64 comparison ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"
#include "fp64.h"

enum {
  C166_DF_LESS = 1,
  C166_DF_EQUAL = 2,
  C166_DF_GREATER = 4,
  C166_DF_UNORDERED = 8,
};

COMPILER_RT_ABI int __c166_cmpdf2(c166_stack_const_word_ptr left,
                                  c166_stack_const_word_ptr right) {
  uint16_t left_top = left[0];
  uint16_t right_top = right[0];
  uint16_t left_tail = left[1] | left[2] | left[3];
  uint16_t right_tail = right[1] | right[2] | right[3];
  uint16_t left_absolute_top = left_top & UINT16_C(0x7fff);
  uint16_t right_absolute_top = right_top & UINT16_C(0x7fff);

  if (left_absolute_top == UINT16_C(0x7ff0)) {
    if (left_tail != 0)
      return C166_DF_UNORDERED;
  } else if (left_absolute_top > UINT16_C(0x7ff0)) {
    return C166_DF_UNORDERED;
  }
  if (right_absolute_top == UINT16_C(0x7ff0)) {
    if (right_tail != 0)
      return C166_DF_UNORDERED;
  } else if (right_absolute_top > UINT16_C(0x7ff0)) {
    return C166_DF_UNORDERED;
  }

  bool negative = (left_top & UINT16_C(0x8000)) != 0;
  if (((left_top ^ right_top) & UINT16_C(0x8000)) != 0) {
    uint16_t magnitude =
        left_absolute_top | right_absolute_top | left_tail | right_tail;
    if (magnitude == 0)
      return C166_DF_EQUAL;
    return negative ? C166_DF_LESS : C166_DF_GREATER;
  }

#define C166_DF_COMPARE_WORD(Left, Right)                                      \
  do {                                                                         \
    if ((Left) != (Right))                                                     \
      return ((Left) < (Right)) != negative ? C166_DF_LESS : C166_DF_GREATER;  \
  } while (false)

  C166_DF_COMPARE_WORD(left_top, right_top);
  for (unsigned int index = 1; index != 4; ++index)
    C166_DF_COMPARE_WORD(left[index], right[index]);

#undef C166_DF_COMPARE_WORD

  return C166_DF_EQUAL;
}
