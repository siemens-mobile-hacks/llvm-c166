//===-- fp_width.c - C166 float/double width conversions ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The C166 ABI stores floating-point objects most-significant-word first,
// while integer objects are stored least-significant-word first.  The generic
// compiler-rt width conversions deliberately assume that both types have the
// same byte order, so keep the IEEE manipulation in a C166-local translation
// unit and cross the typed boundaries with bit_cast.
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"

#include "../int_lib.h"

static __inline su_int c166_single_to_rep(float value) {
  // bit_cast observes the C166 object representation: the most-significant
  // floating word is stored first, while the integer result is interpreted
  // least-significant-word first.  Restore the logical IEEE bit numbering used
  // by the conversion algorithm.
  const su_int physical = __builtin_bit_cast(su_int, value);
  return physical << 16 | physical >> 16;
}

COMPILER_RT_ABI double __extendsfdf2(float value) {
  const su_int source = c166_single_to_rep(value);
  const su_int sourceSign = source >> 31;
  const su_int sourceExponent = (source >> 23) & UINT32_C(0xff);
  const su_int sourceFraction = source & UINT32_C(0x007fffff);

  rep_t destinationExponent;
  rep_t destinationFraction;

  if (sourceExponent >= 1 && sourceExponent < 0xff) {
    destinationExponent = (rep_t)sourceExponent + (1023 - 127);
    destinationFraction = (rep_t)sourceFraction << (52 - 23);
  } else if (sourceExponent == 0xff) {
    destinationExponent = 0x7ff;
    destinationFraction = (rep_t)sourceFraction << (52 - 23);
  } else if (sourceFraction != 0) {
    const int scale = clzsi(sourceFraction) - 8;
    destinationExponent = 1023 - 127 - scale + 1;
    destinationFraction = (rep_t)sourceFraction << (52 - 23 + scale);
    destinationFraction ^= REP_C(1) << 52;
  } else {
    destinationExponent = 0;
    destinationFraction = 0;
  }

  const rep_t destination =
      (rep_t)sourceSign << 63 | destinationExponent << 52 | destinationFraction;
  return fromRep(destination);
}

// SelectionDAG softens the result of FPROUND to its i32 representation before
// making this libcall.  Consequently this compiler-private entry point returns
// the logical binary32 bits in the integer R4:R5 convention; a public C float
// return is converted to the MSW-first R4:R5 convention by its caller.
COMPILER_RT_ABI su_int __truncdfsf2(double value) {
  const rep_t source = toRep(value);
  const rep_t sourceSign = source >> 63;
  const rep_t sourceExponent = source >> 52 & REP_C(0x7ff);
  const rep_t sourceFraction = source & ((REP_C(1) << 52) - 1);

  const rep_t sourceMinNormal = REP_C(1) << 52;
  const rep_t roundMask = (REP_C(1) << (52 - 23)) - 1;
  const rep_t halfway = REP_C(1) << (52 - 23 - 1);
  const rep_t sourceQNaN = REP_C(1) << 51;
  const rep_t sourceNaNCode = sourceQNaN - 1;
  const su_int destinationQNaN = UINT32_C(1) << 22;
  const su_int destinationNaNCode = destinationQNaN - 1;

  su_int destinationExponent;
  su_int destinationFraction;
  const int destinationExponentCandidate = (int)sourceExponent - 1023 + 127;

  if (destinationExponentCandidate >= 1 &&
      destinationExponentCandidate < 0xff) {
    destinationExponent = destinationExponentCandidate;
    destinationFraction = (su_int)(sourceFraction >> (52 - 23));

    const rep_t roundBits = sourceFraction & roundMask;
    if (roundBits > halfway)
      ++destinationFraction;
    else if (roundBits == halfway)
      destinationFraction += destinationFraction & 1;

    if (destinationFraction >= (UINT32_C(1) << 23)) {
      ++destinationExponent;
      destinationFraction ^= UINT32_C(1) << 23;
    }
  } else if (sourceExponent == 0x7ff && sourceFraction != 0) {
    destinationExponent = 0xff;
    destinationFraction = destinationQNaN;
    destinationFraction |=
        ((sourceFraction & sourceNaNCode) >> (52 - 23)) & destinationNaNCode;
  } else if ((int)sourceExponent >= 1023 + 0xff - 127) {
    destinationExponent = 0xff;
    destinationFraction = 0;
  } else {
    rep_t significand = sourceFraction;
    int shift = 1023 - 127 - (int)sourceExponent;

    if (sourceExponent != 0) {
      significand |= sourceMinNormal;
      ++shift;
    }

    destinationExponent = 0;
    if (shift > 52) {
      destinationFraction = 0;
    } else {
      const bool sticky = shift && (significand << (64 - shift)) != 0;
      const rep_t denormalized = significand >> shift | sticky;
      destinationFraction = (su_int)(denormalized >> (52 - 23));
      const rep_t roundBits = denormalized & roundMask;
      if (roundBits > halfway)
        ++destinationFraction;
      else if (roundBits == halfway)
        destinationFraction += destinationFraction & 1;

      if (destinationFraction >= (UINT32_C(1) << 23)) {
        ++destinationExponent;
        destinationFraction ^= UINT32_C(1) << 23;
      }
    }
  }

  const su_int destination = (su_int)sourceSign << 31 |
                             destinationExponent << 23 | destinationFraction;
  return destination;
}
