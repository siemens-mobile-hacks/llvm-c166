//===-- fp64.h - C166 binary64 runtime helpers -------------------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef COMPILER_RT_LIB_BUILTINS_C166_FP64_H
#define COMPILER_RT_LIB_BUILTINS_C166_FP64_H

#if __C166_MEMORY_MODEL__ == 3 || __C166_MEMORY_MODEL__ == 4
#define C166_STACK_ADDRESS_CLASS c166_near
#else
#define C166_STACK_ADDRESS_CLASS c166_xnear
#endif

typedef uint16_t __attribute__((C166_STACK_ADDRESS_CLASS)) *
    c166_stack_word_ptr;
typedef const uint16_t __attribute__((C166_STACK_ADDRESS_CLASS)) *
    c166_stack_const_word_ptr;

typedef union {
  // Little-endian base-2^16 limbs.  The public memory representation is
  // converted only at the runtime boundary.
  uint16_t word[4];
  rep_t all;
} c166_df_limbs;

// Limb temporaries use the user-stack DPP: xnear/DPP1 in Large and Medium,
// and the default near/DPP2 address class in Tiny and Small.
typedef c166_df_limbs __attribute__((C166_STACK_ADDRESS_CLASS)) *
    c166_stack_df_limbs_ptr;
typedef const c166_df_limbs __attribute__((C166_STACK_ADDRESS_CLASS)) *
    c166_stack_const_df_limbs_ptr;

#undef C166_STACK_ADDRESS_CLASS

enum {
  C166_DF_FINITE,
  C166_DF_ZERO,
  C166_DF_INFINITY,
  C166_DF_NAN,
  C166_DF_KIND_MASK = 3,
};

static inline ALWAYS_INLINE void
c166_df_store_limbs(c166_stack_word_ptr destination, c166_df_limbs value) {
  destination[0] = value.word[3];
  destination[1] = value.word[2];
  destination[2] = value.word[1];
  destination[3] = value.word[0];
}

void __c166_df_limbs_increment(c166_stack_df_limbs_ptr value);
void __c166_df_limbs_shift_right_sticky(c166_stack_df_limbs_ptr value,
                                        unsigned int count);
uint32_t __c166_df_unpack(c166_stack_df_limbs_ptr value,
                          c166_stack_const_word_ptr source);
void __c166_df_round_pack(c166_stack_word_ptr destination,
                          c166_stack_df_limbs_ptr value, int exponent,
                          uint16_t sign);

COMPILER_RT_ABI void __c166_addsubdf3(c166_stack_word_ptr destination,
                                      c166_stack_const_word_ptr left,
                                      c166_stack_const_word_ptr right,
                                      uint16_t right_sign);

#endif
