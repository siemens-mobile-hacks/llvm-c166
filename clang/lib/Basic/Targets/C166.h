//===--- C166.h - Declare C166 target feature support ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_C166_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_C166_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/C166TargetParser.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

static constexpr LangASMap C166FarDataAddrSpaceMap = {
    {LangAS::Default, llvm::C166::FarDataAddressSpace},
};

static constexpr LangASMap C166NearDataAddrSpaceMap = {
    {LangAS::Default, llvm::C166::NearAddressSpace},
};

class LLVM_LIBRARY_VISIBILITY C166TargetInfo : public TargetInfo {
  bool IsMediumModel = false;
  bool IsSmallModel = false;

public:
  C166TargetInfo(const llvm::Triple &Triple, const TargetOptions &);

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override {
    return {};
  }

  bool hasFeature(StringRef Feature) const override {
    return Feature == "c166";
  }

  bool isValidCPUName(StringRef Name) const override {
    return Name == "c166" || Name == "generic";
  }

  void fillValidCPUList(SmallVectorImpl<StringRef> &Values) const override {
    Values.emplace_back("c166");
    Values.emplace_back("generic");
  }

  bool setCPU(StringRef Name) override { return isValidCPUName(Name); }

  ArrayRef<const char *> getGCCRegNames() const override;

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    return {};
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override;

  std::string_view getClobbers() const override { return ""; }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::CharPtrBuiltinVaList;
  }

  unsigned getMinRecordAlign() const override { return 16; }

  unsigned getMinPackedRecordFieldAlign() const override { return 16; }

  unsigned getPackedBitFieldAccessUnitWidth() const override { return 16; }

  bool supportsNonStandardCBitFieldTypes() const override { return false; }

  bool allowsLargerPreferedTypeAlignment() const override { return false; }

  bool supportsFixedSizeVectorTypes() const override { return false; }

  bool supportsBuiltinAlloca() const override { return false; }

  CallingConvCheckResult checkCallingConvention(CallingConv CC) const override {
    return CC == CC_C || CC == CC_C166StackParm ? CCCR_OK : CCCR_Warning;
  }

  bool hasBitIntType() const override { return MaxBitIntWidth.has_value(); }

  uint64_t getPointerWidthV(LangAS AddrSpace) const override {
    unsigned TargetAS = getTargetAddressSpace(AddrSpace);
    return TargetAS == llvm::C166::NearAddressSpace ||
                   TargetAS == llvm::C166::XNearDataAddressSpace
               ? 16
               : 32;
  }

  uint64_t getPointerAlignV(LangAS) const override { return 16; }

  uint64_t getMaxPointerWidth() const override { return 32; }

  std::optional<LangAS> getDefaultFunctionAddressSpace() const override {
    return getLangASFromTargetAS(IsMediumModel
                                     ? llvm::C166::NearAddressSpace
                                     : llvm::C166::HugeCodeAddressSpace);
  }

  IntType getPtrDiffTypeV(LangAS AddrSpace) const override {
    unsigned TargetAS = getTargetAddressSpace(AddrSpace);
    return TargetAS == llvm::C166::HugeDataAddressSpace ||
                   TargetAS == llvm::C166::SHugeDataAddressSpace
               ? SignedLong
               : SignedInt;
  }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_C166_H
