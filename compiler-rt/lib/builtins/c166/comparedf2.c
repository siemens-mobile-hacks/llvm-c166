//===-- comparedf2.c - C166 binary64 comparison builtins -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"

typedef long CMP_RESULT;

static inline ALWAYS_INLINE bool c166_df_is_nan(rep_t value) {
  uint16_t top = value >> 48;
  uint16_t absolute_top = top & UINT16_C(0x7fff);
  if (absolute_top == UINT16_C(0x7ff0))
    return ((uint16_t)(value >> 32) | (uint16_t)(value >> 16) |
            (uint16_t)value) != 0;
  return absolute_top > UINT16_C(0x7ff0);
}

static inline ALWAYS_INLINE CMP_RESULT
c166_df_compare_value(fp_t left, fp_t right, CMP_RESULT unordered) {
  rep_t left_rep = toRep(left);
  rep_t right_rep = toRep(right);

  if (c166_df_is_nan(left_rep) || c166_df_is_nan(right_rep))
    return unordered;

  uint16_t left_top = left_rep >> 48;
  uint16_t left_second = left_rep >> 32;
  uint16_t left_third = left_rep >> 16;
  uint16_t left_fourth = left_rep;
  uint16_t right_top = right_rep >> 48;
  uint16_t right_second = right_rep >> 32;
  uint16_t right_third = right_rep >> 16;
  uint16_t right_fourth = right_rep;

  if ((((left_top & UINT16_C(0x7fff)) | left_second | left_third |
        left_fourth) == 0) &&
      (((right_top & UINT16_C(0x7fff)) | right_second | right_third |
        right_fourth) == 0))
    return 0;

  bool negative = (left_top & UINT16_C(0x8000)) != 0;
  if (((left_top ^ right_top) & UINT16_C(0x8000)) != 0)
    return negative ? -1 : 1;

#define C166_DF_COMPARE_WORD(Left, Right)                                      \
  do {                                                                         \
    if ((Left) != (Right))                                                     \
      return ((Left) < (Right)) != negative ? -1 : 1;                          \
  } while (false)

  C166_DF_COMPARE_WORD(left_top, right_top);
  C166_DF_COMPARE_WORD(left_second, right_second);
  C166_DF_COMPARE_WORD(left_third, right_third);
  C166_DF_COMPARE_WORD(left_fourth, right_fourth);

#undef C166_DF_COMPARE_WORD

  return 0;
}

COMPILER_RT_ABI CMP_RESULT __ledf2(fp_t left, fp_t right) {
  return c166_df_compare_value(left, right, 1);
}

#if defined(__ELF__)
COMPILER_RT_ALIAS(__ledf2, __cmpdf2)
#endif
COMPILER_RT_ALIAS(__ledf2, __eqdf2)
COMPILER_RT_ALIAS(__ledf2, __ltdf2)
COMPILER_RT_ALIAS(__ledf2, __nedf2)

COMPILER_RT_ABI CMP_RESULT __gedf2(fp_t left, fp_t right) {
  return c166_df_compare_value(left, right, -1);
}

COMPILER_RT_ALIAS(__gedf2, __gtdf2)

COMPILER_RT_ABI CMP_RESULT __unorddf2(fp_t left, fp_t right) {
  return c166_df_is_nan(toRep(left)) || c166_df_is_nan(toRep(right));
}
