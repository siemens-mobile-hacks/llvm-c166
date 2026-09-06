//===-- fp64_add_impl.c - C166 binary64 addition core --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"
#include "fp64.h"

static inline ALWAYS_INLINE void
c166_df_limbs_shift_left_three(c166_stack_df_limbs_ptr value) {
  uint16_t carry = 0;
  for (unsigned int index = 0; index != 4; ++index) {
    uint16_t word = value->word[index];
    value->word[index] = (word << 3) | carry;
    carry = word >> 13;
  }
}

static inline ALWAYS_INLINE int
c166_df_limbs_compare(c166_stack_const_df_limbs_ptr left,
                      c166_stack_const_df_limbs_ptr right) {
  for (unsigned int index = 4; index-- != 0;) {
    if (left->word[index] != right->word[index])
      return left->word[index] > right->word[index] ? 1 : -1;
  }
  return 0;
}

static inline ALWAYS_INLINE void c166_df_limbs_add(c166_df_limbs *left,
                                                   const c166_df_limbs *right) {
  left->all += right->all;
}

static inline ALWAYS_INLINE void c166_df_limbs_sub(c166_df_limbs *left,
                                                   const c166_df_limbs *right) {
  left->all -= right->all;
}

COMPILER_RT_ABI void
__c166_addsubdf3(c166_stack_word_ptr destination,
                 c166_stack_const_word_ptr left,
                 c166_stack_const_word_ptr right, uint16_t right_sign) {
  const uint16_t sign_bit = UINT16_C(0x8000);
  const uint16_t implicit_bit = UINT16_C(0x0010);

  c166_df_limbs a;
  c166_df_limbs b;
  c166_stack_df_limbs_ptr a_pointer = (c166_stack_df_limbs_ptr)&a;
  c166_stack_df_limbs_ptr b_pointer = (c166_stack_df_limbs_ptr)&b;
  uint32_t a_info = __c166_df_unpack(a_pointer, left);
  uint32_t b_info = __c166_df_unpack(b_pointer, right);
  uint16_t a_status = a_info >> 16;
  uint16_t b_status = (b_info >> 16) ^ right_sign;
  uint16_t a_kind = a_status & C166_DF_KIND_MASK;
  uint16_t b_kind = b_status & C166_DF_KIND_MASK;
  uint16_t result_high;
  uint16_t a_sign = a_status & sign_bit;
  uint16_t b_sign = b_status & sign_bit;
  int a_exponent = (int16_t)a_info;
  int b_exponent = (int16_t)b_info;

  if (a_kind == C166_DF_NAN) {
    c166_df_store_limbs(destination, a);
    return;
  }
  if (b_kind == C166_DF_NAN) {
    b.word[3] = (b.word[3] & ~sign_bit) | b_sign;
    c166_df_store_limbs(destination, b);
    return;
  }
  if (a_kind == C166_DF_INFINITY) {
    if (b_kind == C166_DF_INFINITY && a_sign != b_sign) {
      result_high = UINT16_C(0x7ff8);
      goto store_high_only;
    }
    c166_df_store_limbs(destination, a);
    return;
  }
  if (b_kind == C166_DF_INFINITY) {
    b.word[3] = (b.word[3] & ~sign_bit) | b_sign;
    c166_df_store_limbs(destination, b);
    return;
  }

  if (a_kind == C166_DF_ZERO) {
    if (b_kind == C166_DF_ZERO)
      b_sign &= a_sign;
    b = c166_df_load_limbs(right);
    b.word[3] = (b.word[3] & ~sign_bit) | b_sign;
    c166_df_store_limbs(destination, b);
    return;
  }
  if (b_kind == C166_DF_ZERO) {
    a = c166_df_load_limbs(left);
    c166_df_store_limbs(destination, a);
    return;
  }

  if (b_exponent > a_exponent ||
      (b_exponent == a_exponent &&
       c166_df_limbs_compare(b_pointer, a_pointer) > 0)) {
    c166_stack_word_ptr a_word = (c166_stack_word_ptr)a_pointer;
    c166_stack_word_ptr b_word = (c166_stack_word_ptr)b_pointer;
    for (unsigned int index = 0; index != 4; ++index) {
      uint16_t temporary = *a_word;
      *a_word++ = *b_word;
      *b_word++ = temporary;
    }
    int temporary_exponent = a_exponent;
    a_exponent = b_exponent;
    b_exponent = temporary_exponent;
    uint16_t temporary_sign = a_sign;
    a_sign = b_sign;
    b_sign = temporary_sign;
  }

  c166_df_limbs_shift_left_three(a_pointer);
  c166_df_limbs_shift_left_three(b_pointer);

  unsigned int align = (unsigned int)(a_exponent - b_exponent);
  if (align != 0)
    __c166_df_limbs_shift_right_sticky(b_pointer, align);

  if (a_sign != b_sign) {
    c166_df_limbs_sub(&a, &b);
    if (c166_df_limbs_is_zero(&a)) {
      result_high = 0;
      goto store_high_only;
    }
    if ((a.word[3] & (implicit_bit << 3)) == 0) {
      unsigned int shift =
          __c166_df_limbs_normalize(a_pointer, implicit_bit << 3);
      a_exponent -= shift;
    }
  } else {
    c166_df_limbs_add(&a, &b);
    if ((a.word[3] & (implicit_bit << 4)) != 0) {
      __c166_df_limbs_shift_right_sticky(a_pointer, 1);
      ++a_exponent;
    }
  }

  __c166_df_round_pack(destination, a_pointer, a_exponent, a_sign);
  return;

store_high_only:
  destination[0] = result_high;
  destination[1] = 0;
  destination[2] = 0;
  destination[3] = 0;
}
