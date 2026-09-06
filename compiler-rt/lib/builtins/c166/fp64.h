//===-- fp64.h - C166 binary64 runtime helpers -------------------*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef COMPILER_RT_LIB_BUILTINS_C166_FP64_H
#define COMPILER_RT_LIB_BUILTINS_C166_FP64_H

#if __C166_MEMORY_MODEL__ == 3
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
// and the default near/DPP2 address class in Small.
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

static inline ALWAYS_INLINE c166_df_limbs
c166_df_load_limbs(c166_stack_const_word_ptr value) {
  c166_df_limbs result = {{value[3], value[2], value[1], value[0]}};
  return result;
}

static inline ALWAYS_INLINE void
c166_df_store_limbs(c166_stack_word_ptr destination, c166_df_limbs value) {
  destination[0] = value.word[3];
  destination[1] = value.word[2];
  destination[2] = value.word[1];
  destination[3] = value.word[0];
}

static inline ALWAYS_INLINE bool
c166_df_limbs_is_zero(const c166_df_limbs *value) {
  return (value->word[0] | value->word[1] | value->word[2] | value->word[3]) ==
         0;
}

static inline ALWAYS_INLINE void
c166_df_limbs_shift_right_three(c166_df_limbs *value) {
  value->word[0] = (value->word[0] >> 3) | (uint16_t)(value->word[1] << 13);
  value->word[1] = (value->word[1] >> 3) | (uint16_t)(value->word[2] << 13);
  value->word[2] = (value->word[2] >> 3) | (uint16_t)(value->word[3] << 13);
  value->word[3] >>= 3;
}

unsigned int __c166_df_limbs_normalize(c166_stack_df_limbs_ptr value,
                                       uint16_t top_bit);
void __c166_df_limbs_increment(c166_stack_df_limbs_ptr value);
void __c166_df_limbs_shift_right_sticky(c166_stack_df_limbs_ptr value,
                                        unsigned int count);
uint32_t __c166_df_unpack(c166_stack_df_limbs_ptr value,
                          c166_stack_const_word_ptr source);
void __c166_df_round_pack(c166_stack_word_ptr destination,
                          c166_stack_df_limbs_ptr value, int exponent,
                          uint16_t sign);

typedef struct {
  uint32_t low;
  uint32_t high;
} c166_df_words;

static inline ALWAYS_INLINE c166_df_words
c166_df_load(c166_stack_const_word_ptr value) {
  c166_df_words result;
  result.high = (uint32_t)value[0] << 16 | value[1];
  result.low = (uint32_t)value[2] << 16 | value[3];
  return result;
}

static inline ALWAYS_INLINE void c166_df_store(c166_stack_word_ptr destination,
                                               c166_df_words value) {
  destination[0] = (uint16_t)(value.high >> 16);
  destination[1] = (uint16_t)value.high;
  destination[2] = (uint16_t)(value.low >> 16);
  destination[3] = (uint16_t)value.low;
}

static inline ALWAYS_INLINE bool c166_df_is_zero(c166_df_words value) {
  return (value.high | value.low) == 0;
}

static inline ALWAYS_INLINE int c166_df_compare(c166_df_words left,
                                                c166_df_words right) {
  if (left.high != right.high)
    return left.high > right.high ? 1 : -1;
  if (left.low != right.low)
    return left.low > right.low ? 1 : -1;
  return 0;
}

static inline ALWAYS_INLINE c166_df_words c166_df_sub(c166_df_words left,
                                                      c166_df_words right) {
  c166_df_words result;
  result.low = left.low - right.low;
  result.high = left.high - right.high - (left.low < right.low);
  return result;
}

static inline ALWAYS_INLINE c166_df_words
c166_df_shift_left_one(c166_df_words value) {
  value.high = value.high << 1 | value.low >> 31;
  value.low <<= 1;
  return value;
}

static inline ALWAYS_INLINE c166_df_words
c166_df_shift_left(c166_df_words value, unsigned int count) {
  if (count == 0)
    return value;
  if (count < 32) {
    value.high = value.high << count | value.low >> (32 - count);
    value.low <<= count;
  } else if (count < 64) {
    value.high = value.low << (count - 32);
    value.low = 0;
  } else {
    value.high = 0;
    value.low = 0;
  }
  return value;
}

static inline ALWAYS_INLINE unsigned int c166_df_clz(c166_df_words value) {
  if (value.high != 0)
    return __builtin_clzl(value.high);
  return 32 + __builtin_clzl(value.low);
}

static inline ALWAYS_INLINE int c166_df_normalize(c166_df_words *significand) {
  unsigned int shift = c166_df_clz(*significand) - 11;
  *significand = c166_df_shift_left(*significand, shift);
  return shift;
}

static inline ALWAYS_INLINE c166_df_words
c166_df_increment(c166_df_words value) {
  ++value.low;
  if (value.low == 0)
    ++value.high;
  return value;
}

COMPILER_RT_ABI void __c166_addsubdf3(c166_stack_word_ptr destination,
                                      c166_stack_const_word_ptr left,
                                      c166_stack_const_word_ptr right,
                                      uint16_t right_sign);

#endif
