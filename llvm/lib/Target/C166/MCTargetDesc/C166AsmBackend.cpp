//===-- C166AsmBackend.cpp - C166 assembler backend ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166FixupKinds.h"
#include "C166MCAsmInfo.h"
#include "C166MCTargetDesc.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

static unsigned getJMPREncoding(unsigned Opcode) {
  switch (Opcode) {
  case C166::JMPR_UC:
    return 0x0d;
  case C166::JMPR_EQ:
    return 0x2d;
  case C166::JMPR_NE:
    return 0x3d;
  case C166::JMPR_ULT:
    return 0x8d;
  case C166::JMPR_UGE:
    return 0x9d;
  case C166::JMPR_SGT:
    return 0xad;
  case C166::JMPR_SLE:
    return 0xbd;
  case C166::JMPR_SLT:
    return 0xcd;
  case C166::JMPR_SGE:
    return 0xdd;
  case C166::JMPR_UGT:
    return 0xed;
  case C166::JMPR_ULE:
    return 0xfd;
  default:
    llvm_unreachable("not a C166 relative branch");
  }
}

class C166AsmBackend : public MCAsmBackend {
  uint64_t adjustFixupValue(const MCFixup &Fixup, const MCValue &Target,
                            uint64_t Value) const {
    switch (Fixup.getKind()) {
    case FK_Data_1:
    case FK_Data_2:
    case FK_Data_4:
      if (Target.getSpecifier() == C166::S_PAGED32) {
        if (Value > 0xffffff)
          getContext().reportError(Fixup.getLoc(),
                                   "far data address exceeds 24 bits");
        return (Value & 0x3fff) | (((Value >> 14) & 0x3ff) << 16);
      }
      return Value;
    case C166::fixup_c166_seg8:
      if (Value > 0xffffff)
        getContext().reportError(Fixup.getLoc(),
                                 "code address exceeds 24 bits");
      return (Value >> 16) & 0xff;
    case C166::fixup_c166_seg24:
      if (Value > 0xffffff)
        getContext().reportError(Fixup.getLoc(),
                                 "code address exceeds 24 bits");
      return ((Value & 0xffff) << 8) | ((Value >> 16) & 0xff);
    case C166::fixup_c166_sof16:
      return Value & 0xffff;
    case C166::fixup_c166_cof16:
      return Value & 0xffff;
    case C166::fixup_c166_pag10:
      if (Value > 0xffffff)
        getContext().reportError(Fixup.getLoc(),
                                 "far data address exceeds 24 bits");
      return (Value >> 14) & 0x3ff;
    case C166::fixup_c166_pof14:
      return Value & 0x3fff;
    case C166::fixup_c166_dpp1_16:
      return 0x4000 | (Value & 0x3fff);
    case C166::fixup_c166_dpp2_16:
      return 0x8000 | (Value & 0x3fff);
    case C166::fixup_c166_pc8: {
      int64_t Delta = static_cast<int64_t>(Value) - 1;
      if (Delta & 1)
        getContext().reportError(Fixup.getLoc(),
                                 "relative branch target is not word-aligned");
      int64_t Words = Delta / 2;
      if (!isInt<8>(Words))
        getContext().reportError(Fixup.getLoc(),
                                 "relative branch is out of range");
      return static_cast<uint8_t>(Words);
    }
    case C166::fixup_c166_pc8_relax:
      llvm_unreachable("relaxable C166 branch fixup must remain unresolved");
    case C166::fixup_c166_pc16:
      return Value & 0xffff;
    default:
      llvm_unreachable("unknown C166 fixup kind");
    }
  }

public:
  C166AsmBackend() : MCAsmBackend(endianness::little) {}

  bool mayNeedRelaxation(unsigned Opcode, ArrayRef<MCOperand>,
                         const MCSubtargetInfo &) const override {
    switch (Opcode) {
    case C166::JMPR_UC:
    case C166::JMPR_EQ:
    case C166::JMPR_NE:
    case C166::JMPR_ULT:
    case C166::JMPR_UGE:
    case C166::JMPR_SGT:
    case C166::JMPR_SLE:
    case C166::JMPR_SLT:
    case C166::JMPR_SGE:
    case C166::JMPR_UGT:
    case C166::JMPR_ULE:
      return true;
    default:
      return false;
    }
  }

  bool fixupNeedsRelaxationAdvanced(const MCFragment &, const MCFixup &Fixup,
                                    const MCValue &, uint64_t Value,
                                    bool Resolved) const override {
    if (Fixup.getKind() != C166::fixup_c166_pc8)
      return false;
    if (!Resolved)
      return true;

    int64_t Delta = static_cast<int64_t>(Value) - 1;
    if (Delta & 1)
      return false;
    return !isInt<8>(Delta / 2);
  }

