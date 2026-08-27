//===-- fp64_conv.c - C166 double conversions/comparisons ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Every double operand is passed on the user stack in
// most-significant-word-first order and returns double through an eight-byte
// caller-owned block.  Keep these entry points double-typed so the C166
// frontend gives the compiler-rt implementation that same public boundary.
// fp_lib.h converts between the physical C166 object representation and
// compiler-rt's logical IEEE binary64 representation.
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"

#include "../int_lib.h"

typedef si_int fixint_t;
typedef su_int fixuint_t;
#include "../fp_compare_impl.inc"
#include "../fp_fixint_impl.inc"
#include "../fp_fixuint_impl.inc"

COMPILER_RT_ABI si_int __fixdfsi(fp_t value) { return __fixint(value); }

COMPILER_RT_ABI su_int __fixunsdfsi(fp_t value) { return __fixuint(value); }

static rep_t c166_double_unsigned(su_int value) {
  const int width = sizeof(value) * CHAR_BIT;

  if (value == 0)
    return 0;

  const int exponent = (width - 1) - clzsi(value);
  const int shift = significandBits - exponent;
  rep_t result = (rep_t)value << shift ^ implicitBit;
  return result + ((rep_t)(exponent + exponentBias) << significandBits);
}

COMPILER_RT_ABI fp_t __floatsidf(si_int value) {
  if (value >= 0)
    return fromRep(c166_double_unsigned((su_int)value));
  su_int magnitude = (su_int)value;
  magnitude = -magnitude;
  return fromRep(c166_double_unsigned(magnitude) | signBit);
}

COMPILER_RT_ABI fp_t __floatunsidf(su_int value) {
  return fromRep(c166_double_unsigned(value));
}

COMPILER_RT_ABI CMP_RESULT __ledf2(fp_t lhs, fp_t rhs) {
  return __leXf2__(lhs, rhs);
}

COMPILER_RT_ABI CMP_RESULT __eqdf2(fp_t lhs, fp_t rhs) {
  return __leXf2__(lhs, rhs);
}

COMPILER_RT_ABI CMP_RESULT __ltdf2(fp_t lhs, fp_t rhs) {
  return __leXf2__(lhs, rhs);
}

COMPILER_RT_ABI CMP_RESULT __nedf2(fp_t lhs, fp_t rhs) {
  return __leXf2__(lhs, rhs);
}

COMPILER_RT_ABI CMP_RESULT __gedf2(fp_t lhs, fp_t rhs) {
  return __geXf2__(lhs, rhs);
}

COMPILER_RT_ABI CMP_RESULT __gtdf2(fp_t lhs, fp_t rhs) {
  return __geXf2__(lhs, rhs);
}

COMPILER_RT_ABI CMP_RESULT __unorddf2(fp_t lhs, fp_t rhs) {
  return __unordXf2__(lhs, rhs);
}
