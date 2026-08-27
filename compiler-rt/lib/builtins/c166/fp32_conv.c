//===-- fp32_conv.c - C166 single-precision conversions/comparisons -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// SelectionDAG softens an f32 libcall operand to its raw i32 representation.
// Keep these internal entry points integer-typed so their R12-R15/R4:R5
// boundary agrees with the lowered call.  Public C functions continue to use
// the stack-only, most-significant-word-first float ABI.
//
//===----------------------------------------------------------------------===//

#define SINGLE_PRECISION
#include "../fp_lib.h"

#include "../int_lib.h"

typedef si_int fixint_t;
typedef su_int fixuint_t;
#include "../fp_compare_impl.inc"
#include "../fp_fixint_impl.inc"
#include "../fp_fixuint_impl.inc"

COMPILER_RT_ABI si_int __fixsfsi(rep_t value) {
  return __fixint(fromRep(value));
}

COMPILER_RT_ABI su_int __fixunssfsi(rep_t value) {
  return __fixuint(fromRep(value));
}

static rep_t c166_float_unsigned(su_int value) {
  const int width = sizeof(value) * CHAR_BIT;

  if (value == 0)
    return 0;

  const int exponent = (width - 1) - clzsi(value);
  rep_t result;
  if (exponent <= significandBits) {
    const int shift = significandBits - exponent;
    result = (rep_t)value << shift ^ implicitBit;
  } else {
    const int shift = exponent - significandBits;
    result = (rep_t)value >> shift ^ implicitBit;
    rep_t round = (rep_t)value << (typeWidth - shift);
    if (round > signBit || (round == signBit && (result & 1)))
      ++result;
  }

  return result + ((rep_t)(exponent + exponentBias) << significandBits);
}

COMPILER_RT_ABI rep_t __floatsisf(si_int value) {
  if (value >= 0)
    return c166_float_unsigned((su_int)value);
  su_int magnitude = (su_int)value;
  magnitude = -magnitude;
  return c166_float_unsigned(magnitude) | signBit;
}

COMPILER_RT_ABI rep_t __floatunsisf(su_int value) {
  return c166_float_unsigned(value);
}

COMPILER_RT_ABI CMP_RESULT __lesf2(rep_t lhs, rep_t rhs) {
  return __leXf2__(fromRep(lhs), fromRep(rhs));
}

COMPILER_RT_ABI CMP_RESULT __eqsf2(rep_t lhs, rep_t rhs) {
  return __leXf2__(fromRep(lhs), fromRep(rhs));
}

COMPILER_RT_ABI CMP_RESULT __ltsf2(rep_t lhs, rep_t rhs) {
  return __leXf2__(fromRep(lhs), fromRep(rhs));
}

COMPILER_RT_ABI CMP_RESULT __nesf2(rep_t lhs, rep_t rhs) {
  return __leXf2__(fromRep(lhs), fromRep(rhs));
}

COMPILER_RT_ABI CMP_RESULT __gesf2(rep_t lhs, rep_t rhs) {
  return __geXf2__(fromRep(lhs), fromRep(rhs));
}

COMPILER_RT_ABI CMP_RESULT __gtsf2(rep_t lhs, rep_t rhs) {
  return __geXf2__(fromRep(lhs), fromRep(rhs));
}

COMPILER_RT_ABI CMP_RESULT __unordsf2(rep_t lhs, rep_t rhs) {
  return __unordXf2__(fromRep(lhs), fromRep(rhs));
}
