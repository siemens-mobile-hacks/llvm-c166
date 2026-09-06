//===-- fp64_mul.c - C166 binary64 multiplication runtime ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"
#include "fp64.h"

static void c166_df_multiply_significands(c166_stack_word_ptr product,
                                          c166_stack_const_df_limbs_ptr left,
                                          c166_stack_const_df_limbs_ptr right) {
  // Use the native 16x16-to-32 multiply for each pair of little-endian
  // limbs.  A limb, one partial product, and the carry fit exactly in 32
  // bits, so no wider intermediate operation is needed.
  // The first row writes words 0 through 3.  Every higher word is written by
  // the carry from the preceding row before it can be read.
  product[0] = 0;
  product[1] = 0;
  product[2] = 0;
  product[3] = 0;

  for (unsigned int left_index = 0; left_index != 4; ++left_index) {
    uint16_t carry = 0;
    uint16_t left_word = left->word[left_index];
    c166_stack_const_word_ptr right_word =
        (c166_stack_const_word_ptr)right;
    c166_stack_word_ptr product_word = product + left_index;
    for (unsigned int right_index = 0; right_index != 4; ++right_index) {
      uint32_t partial = (uint32_t)left_word * *right_word++;
      uint32_t sum = partial + *product_word + carry;
      *product_word++ = (uint16_t)sum;
      carry = sum >> 16;
    }
    if (left_index != 3)
      *product_word = carry;
  }
}

COMPILER_RT_ABI void __c166_muldf3(c166_stack_word_ptr destination,
                                   c166_stack_const_word_ptr left,
                                   c166_stack_const_word_ptr right) {
  const uint16_t sign_bit = UINT16_C(0x8000);
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

  if (a_kind == C166_DF_NAN || b_kind == C166_DF_NAN) {
    if (a_kind == C166_DF_NAN) {
      c166_df_store_limbs(destination, a);
      return;
    }
    c166_df_store_limbs(destination, b);
    return;
  }
  uint16_t combined_kind = a_kind + b_kind;
  if (combined_kind == C166_DF_ZERO + C166_DF_INFINITY) {
    result_high = UINT16_C(0x7ff8);
    goto store_high_only;
  }

  uint16_t kind_bits = a_kind | b_kind;
  if ((kind_bits & C166_DF_INFINITY) != 0) {
    result_high = result_sign | UINT16_C(0x7ff0);
    goto store_high_only;
  }
  if ((kind_bits & C166_DF_ZERO) != 0) {
    result_high = result_sign;
    goto store_high_only;
  }

  int a_effective_exponent = (int16_t)a_info;
  int b_effective_exponent = (int16_t)b_info;

  uint16_t product[7];
  c166_stack_word_ptr product_pointer = (c166_stack_word_ptr)product;
  c166_df_multiply_significands(product_pointer, a_pointer, b_pointer);

  // The product has either 105 or 106 significant bits.  Retain the 53-bit
  // significand plus guard, round, and sticky bits.
  int result_exponent = a_effective_exponent + b_effective_exponent - 1023;
  bool high_product = (product[6] & UINT16_C(0x0200)) != 0;
  result_exponent += high_product;

  unsigned int bit_shift = 1 + high_product;
  if ((product[0] | product[1] | product[2]) != 0)
    product[3] |= 1;
  c166_stack_df_limbs_ptr result_pointer =
      (c166_stack_df_limbs_ptr)(product_pointer + 3);
  __c166_df_limbs_shift_right_sticky(result_pointer, bit_shift);

  __c166_df_round_pack(destination, result_pointer, result_exponent,
                       result_sign);
  return;

store_high_only:
  destination[0] = result_high;
  destination[1] = 0;
  destination[2] = 0;
  destination[3] = 0;
}