  void relaxInstruction(MCInst &Inst, const MCSubtargetInfo &) const override {
    MCInst Relaxed;
    if (Inst.getOpcode() == C166::JMPR_UC) {
      Relaxed.setOpcode(C166::PseudoJMPRRelaxUC);
      Relaxed.addOperand(Inst.getOperand(0));
    } else {
      Relaxed.setOpcode(C166::PseudoJMPRRelax);
      Relaxed.addOperand(
          MCOperand::createImm(getJMPREncoding(Inst.getOpcode())));
      Relaxed.addOperand(Inst.getOperand(0));
    }
    Inst = Relaxed;
  }

  std::optional<MCFixupKind> getFixupKind(StringRef Name) const override {
    unsigned Type = StringSwitch<unsigned>(Name)
#define ELF_RELOC(Name, Value) .Case(#Name, Value)
#include "llvm/BinaryFormat/ELFRelocs/C166.def"
#undef ELF_RELOC
                        .Default(-1U);
    // The final three relocation numbers are reserved.
    if (Type != -1U && Type < ELF::R_C166_RESERVED_253)
      return static_cast<MCFixupKind>(FirstLiteralRelocationKind + Type);
    return MCAsmBackend::getFixupKind(Name);
  }

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override {
    if (mc::isRelocation(Fixup.getKind())) {
      maybeAddReloc(F, Fixup, Target, Value, IsResolved);
      return;
    }

    if (Fixup.getKind() == C166::fixup_c166_pc8_relax) {
      maybeAddReloc(F, Fixup, Target, Value, /*IsResolved=*/false);
      return;
    }

    const bool IsPCRel = Fixup.getKind() == C166::fixup_c166_pc8 ||
                         Fixup.getKind() == C166::fixup_c166_pc16;
    if (Fixup.getKind() >= FirstTargetFixupKind && !IsPCRel &&
        !Target.isAbsolute())
      IsResolved = false;
    maybeAddReloc(F, Fixup, Target, Value, IsResolved);
    // C166 uses RELA.  An unresolved PC-relative fixup is left as zero and
    // converted from its byte displacement to the architectural word
    // displacement by the linker once S and P are known.
    if (IsPCRel && !IsResolved)
      return;
    Value = adjustFixupValue(Fixup, Target, Value);
    const MCFixupKindInfo Info = getFixupKindInfo(Fixup.getKind());
    if (!Value)
      return;

    Value <<= Info.TargetOffset;
    const unsigned NumBytes =
        alignTo(Info.TargetSize + Info.TargetOffset, 8) / 8;
    assert(Fixup.getOffset() + NumBytes <= F.getSize() &&
           "invalid C166 fixup offset");
    for (unsigned I = 0; I != NumBytes; ++I)
      Data[I] |= static_cast<uint8_t>((Value >> (I * 8)) & 0xff);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    static const MCFixupKindInfo Infos[C166::NumTargetFixupKinds] = {
        {"fixup_c166_seg8", 0, 8, 0},      {"fixup_c166_seg24", 0, 24, 0},
        {"fixup_c166_sof16", 0, 16, 0},    {"fixup_c166_cof16", 0, 16, 0},
        {"fixup_c166_pag10", 0, 10, 0},    {"fixup_c166_pof14", 0, 14, 0},
        {"fixup_c166_pc8", 0, 8, 0},       {"fixup_c166_pc8_relax", 8, 8, 0},
        {"fixup_c166_pc16", 0, 16, 0},     {"fixup_c166_dpp1_16", 0, 16, 0},
        {"fixup_c166_dpp2_16", 0, 16, 0},
    };
    static_assert(std::size(Infos) == C166::NumTargetFixupKinds);

    if (mc::isRelocation(Kind))
      return {};
    if (Kind < FirstTargetFixupKind)
      return MCAsmBackend::getFixupKindInfo(Kind);
    return Infos[Kind - FirstTargetFixupKind];
  }

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createC166ELFObjectWriter(ELF::ELFOSABI_STANDALONE);
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    if (Count % 2)
      return false;
    while (Count) {
      OS.write("\xcc\x00", 2);
      Count -= 2;
    }
    return true;
  }
};

} // namespace

MCAsmBackend *llvm::createC166MCAsmBackend(const Target &T,
                                           const MCSubtargetInfo &STI,
                                           const MCRegisterInfo &MRI,
                                           const MCTargetOptions &Options) {
  return new C166AsmBackend();
}
