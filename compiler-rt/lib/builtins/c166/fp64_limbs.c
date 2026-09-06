//===-- fp64_limbs.c - C166 binary64 limb operations ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"
#include "fp64.h"

// The loop is shared by unpacking and add/subtract, so keep one compact body.
NOINLINE __attribute__((minsize)) unsigned int
__c166_df_limbs_normalize(c166_stack_df_limbs_ptr value, uint16_t top_bit) {
  unsigned int shift = 0;
  while ((value->word[3] & top_bit) == 0) {
    uint16_t carry = 0;
    unsigned int index = 0;
    do {
      uint16_t next_carry = value->word[index] >> 15;
      value->word[index] = (uint16_t)(value->word[index] << 1) | carry;
      carry = next_carry;
    } while (++index != 4);
    ++shift;
  }
  return shift;
}

// The loop is shared by add/subtract and round-pack, so keep one compact body.
NOINLINE __attribute__((minsize)) void
__c166_df_limbs_shift_right_sticky(c166_stack_df_limbs_ptr value,
                                   unsigned int count) {
  uint16_t sticky = 0;
  if (count >= 64) {
    c166_stack_word_ptr word = (c166_stack_word_ptr)value;
    unsigned int index = 0;
    do {
      sticky |= *word;
      *word++ = 0;
    } while (++index != 4);
  } else {
    while (count-- != 0) {
      c166_stack_word_ptr word = (c166_stack_word_ptr)value;
      uint16_t lower = *word;
      sticky |= lower & 1;
      for (unsigned int index = 0; index != 3; ++index) {
        uint16_t higher = word[1];
        *word++ = (lower >> 1) | (higher << 15);
        lower = higher;
      }
      *word = lower >> 1;
    }
  }
  if (sticky != 0)
    value->word[0] |= 1;
}

uint32_t __c166_df_unpack(c166_stack_df_limbs_ptr value,
                          c166_stack_const_word_ptr source) {
  const uint16_t sign_bit = UINT16_C(0x8000);
  const uint16_t fraction_mask = UINT16_C(0x000f);
  const uint16_t implicit_bit = UINT16_C(0x0010);
  c166_stack_const_word_ptr input = source;
  uint16_t high = *input++;
  value->word[3] = high;
  uint16_t fraction = high & fraction_mask;
  c166_stack_word_ptr destination = (c166_stack_word_ptr)value + 3;
  for (unsigned int index = 0; index != 3; ++index) {
    uint16_t word = *input++;
    *--destination = word;
    fraction |= word;
  }

  uint16_t sign = high & sign_bit;
  int exponent = (high >> 4) & 0x7ff;
  bool fraction_zero = fraction == 0;
  uint16_t kind;

  if (exponent == 0x7ff) {
    if (fraction_zero) {
      kind = C166_DF_INFINITY;
    } else {
      value->word[3] |= UINT16_C(0x0008);
      kind = C166_DF_NAN;
    }
  } else if (exponent == 0 && fraction_zero) {
    kind = C166_DF_ZERO;
  } else {
    uint16_t significand_high = high & fraction_mask;
    if (exponent == 0) {
      value->word[3] = significand_high;
      exponent = 1 - __c166_df_limbs_normalize(value, implicit_bit);
    } else {
      value->word[3] = significand_high | implicit_bit;
    }
    kind = C166_DF_FINITE;
  }

  return (uint32_t)(sign | kind) << 16 | (uint16_t)exponent;
}

void __c166_df_round_pack(c166_stack_word_ptr destination,
                          c166_stack_df_limbs_ptr value, int exponent,
                          uint16_t sign) {
  const uint16_t fraction_mask = UINT16_C(0x000f);
  if (exponent >= 0x7ff) {
    destination[0] = sign | UINT16_C(0x7ff0);
    destination[1] = 0;
    destination[2] = 0;
    destination[3] = 0;
    return;
  }

  if (exponent <= 0) {
    __c166_df_limbs_shift_right_sticky(value, 1 - exponent);
    exponent = 0;
  }

  unsigned int round_guard_sticky = value->word[0] & 7;
  c166_stack_word_ptr word = (c166_stack_word_ptr)value;
  uint16_t lower = *word;
  for (unsigned int index = 0; index != 3; ++index) {
    uint16_t higher = word[1];
    *word++ = (lower >> 3) | (uint16_t)(higher << 13);
    lower = higher;
  }
  uint16_t high = (lower >> 3) & fraction_mask;
  *word = high | (uint16_t)(exponent << 4) | sign;

  unsigned int round_up = (round_guard_sticky + (value->word[0] & 1) + 3) >> 3;
  if (round_up != 0)
    __c166_df_limbs_increment(value);
  c166_df_store_limbs(destination, *value);
}
