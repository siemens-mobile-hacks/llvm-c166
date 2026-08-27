//===-- fp32.c - C166 single-precision floating-point builtins -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Float operands are passed through the user stack and a result is returned
// float in R4:R5.  Reuse compiler-rt's IEEE-754 implementations, selecting
// the division variant intended for 16-bit targets.  fp_lib.h supplies the
// C166 word-wise 32x32-to-64 multiply used by multiplication and division.
//
//===----------------------------------------------------------------------===//

#define SINGLE_PRECISION
#include "../fp_add_impl.inc"
#include "../fp_mul_impl.inc"

#define NUMBER_OF_HALF_ITERATIONS 2
#define NUMBER_OF_FULL_ITERATIONS 1
#include "../fp_div_impl.inc"

COMPILER_RT_ABI rep_t __addsf3(rep_t a, rep_t b) {
  return toRep(__addXf3__(fromRep(a), fromRep(b)));
}

COMPILER_RT_ABI rep_t __subsf3(rep_t a, rep_t b) {
  // Do not let subtraction's sign flip alter a NaN payload's sign.  The
  // generic compiler-rt tests require the original operand to be quieted and
  // propagated, with the first operand taking precedence when both are NaNs.
  if ((a & absMask) > infRep)
    return a | quietBit;
  if ((b & absMask) > infRep)
    return b | quietBit;
  return toRep(__addXf3__(fromRep(a), fromRep(b ^ signBit)));
}

COMPILER_RT_ABI rep_t __mulsf3(rep_t a, rep_t b) {
  return toRep(__mulXf3__(fromRep(a), fromRep(b)));
}

COMPILER_RT_ABI rep_t __divsf3(rep_t a, rep_t b) {
  return toRep(__divXf3__(fromRep(a), fromRep(b)));
}
