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
    : TargetInfo(Triple),
      MemoryModel(llvm::C166::getMemoryModel(Opts.ABI, Opts.CodeModel)) {
  TLSSupported = false;
  HasMustTail = false;
  UserLabelPrefix = "_";
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

  PointerAlign = 16;
  SuitableAlign = 16;
  DefaultAlignForAttributeAligned = 16;
  NewAlign = 16;

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
  setMemoryModel(MemoryModel);
}

void C166TargetInfo::setMemoryModel(llvm::C166::MemoryModel Model) {
  MemoryModel = Model;
  if (Model == llvm::C166::MemoryModel::Huge)
    AddrSpaceMap = &C166HugeDataAddrSpaceMap;
  else if (llvm::C166::hasNearData(Model))
    AddrSpaceMap = &C166NearDataAddrSpaceMap;
  else
    AddrSpaceMap = &C166FarDataAddrSpaceMap;

  PointerWidth = llvm::C166::hasNearData(Model) ? 16 : 32;
  SizeType = UnsignedInt;
  PtrDiffType = Model == llvm::C166::MemoryModel::Huge ? SignedLong : SignedInt;
  IntPtrType = llvm::C166::hasNearData(Model) ? SignedInt : SignedLong;
  resetDataLayout(llvm::C166::getDataLayout(Model));
}

bool C166TargetInfo::setABI(const std::string &Name) {
  std::optional Model = llvm::C166::parseMemoryModel(Name);
  if (!Model)
    return false;
  setMemoryModel(*Model);
  return true;
}

void C166TargetInfo::getTargetDefines(const LangOptions &Opts,
                                      MacroBuilder &Builder) const {
  Builder.defineMacro("__c166__");
  Builder.defineMacro("__C166__");
  Builder.defineMacro("__near", "__attribute__((c166_near))");
  Builder.defineMacro("__xnear", "__attribute__((c166_xnear))");
  Builder.defineMacro("__far", "__attribute__((c166_far))");
  Builder.defineMacro("__huge", "__attribute__((c166_huge))");
  Builder.defineMacro("__shuge", "__attribute__((c166_shuge))");
  Builder.defineMacro("__sfr", "__attribute__((c166_sfr))");
  Builder.defineMacro("__esfr", "__attribute__((c166_esfr))");

  StringRef ModelValue;
  switch (MemoryModel) {
  case llvm::C166::MemoryModel::Large:
    ModelValue = "1";
    break;
  case llvm::C166::MemoryModel::Medium:
    ModelValue = "2";
    break;
  case llvm::C166::MemoryModel::Small:
    ModelValue = "3";
    break;
  case llvm::C166::MemoryModel::Tiny:
    ModelValue = "4";
    break;
  case llvm::C166::MemoryModel::Huge:
    ModelValue = "5";
    break;
  }
  Builder.defineMacro("__C166_MEMORY_MODEL__", ModelValue);
}

llvm::SmallVector<Builtin::InfosShard>
C166TargetInfo::getTargetBuiltins() const {
  return {{&BuiltinStrings, BuiltinInfos}};
}

ArrayRef<const char *> C166TargetInfo::getGCCRegNames() const {
  static const char *const GCCRegNames[] = {
      "r0", "r1", "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
      "mdl", "mdh",
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
