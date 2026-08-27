//===-- C166TargetParser.h - C166 target ABI constants ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGETPARSER_C166TARGETPARSER_H
#define LLVM_TARGETPARSER_C166TARGETPARSER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm::C166 {

enum class MemoryModel { Large, Medium, Small };

inline constexpr unsigned HugeCodeAddressSpace = 1;
inline constexpr unsigned FarDataAddressSpace = 2;
inline constexpr unsigned NearAddressSpace = 3;
inline constexpr unsigned XNearDataAddressSpace = 4;
inline constexpr unsigned HugeDataAddressSpace = 5;
inline constexpr unsigned SHugeDataAddressSpace = 6;

inline StringRef getDataLayout(MemoryModel Model) {
  switch (Model) {
  case MemoryModel::Large:
    return "e-m:u-P1-G2-A2-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-"
           "p4:16:16-p5:32:16:16:32-p6:32:16:16:32-i32:16-i64:16-"
           "f32:16-f64:16-a:0:16-n8:16-S16-ni:2";
  case MemoryModel::Medium:
    return "e-m:u-P3-G2-A2-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-"
           "p4:16:16-p5:32:16:16:32-p6:32:16:16:32-i32:16-i64:16-"
           "f32:16-f64:16-a:0:16-n8:16-S16-ni:2";
  case MemoryModel::Small:
    return "e-m:u-P1-G3-A3-p:16:16-p1:32:16-p2:32:16:16:32-p3:16:16-"
           "p4:16:16-p5:32:16:16:32-p6:32:16:16:32-i32:16-i64:16-"
           "f32:16-f64:16-a:0:16-n8:16-S16-ni:2";
  }
  llvm_unreachable("invalid C166 memory model");
}

// Address spaces 257 through 511 encode code banks 1 through 255.  This is
// an IR convention, not a physical processor address space.
inline constexpr unsigned CodeBankAddressSpaceBase = 256;

inline constexpr unsigned getCodeBankAddressSpace(unsigned Bank) {
  return CodeBankAddressSpaceBase + Bank;
}

inline constexpr bool isCodeBankAddressSpace(unsigned AddressSpace) {
  return AddressSpace > CodeBankAddressSpaceBase &&
         AddressSpace <= CodeBankAddressSpaceBase + 255;
}

inline constexpr unsigned getCodeBank(unsigned AddressSpace) {
  return isCodeBankAddressSpace(AddressSpace)
             ? AddressSpace - CodeBankAddressSpaceBase
             : 0;
}

} // namespace llvm::C166

#endif // LLVM_TARGETPARSER_C166TARGETPARSER_H
