//===-- fp64_div.c - C166 binary64 division runtime ----------------------===//
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
c166_df_limbs_shift_left_one_pair(c166_stack_df_limbs_ptr first,
                                  c166_stack_df_limbs_ptr second) {
  uint16_t first_carry = 0;
  uint16_t second_carry = 0;
  for (unsigned int index = 0; index != 4; ++index) {
    uint16_t first_word = first->word[index];
    uint16_t second_word = second->word[index];
    first->word[index] = (first_word << 1) | first_carry;
    second->word[index] = (second_word << 1) | second_carry;
    first_carry = first_word >> 15;
    second_carry = second_word >> 15;
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

static inline ALWAYS_INLINE int
c166_df_limbs_compare_twice(c166_stack_const_df_limbs_ptr left,
                            c166_stack_const_df_limbs_ptr right) {
  uint16_t current = left->word[3];
  for (unsigned int index = 4; index-- != 0;) {
    uint16_t next = index != 0 ? left->word[index - 1] : 0;
    uint16_t doubled = (current << 1) | (next >> 15);
    if (doubled != right->word[index])
      return doubled > right->word[index] ? 1 : -1;
    current = next;
  }
  return 0;
}

static inline ALWAYS_INLINE void
c166_df_limbs_sub(c166_stack_df_limbs_ptr left,
                  c166_stack_const_df_limbs_ptr right) {
  bool borrow = false;
  for (unsigned int index = 0; index != 4; ++index) {
    unsigned int next_borrow;
    left->word[index] = __builtin_subc(left->word[index], right->word[index],
                                       borrow, &next_borrow);
    borrow = next_borrow != 0;
  }
}

static inline ALWAYS_INLINE void c166_df_divide_shifted(
    c166_stack_df_limbs_ptr quotient, c166_stack_df_limbs_ptr numerator,
    c166_stack_const_df_limbs_ptr denominator, unsigned int shift) {
  for (unsigned int index = 0;; ++index) {
    if (c166_df_limbs_compare(numerator, denominator) >= 0) {
      c166_df_limbs_sub(numerator, denominator);
      quotient->word[0] |= 1;
    }
    if (index == shift)
      break;
    c166_df_limbs_shift_left_one_pair(quotient, numerator);
  }
}

static inline ALWAYS_INLINE bool
c166_df_round_up(c166_stack_const_df_limbs_ptr quotient,
                 c166_stack_const_df_limbs_ptr remainder,
                 c166_stack_const_df_limbs_ptr denominator) {
  int comparison = c166_df_limbs_compare_twice(remainder, denominator);
  return comparison > 0 || (comparison == 0 && (quotient->word[0] & 1) != 0);
}

COMPILER_RT_ABI void __c166_divdf3(c166_stack_word_ptr destination,
                                   c166_stack_const_word_ptr left,
                                   c166_stack_const_word_ptr right) {
  const uint16_t sign_bit = UINT16_C(0x8000);
  const uint16_t implicit_bit = UINT16_C(0x0010);
  c166_df_limbs a;
  c166_df_limbs b;
  c166_stack_df_limbs_ptr a_pointer = (c166_stack_df_limbs_ptr)&a;
  c166_stack_df_limbs_ptr b_pointer = (c166_stack_df_limbs_ptr)&b;
  uint32_t a_info = __c166_df_unpack(a_pointer, left);
  uint32_t b_info = __c166_df_unpack(b_pointer, right);
  uint16_t a_status = a_info >> 16;
  uint16_t b_status = b_info >> 16;
  uint16_t a_kind = a_status & C166_DF_KIND_MASK;
  uint16_t b_kind = b_status & C166_DF_KIND_MASK;
  uint16_t result_high;
  uint16_t result_sign = (a_status ^ b_status) & sign_bit;

  if (a_kind == C166_DF_NAN) {
    c166_df_store_limbs(destination, a);
    return;
  }
  if (b_kind == C166_DF_NAN) {
    c166_df_store_limbs(destination, b);
    return;
  }
  if (a_kind == b_kind && a_kind != C166_DF_FINITE) {
    result_high = UINT16_C(0x7ff8);
    goto store_high_only;
  }
  if (a_kind == C166_DF_INFINITY || b_kind == C166_DF_ZERO) {
    result_high = result_sign | UINT16_C(0x7ff0);
    goto store_high_only;
  }
  if (b_kind == C166_DF_INFINITY || a_kind == C166_DF_ZERO) {
    result_high = result_sign;
    goto store_high_only;
  }

  int a_effective_exponent = (int16_t)a_info;
  int b_effective_exponent = (int16_t)b_info;

  int exponent_difference = a_effective_exponent - b_effective_exponent;
  int result_exponent = exponent_difference + 1023;
  int numerator_comparison = c166_df_limbs_compare(a_pointer, b_pointer);
  bool numerator_is_smaller = numerator_comparison < 0;
  result_exponent -= numerator_is_smaller;

  if (result_exponent >= 0x7ff) {
    result_high = result_sign | UINT16_C(0x7ff0);
    goto store_high_only;
  }

  unsigned int shift;
  if (result_exponent > 0) {
    shift = 52 + numerator_is_smaller;
  } else {
    int subnormal_shift = exponent_difference + 1074;
    if (subnormal_shift < 0) {
      c166_df_limbs result = {{0, 0, 0, result_sign}};
      uint16_t shift_difference = (uint16_t)(subnormal_shift + 1);
      uint16_t comparison_difference = (uint16_t)(numerator_comparison - 1);
      result.word[0] = (shift_difference | comparison_difference) == 0;
      c166_df_store_limbs(destination, result);
      return;
    }
    shift = subnormal_shift;
    result_exponent = 0;
  }

  c166_df_limbs result = {{0, 0, 0, 0}};
  c166_stack_df_limbs_ptr result_pointer = (c166_stack_df_limbs_ptr)&result;
  c166_df_divide_shifted(result_pointer, a_pointer, b_pointer, shift);
  // The quotient has at most 53 bits, so only its implicit bit needs removal.
  result.word[3] &= ~implicit_bit;
  result.word[3] |= (uint16_t)(result_exponent << 4);
  result.word[3] |= result_sign;
  if (c166_df_round_up(result_pointer, a_pointer, b_pointer))
    __c166_df_limbs_increment(result_pointer);
  c166_df_store_limbs(destination, result);
  return;

store_high_only:
  destination[0] = result_high;
  destination[1] = 0;
  destination[2] = 0;
  destination[3] = 0;
}
