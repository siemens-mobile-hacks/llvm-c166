//===-- C166ELFObjectWriter.cpp - C166 ELF writer ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166FixupKinds.h"
#include "C166MCAsmInfo.h"
#include "C166MCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {

class C166ELFObjectWriter : public MCELFObjectTargetWriter {
public:
  explicit C166ELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(false, OSABI, ELF::EM_C166,
                                /*HasRelocationAddend=*/true) {}

protected:
  bool needsRelocateWithSymbol(const MCValue &, unsigned Type) const override {
    switch (Type) {
    case ELF::R_C166_PAG10:
    case ELF::R_C166_POF14:
    case ELF::R_C166_DPP1_16:
    case ELF::R_C166_DPP2_16:
      return true;
    default:
      return false;
    }
  }

  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override {
    switch (Fixup.getKind()) {
    case FK_Data_1:
      return ELF::R_C166_8;
    case FK_Data_2:
      if (Target.getSpecifier() == C166::S_SOF)
        return ELF::R_C166_SOF16;
      return ELF::R_C166_16;
    case FK_Data_4:
      if (Target.getSpecifier() == C166::S_PAGED32)
        return ELF::R_C166_PAGED32;
      return ELF::R_C166_32;
    case C166::fixup_c166_seg8:
      return ELF::R_C166_SEG8;
    case C166::fixup_c166_seg24:
      return ELF::R_C166_SEG24;
    case C166::fixup_c166_sof16:
      return ELF::R_C166_SOF16;
    case C166::fixup_c166_cof16:
      return ELF::R_C166_COF16;
    case C166::fixup_c166_pag10:
      return ELF::R_C166_PAG10;
    case C166::fixup_c166_pof14:
      return ELF::R_C166_POF14;
    case C166::fixup_c166_pc8:
      return ELF::R_C166_PC8;
    case C166::fixup_c166_bit_pc8:
      return ELF::R_C166_BIT_PC8;
    case C166::fixup_c166_pc8_relax:
      return ELF::R_C166_PC8_RELAX;
    case C166::fixup_c166_pc16:
      return ELF::R_C166_PC16;
    case C166::fixup_c166_dpp1_16:
      return ELF::R_C166_DPP1_16;
    case C166::fixup_c166_dpp2_16:
      return ELF::R_C166_DPP2_16;
    default:
      llvm_unreachable("invalid C166 fixup kind");
    }
  }
};

} // namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createC166ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<C166ELFObjectWriter>(OSABI);
}
