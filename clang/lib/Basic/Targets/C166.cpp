//===--- C166.cpp - Implement C166 target feature support ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/TargetParser/C166TargetParser.h"

using namespace clang;
using namespace clang::targets;

static constexpr int NumBuiltins =
    C166::LastTSBuiltin - Builtin::FirstTSBuiltin;

static constexpr llvm::StringTable BuiltinStrings =
    CLANG_BUILTIN_STR_TABLE_START
#define BUILTIN CLANG_BUILTIN_STR_TABLE
#include "clang/Basic/BuiltinsC166.def"
    ;

static constexpr auto BuiltinInfos = Builtin::MakeInfos<NumBuiltins>({
#define BUILTIN CLANG_BUILTIN_ENTRY
#include "clang/Basic/BuiltinsC166.def"
});

C166TargetInfo::C166TargetInfo(const llvm::Triple &Triple,
                               const TargetOptions &Opts)
    : TargetInfo(Triple), IsMediumModel(Opts.CodeModel == "medium"),
      IsSmallModel(Opts.CodeModel == "small") {
  TLSSupported = false;
  // The backend has no dynamic user-stack adjustment.
  VLASupported = false;
  HasMustTail = false;
  UserLabelPrefix = "_";
  AddrSpaceMap =
      IsSmallModel ? &C166NearDataAddrSpaceMap : &C166FarDataAddrSpaceMap;
  UseAddrSpaceMapMangling = true;

  BoolWidth = BoolAlign = 8;
  ShortWidth = ShortAlign = 16;
  IntWidth = IntAlign = 16;
  LongWidth = 32;
  LongAlign = 16;
  // No C166 ABI integer type is wider than 32 bits.
  LongLongWidth = 32;
  LongLongAlign = 16;

  HalfWidth = HalfAlign = 16;
  FloatWidth = 32;
  FloatAlign = 16;
  DoubleWidth = LongDoubleWidth = 64;
  DoubleAlign = LongDoubleAlign = 16;

  PointerWidth = IsSmallModel ? 16 : 32;
  PointerAlign = 16;
  SuitableAlign = 16;
  DefaultAlignForAttributeAligned = 16;
  NewAlign = 16;

  SizeType = UnsignedInt;
  PtrDiffType = SignedInt;
  IntPtrType = IsSmallModel ? SignedInt : SignedLong;
  IntMaxType = SignedLong;
  WCharType = SignedInt;
  WIntType = SignedInt;
  Char16Type = UnsignedInt;
  Char32Type = UnsignedLong;
  Int16Type = SignedInt;
  Int64Type = SignedLongLong;
  SigAtomicType = SignedInt;

  MaxAtomicPromoteWidth = 0;
  MaxAtomicInlineWidth = 0;
  llvm::C166::MemoryModel Model = IsSmallModel ? llvm::C166::MemoryModel::Small
                                  : IsMediumModel
                                      ? llvm::C166::MemoryModel::Medium
                                      : llvm::C166::MemoryModel::Large;
  resetDataLayout(llvm::C166::getDataLayout(Model));
}

void C166TargetInfo::getTargetDefines(const LangOptions &Opts,
                                      MacroBuilder &Builder) const {
  Builder.defineMacro("__c166__");
  Builder.defineMacro("__C166__");

  Builder.defineMacro("__C166_MEMORY_MODEL__", IsMediumModel  ? "2"
                                               : IsSmallModel ? "3"
                                                              : "1");
}

llvm::SmallVector<Builtin::InfosShard>
C166TargetInfo::getTargetBuiltins() const {
  return {{&BuiltinStrings, BuiltinInfos}};
}

ArrayRef<const char *> C166TargetInfo::getGCCRegNames() const {
  static const char *const GCCRegNames[] = {
      "r0", "r1", "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  };
  return llvm::ArrayRef(GCCRegNames);
}

bool C166TargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  switch (*Name) {
  default:
    return false;
  case 'r':
    Info.setAllowsRegister();
    return true;
  case 'I':
    Info.setRequiresImmediate(0, 15);
    return true;
  }
}
