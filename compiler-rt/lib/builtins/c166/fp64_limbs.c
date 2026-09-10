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

// The loop is shared by multiplication and round-pack, so keep one compact
// body.
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
