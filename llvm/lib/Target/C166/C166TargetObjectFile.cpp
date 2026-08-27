//===-- C166TargetObjectFile.cpp - C166 ELF object lowering -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166TargetObjectFile.h"
#include "C166.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalObject.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/C166TargetParser.h"

using namespace llvm;

void C166TargetObjectFile::Initialize(MCContext &Ctx, const TargetMachine &TM) {
  Base::Initialize(Ctx, TM);

  constexpr unsigned AllocWrite = ELF::SHF_ALLOC | ELF::SHF_WRITE;
  NearTextSection = Ctx.getELFSection(".c166.near.text", ELF::SHT_PROGBITS,
                                      ELF::SHF_ALLOC | ELF::SHF_EXECINSTR);
  NearDataSection =
      Ctx.getELFSection(".c166.near.data", ELF::SHT_PROGBITS, AllocWrite);
  NearBSSSection =
      Ctx.getELFSection(".c166.near.bss", ELF::SHT_NOBITS, AllocWrite);
  NearReadOnlySection =
      Ctx.getELFSection(".c166.near.rodata", ELF::SHT_PROGBITS, ELF::SHF_ALLOC);
  XNearDataSection =
      Ctx.getELFSection(".c166.xnear.data", ELF::SHT_PROGBITS, AllocWrite);
  XNearBSSSection =
      Ctx.getELFSection(".c166.xnear.bss", ELF::SHT_NOBITS, AllocWrite);
  XNearReadOnlySection = Ctx.getELFSection(".c166.xnear.rodata",
                                           ELF::SHT_PROGBITS, ELF::SHF_ALLOC);

  SmallDataSection =
      Ctx.getELFSection(".c166.small.data", ELF::SHT_PROGBITS, AllocWrite);
  SmallBSSSection =
      Ctx.getELFSection(".c166.small.bss", ELF::SHT_NOBITS, AllocWrite);
  SmallReadOnlySection = Ctx.getELFSection(".c166.small.rodata",
                                           ELF::SHT_PROGBITS, ELF::SHF_ALLOC);
  SmallFarDataSection =
      Ctx.getELFSection(".c166.small.far.data", ELF::SHT_PROGBITS, AllocWrite);
  SmallFarBSSSection =
      Ctx.getELFSection(".c166.small.far.bss", ELF::SHT_NOBITS, AllocWrite);
  SmallFarReadOnlySection = Ctx.getELFSection(
      ".c166.small.far.rodata", ELF::SHT_PROGBITS, ELF::SHF_ALLOC);
  SmallHugeDataSection =
      Ctx.getELFSection(".c166.small.huge.data", ELF::SHT_PROGBITS, AllocWrite);
  SmallHugeBSSSection =
      Ctx.getELFSection(".c166.small.huge.bss", ELF::SHT_NOBITS, AllocWrite);
  SmallHugeReadOnlySection = Ctx.getELFSection(
      ".c166.small.huge.rodata", ELF::SHT_PROGBITS, ELF::SHF_ALLOC);
  SmallSHugeDataSection = Ctx.getELFSection(".c166.small.shuge.data",
                                            ELF::SHT_PROGBITS, AllocWrite);
  SmallSHugeBSSSection =
      Ctx.getELFSection(".c166.small.shuge.bss", ELF::SHT_NOBITS, AllocWrite);
  SmallSHugeReadOnlySection = Ctx.getELFSection(
      ".c166.small.shuge.rodata", ELF::SHT_PROGBITS, ELF::SHF_ALLOC);
}

MCSection *C166TargetObjectFile::SelectSectionForGlobal(
    const GlobalObject *GO, SectionKind Kind, const TargetMachine &TM) const {
  if (!GO->hasSection() && isa<Function>(GO) &&
      C166::isCodeBankAddressSpace(GO->getAddressSpace())) {
    SmallString<32> Name(".text.c166.bank.");
    Name += utostr(C166::getCodeBank(GO->getAddressSpace()));
    return getContext().getELFSection(Name, ELF::SHT_PROGBITS,
                                      ELF::SHF_ALLOC | ELF::SHF_EXECINSTR);
  }
  if (!GO->hasSection() && isa<Function>(GO) &&
      GO->getAddressSpace() == C166::NearAddressSpace)
    return NearTextSection;
  bool IsSmallData =
      TM.getCodeModel() == CodeModel::Small && !isa<Function>(GO);
  if (!GO->hasSection() && IsSmallData) {
    auto Select = [&](MCSection *Data, MCSection *BSS,
                      MCSection *ReadOnly) -> MCSection * {
      if (Kind.isBSS() || Kind.isCommon())
        return BSS;
      if (Kind.isReadOnly())
        return ReadOnly;
      return Data;
    };
    switch (GO->getAddressSpace()) {
    case C166::FarDataAddressSpace:
      return Select(SmallFarDataSection, SmallFarBSSSection,
                    SmallFarReadOnlySection);
    case C166::NearAddressSpace:
      return Select(SmallDataSection, SmallBSSSection, SmallReadOnlySection);
    case C166::HugeDataAddressSpace:
      return Select(SmallHugeDataSection, SmallHugeBSSSection,
                    SmallHugeReadOnlySection);
    case C166::SHugeDataAddressSpace:
      return Select(SmallSHugeDataSection, SmallSHugeBSSSection,
                    SmallSHugeReadOnlySection);
    default:
      break;
    }
  }
  if (!GO->hasSection() &&
      (GO->getAddressSpace() == C166::NearAddressSpace ||
       GO->getAddressSpace() == C166::XNearDataAddressSpace)) {
    bool IsNear = GO->getAddressSpace() == C166::NearAddressSpace;
    if (Kind.isBSS() || Kind.isCommon())
      return IsNear ? NearBSSSection : XNearBSSSection;
    if (Kind.isReadOnly())
      return IsNear ? NearReadOnlySection : XNearReadOnlySection;
    return IsNear ? NearDataSection : XNearDataSection;
  }
  return Base::SelectSectionForGlobal(GO, Kind, TM);
}

MCSection *
C166TargetObjectFile::getSectionForJumpTable(const Function &F,
                                             const TargetMachine &TM) const {
  if (TM.getCodeModel() == CodeModel::Small)
    return SmallReadOnlySection;
  return Base::getSectionForJumpTable(F, TM);
}
