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
#include <optional>

namespace llvm::C166 {

enum class MemoryModel { Tiny, Small, Medium, Large, Huge };

inline std::optional<MemoryModel> parseMemoryModel(StringRef Name) {
  if (Name.equals_insensitive("tiny"))
    return MemoryModel::Tiny;
  if (Name.equals_insensitive("small"))
    return MemoryModel::Small;
  if (Name.equals_insensitive("medium"))
    return MemoryModel::Medium;
  if (Name.equals_insensitive("large"))
    return MemoryModel::Large;
  if (Name.equals_insensitive("huge"))
    return MemoryModel::Huge;
  return std::nullopt;
}

inline MemoryModel getMemoryModel(StringRef ABIName, StringRef CodeModel) {
  if (std::optional Model = parseMemoryModel(ABIName))
    return *Model;
  if (std::optional Model = parseMemoryModel(CodeModel))
    return *Model;
  return MemoryModel::Large;
}

inline StringRef getMemoryModelName(MemoryModel Model) {
  switch (Model) {
  case MemoryModel::Tiny:
    return "tiny";
  case MemoryModel::Small:
    return "small";
  case MemoryModel::Medium:
    return "medium";
  case MemoryModel::Large:
    return "large";
  case MemoryModel::Huge:
    return "huge";
  }
  llvm_unreachable("invalid C166 memory model");
}

inline constexpr bool hasNearCode(MemoryModel Model) {
  return Model == MemoryModel::Tiny || Model == MemoryModel::Medium;
}

inline constexpr bool hasNearData(MemoryModel Model) {
  return Model == MemoryModel::Tiny || Model == MemoryModel::Small;
}

inline constexpr unsigned HugeCodeAddressSpace = 1;
inline constexpr unsigned FarDataAddressSpace = 2;
inline constexpr unsigned NearAddressSpace = 3;
inline constexpr unsigned XNearDataAddressSpace = 4;
inline constexpr unsigned HugeDataAddressSpace = 5;
inline constexpr unsigned SHugeDataAddressSpace = 6;
inline constexpr unsigned SFRAddressSpace = 7;
inline constexpr unsigned ESFRAddressSpace = 8;
inline constexpr StringLiteral SFRBitfieldMetadataName = "c166.sfr.bitfield";
inline constexpr StringLiteral SFRBitMetadataName = "c166.sfr.bit";

inline StringRef getDataLayout(MemoryModel Model) {
  switch (Model) {
  case MemoryModel::Tiny:
    return "e-m:u-P3-G3-A3-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-"
           "p4:16:16-p5:32:16:16:32-p6:32:16:16:32-p7:16:16-p8:16:16-"
           "i32:16-i64:16-"
           "f32:16-f64:16-a:0:16-n8:16-S16-ni:2";
  case MemoryModel::Large:
    return "e-m:u-P1-G2-A2-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-"
           "p4:16:16-p5:32:16:16:32-p6:32:16:16:32-p7:16:16-p8:16:16-"
           "i32:16-i64:16-"
           "f32:16-f64:16-a:0:16-n8:16-S16-ni:2";
  case MemoryModel::Medium:
    return "e-m:u-P3-G2-A2-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-"
           "p4:16:16-p5:32:16:16:32-p6:32:16:16:32-p7:16:16-p8:16:16-"
           "i32:16-i64:16-"
           "f32:16-f64:16-a:0:16-n8:16-S16-ni:2";
  case MemoryModel::Small:
    return "e-m:u-P1-G3-A3-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-"
           "p4:16:16-p5:32:16:16:32-p6:32:16:16:32-p7:16:16-p8:16:16-"
           "i32:16-i64:16-"
           "f32:16-f64:16-a:0:16-n8:16-S16-ni:2";
  case MemoryModel::Huge:
    return "e-m:u-P1-G5-A5-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-"
           "p4:16:16-p5:32:16:16:32-p6:32:16:16:32-p7:16:16-p8:16:16-"
           "i32:16-i64:16-"
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
