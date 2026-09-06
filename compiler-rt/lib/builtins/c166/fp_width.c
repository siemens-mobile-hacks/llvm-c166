//===-- fp_width.c - C166 float/double width conversions ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The C166 ABI stores floating-point objects most-significant-word first,
// while integer objects are stored least-significant-word first.  The target
// translates typed floating-point loads and stores to LLVM's logical IEEE
// representation, so keep the conversion arithmetic in that representation.
//
//===----------------------------------------------------------------------===//

#define DOUBLE_PRECISION
#include "../fp_lib.h"

#include "../int_lib.h"

typedef union {
  rep_t all;
  uint16_t word[4];
} c166_width_rep;

enum {
  binary32ExponentBias = 127,
  binary64ExponentBias = 1023,
  exponentBiasDelta = binary64ExponentBias - binary32ExponentBias,
  binary32NormalSourceExponentMin = exponentBiasDelta + 1,
  binary32NormalSourceExponentMax = exponentBiasDelta + 0xfe,
  binary32OverflowSourceExponent = exponentBiasDelta + 0xff,
  binary32HalfMinSubnormalSourceExponent = binary64ExponentBias - 150,
  binary32SubnormalShiftBase = binary64ExponentBias + 52 - 149,
};

COMPILER_RT_ABI double __extendsfdf2(float value) {
  const su_int source = __builtin_bit_cast(su_int, value);
  const uint16_t sourceHigh = (uint16_t)(source >> 16);
  const uint16_t sourceSign = sourceHigh & UINT16_C(0x8000);
  const uint16_t sourceExponent = (sourceHigh >> 7) & UINT16_C(0xff);
  uint16_t sourceFractionLow = (uint16_t)source;
  uint16_t sourceFractionHigh = sourceHigh & UINT16_C(0x7f);

  uint16_t destinationExponent;

  if (sourceExponent >= 1 && sourceExponent < 0xff) {
    destinationExponent = sourceExponent + exponentBiasDelta;
  } else if (sourceExponent == 0xff) {
    destinationExponent = 0x7ff;
  } else if ((sourceFractionHigh | sourceFractionLow) != 0) {
    const int scale = sourceFractionHigh != 0
                          ? __builtin_clz((unsigned int)sourceFractionHigh) - 8
                          : __builtin_clz((unsigned int)sourceFractionLow) + 8;
    destinationExponent = exponentBiasDelta - scale + 1;
    if (scale < 16) {
      sourceFractionHigh =
          sourceFractionHigh << scale | sourceFractionLow >> (16 - scale);
      sourceFractionLow <<= scale;
    } else {
      sourceFractionHigh = sourceFractionLow << (scale - 16);
      sourceFractionLow = 0;
    }
    // The packing expressions below discard the normalized implicit bit.
  } else {
    destinationExponent = 0;
  }

  const c166_width_rep destination = {
      .word = {0, (uint16_t)(sourceFractionLow << 13),
               (uint16_t)(sourceFractionLow >> 3 | sourceFractionHigh << 13),
               (uint16_t)(sourceSign | destinationExponent << 4 |
                          (sourceFractionHigh >> 3 & 0xf))}};
  return fromRep(destination.all);
}

// SelectionDAG softens the result of FPROUND to its i32 representation before
// making this libcall.  Consequently this compiler-private entry point returns
// the logical binary32 bits in the integer R4:R5 convention; a public C float
// return is converted to the MSW-first R4:R5 convention by its caller.
COMPILER_RT_ABI su_int __truncdfsf2(double value) {
  const c166_width_rep source = {.all = toRep(value)};
  const uint16_t sourceSign = source.word[3] & UINT16_C(0x8000);
  const uint16_t sourceExponent = source.word[3] >> 4 & UINT16_C(0x7ff);
  const bool sourceFractionIsZero = ((source.word[3] & 0xf) | source.word[2] |
                                     source.word[1] | source.word[0]) == 0;

  uint16_t destinationExponent;
  uint16_t destinationFractionLow;
  uint16_t destinationFractionHigh;

  if (sourceExponent >= binary32NormalSourceExponentMin &&
      sourceExponent <= binary32NormalSourceExponentMax) {
    destinationExponent = sourceExponent - exponentBiasDelta;
    destinationFractionLow = source.word[2] << 3 | source.word[1] >> 13;
    destinationFractionHigh =
        (source.word[3] & 0xf) << 3 | source.word[2] >> 13;

    const uint16_t roundHigh = source.word[1];
    if ((roundHigh & UINT16_C(0x1000)) != 0 &&
        ((roundHigh & UINT16_C(0x0fff)) != 0 || source.word[0] != 0 ||
         (destinationFractionLow & 1) != 0)) {
      ++destinationFractionLow;
      if (destinationFractionLow == 0)
        ++destinationFractionHigh;
    }

    if (destinationFractionHigh & UINT16_C(0x80)) {
      ++destinationExponent;
      destinationFractionHigh ^= UINT16_C(0x80);
    }
  } else if (sourceExponent == 0x7ff && !sourceFractionIsZero) {
    destinationExponent = 0xff;
    destinationFractionLow = source.word[2] << 3 | source.word[1] >> 13;
    destinationFractionHigh =
        UINT16_C(0x40) | (source.word[3] & 0x7) << 3 | source.word[2] >> 13;
  } else if (sourceExponent >= binary32OverflowSourceExponent) {
    destinationExponent = 0xff;
    destinationFractionLow = 0;
    destinationFractionHigh = 0;
  } else if (sourceExponent < binary32HalfMinSubnormalSourceExponent) {
    destinationExponent = 0;
    destinationFractionLow = 0;
    destinationFractionHigh = 0;
  } else {
    unsigned int shift = binary32SubnormalShiftBase - sourceExponent;
    uint16_t significand0 = source.word[0];
    uint16_t significand1 = source.word[1];
    uint16_t significand2 = source.word[2];
    uint16_t significand3 = (source.word[3] & 0xf) | 0x10;
    bool guard = false;
    bool sticky = false;
    do {
      sticky |= guard;
      guard = (significand0 & 1) != 0;
      significand0 = (significand0 >> 1) | (uint16_t)(significand1 << 15);
      significand1 = (significand1 >> 1) | (uint16_t)(significand2 << 15);
      significand2 = (significand2 >> 1) | (uint16_t)(significand3 << 15);
      significand3 >>= 1;
    } while (--shift != 0);

    destinationFractionLow = significand0;
    destinationFractionHigh = significand1;
    destinationExponent = 0;
    uint16_t roundUp = guard && (sticky || (destinationFractionLow & 1));
    uint16_t unroundedLow = destinationFractionLow;
    destinationFractionLow += roundUp;
    destinationFractionHigh += destinationFractionLow < unroundedLow;
    if (destinationFractionHigh & UINT16_C(0x80)) {
      ++destinationExponent;
      destinationFractionHigh ^= UINT16_C(0x80);
    }
  }

  const uint16_t destinationHigh =
      sourceSign | destinationExponent << 7 | destinationFractionHigh;
  return (su_int)destinationHigh << 16 | destinationFractionLow;
}
