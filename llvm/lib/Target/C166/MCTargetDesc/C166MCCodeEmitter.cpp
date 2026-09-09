//===-- C166MCCodeEmitter.cpp - Encode C166 MC instructions --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166BitExpr.h"
#include "C166FixupKinds.h"
#include "C166MCAsmInfo.h"
#include "C166MCTargetDesc.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {

static bool haveSameRelocatableValue(const MCExpr *LHS, const MCExpr *RHS) {
  MCValue Left;
  MCValue Right;
  if (!LHS->evaluateAsRelocatable(Left, nullptr) ||
      !RHS->evaluateAsRelocatable(Right, nullptr))
    return false;
  return Left.getAddSym() == Right.getAddSym() &&
         Left.getSubSym() == Right.getSubSym() &&
         Left.getConstant() == Right.getConstant() &&
         Left.getSpecifier() == Right.getSpecifier();
}

class C166MCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;
  const MCInstrInfo &MCII;
  unsigned encodeBitAddress(const MCOperand &MO, C166::Fixups Kind,
                            SmallVectorImpl<MCFixup> &Fixups) const;

public:
  unsigned getBitAddressOpValue(const MCInst &MI, unsigned OpNo,
                                SmallVectorImpl<MCFixup> &Fixups,
                                const MCSubtargetInfo &STI) const;
  unsigned getBitOffsetOpValue(const MCInst &MI, unsigned OpNo,
                               SmallVectorImpl<MCFixup> &Fixups,
                               const MCSubtargetInfo &STI) const;
  C166MCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx)
      : Ctx(Ctx), MCII(MCII) {}

  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

  unsigned getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  unsigned getBranchTargetOpValue(const MCInst &MI, unsigned OpNo,
                                  SmallVectorImpl<MCFixup> &Fixups,
                                  const MCSubtargetInfo &STI) const;

  unsigned getBitBranchTargetOpValue(const MCInst &MI, unsigned OpNo,
                                     SmallVectorImpl<MCFixup> &Fixups,
                                     const MCSubtargetInfo &STI) const;

  unsigned getSequenceCountOpValue(const MCInst &MI, unsigned OpNo,
                                   SmallVectorImpl<MCFixup> &Fixups,
                                   const MCSubtargetInfo &STI) const;

  unsigned getSegmentOpValue(const MCInst &MI, unsigned OpNo,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  unsigned getCodeAddressOpValue(const MCInst &MI, unsigned OpNo,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

  unsigned getDirectAddressOpValue(const MCInst &MI, unsigned OpNo,
                                   SmallVectorImpl<MCFixup> &Fixups,
                                   const MCSubtargetInfo &STI) const;

  unsigned getByteImmediateOpValue(const MCInst &MI, unsigned OpNo,
                                   SmallVectorImpl<MCFixup> &Fixups,
                                   const MCSubtargetInfo &STI) const;

  unsigned getDirectRegOpValue(const MCInst &MI, unsigned OpNo,
                               SmallVectorImpl<MCFixup> &Fixups,
                               const MCSubtargetInfo &STI) const;

  unsigned getDirectByteRegOpValue(const MCInst &MI, unsigned OpNo,
                                   SmallVectorImpl<MCFixup> &Fixups,
                                   const MCSubtargetInfo &STI) const;

  unsigned getSFRAddressOpValue(const MCInst &MI, unsigned OpNo,
                                SmallVectorImpl<MCFixup> &Fixups,
                                const MCSubtargetInfo &STI) const;

  unsigned getSFRShortOpValue(const MCInst &MI, unsigned OpNo,
                              SmallVectorImpl<MCFixup> &Fixups,
                              const MCSubtargetInfo &STI) const;

  void encodeRelaxableBranch(const MCInst &MI, SmallVectorImpl<char> &CB,
                             SmallVectorImpl<MCFixup> &Fixups) const;

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;
};

} // namespace

unsigned
C166MCCodeEmitter::getBitOffsetOpValue(const MCInst &MI, unsigned OpNo,
                                       SmallVectorImpl<MCFixup> &Fixups,
                                       const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm())
    return MO.getImm();
  Fixups.push_back(MCFixup::create(
      1, MO.getExpr(), static_cast<MCFixupKind>(C166::fixup_c166_bit_offset)));
  return 0;
}

unsigned
C166MCCodeEmitter::getBitAddressOpValue(const MCInst &MI, unsigned OpNo,
                                        SmallVectorImpl<MCFixup> &Fixups,
                                        const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm())
    return MO.getImm();
  auto Kind = C166::fixup_c166_bit_src;
  if (MI.getOpcode() == C166::BSET || MI.getOpcode() == C166::BCLR) {
    Kind = C166::fixup_c166_bit_set;
  } else if (!MCII.get(MI.getOpcode()).isBranch() && OpNo == 0) {
    Kind = C166::fixup_c166_bit_dst;
  }
  return encodeBitAddress(MO, Kind, Fixups);
}

unsigned
C166MCCodeEmitter::encodeBitAddress(const MCOperand &MO, C166::Fixups Kind,
                                    SmallVectorImpl<MCFixup> &Fixups) const {
  if (MO.isImm())
    return MO.getImm();
  if (const auto *Parts = dyn_cast<C166BitExpr>(MO.getExpr())) {
    unsigned WordOffset = Kind == C166::fixup_c166_bit_dst ? 2 : 1;
    unsigned BitOffset = Kind == C166::fixup_c166_bit_set ? 0 : 3;
    auto BitKind = Kind == C166::fixup_c166_bit_dst
                       ? C166::fixup_c166_bit_low4
                       : C166::fixup_c166_bit_high4;
    Fixups.push_back(
        MCFixup::create(WordOffset, Parts->getWord(),
                        static_cast<MCFixupKind>(C166::fixup_c166_bit_offset)));
    Fixups.push_back(MCFixup::create(BitOffset, Parts->getBit(),
                                     static_cast<MCFixupKind>(BitKind)));
    return 0;
  }
  unsigned Offset = Kind == C166::fixup_c166_bit_dst ? 2 : 0;
  Fixups.push_back(
      MCFixup::create(Offset, MO.getExpr(), static_cast<MCFixupKind>(Kind)));
  return 0;
}

unsigned
C166MCCodeEmitter::getSequenceCountOpValue(const MCInst &MI, unsigned OpNo,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  int64_t Count = MI.getOperand(OpNo).getImm();
  assert(Count >= 1 && Count <= 4 && "invalid C166 sequence count");
  return static_cast<unsigned>(Count - 1);
}

unsigned
C166MCCodeEmitter::getByteImmediateOpValue(const MCInst &MI, unsigned OpNo,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  Fixups.push_back(MCFixup::create(2, MO.getExpr(), FK_Data_1));
  return 0;
}

unsigned
C166MCCodeEmitter::getSegmentOpValue(const MCInst &MI, unsigned OpNo,
                                     SmallVectorImpl<MCFixup> &Fixups,
                                     const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  const auto *Expr = cast<MCSpecifierExpr>(MO.getExpr());
  assert(Expr->getSpecifier() == C166::S_SEG &&
         "expected a segment expression");
  Fixups.push_back(MCFixup::create(
      1, Expr->getSubExpr(), static_cast<MCFixupKind>(C166::fixup_c166_seg8)));
  return 0;
}

unsigned
C166MCCodeEmitter::getCodeAddressOpValue(const MCInst &MI, unsigned OpNo,
                                         SmallVectorImpl<MCFixup> &Fixups,
                                         const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm() || isa<MCSpecifierExpr>(MO.getExpr()))
    return getMachineOpValue(MI, MO, Fixups, STI);
  Fixups.push_back(MCFixup::create(
      2, MO.getExpr(), static_cast<MCFixupKind>(C166::fixup_c166_cof16)));
  return 0;
}

unsigned
C166MCCodeEmitter::getDirectAddressOpValue(const MCInst &MI, unsigned OpNo,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm() || isa<MCSpecifierExpr>(MO.getExpr()))
    return getMachineOpValue(MI, MO, Fixups, STI);
  Fixups.push_back(MCFixup::create(
      2, MO.getExpr(), static_cast<MCFixupKind>(C166::fixup_c166_address16)));
  return 0;
}

unsigned
C166MCCodeEmitter::getDirectRegOpValue(const MCInst &MI, unsigned OpNo,
                                       SmallVectorImpl<MCFixup> &Fixups,
                                       const MCSubtargetInfo &STI) const {
  if (MI.getOperand(OpNo).isImm())
    return MI.getOperand(OpNo).getImm();
  MCRegister Reg = MI.getOperand(OpNo).getReg();
  const MCRegisterInfo *MRI = Ctx.getRegisterInfo();
  unsigned Encoding = MRI->getEncodingValue(Reg);
  return MRI->getRegClass(C166::GR16RegClassID).contains(Reg) ? 0xf0 | Encoding
                                                              : Encoding;
}

unsigned
C166MCCodeEmitter::getDirectByteRegOpValue(const MCInst &MI, unsigned OpNo,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  MCRegister Reg = MI.getOperand(OpNo).getReg();
  return 0xf0 | Ctx.getRegisterInfo()->getEncodingValue(Reg);
}

unsigned
C166MCCodeEmitter::getSFRAddressOpValue(const MCInst &MI, unsigned OpNo,
                                        SmallVectorImpl<MCFixup> &Fixups,
                                        const MCSubtargetInfo &STI) const {
  if (MI.getOperand(OpNo).isImm())
    return MI.getOperand(OpNo).getImm();
  MCRegister Reg = MI.getOperand(OpNo).getReg();
  return 0xfe00 | (Ctx.getRegisterInfo()->getEncodingValue(Reg) << 1);
}

unsigned
C166MCCodeEmitter::getSFRShortOpValue(const MCInst &MI, unsigned OpNo,
                                      SmallVectorImpl<MCFixup> &Fixups,
                                      const MCSubtargetInfo &STI) const {
  if (MI.getOperand(OpNo).isImm())
    return MI.getOperand(OpNo).getImm();
  MCRegister Reg = MI.getOperand(OpNo).getReg();
  return Ctx.getRegisterInfo()->getEncodingValue(Reg);
}

unsigned
C166MCCodeEmitter::getBranchTargetOpValue(const MCInst &MI, unsigned OpNo,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  assert(MO.isExpr() && "expected a C166 branch expression");
  Fixups.push_back(MCFixup::create(
      1, MO.getExpr(), static_cast<MCFixupKind>(C166::fixup_c166_pc8), true));
  return 0;
}

unsigned C166MCCodeEmitter::getBitBranchTargetOpValue(
    const MCInst &MI, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  assert(MO.isExpr() && "expected a C166 bit-branch expression");
  Fixups.push_back(MCFixup::create(
      2, MO.getExpr(), static_cast<MCFixupKind>(C166::fixup_c166_bit_pc8),
      true));
  return 0;
}

void C166MCCodeEmitter::encodeRelaxableBranch(
    const MCInst &MI, SmallVectorImpl<char> &CB,
    SmallVectorImpl<MCFixup> &Fixups) const {
  if (MI.getOpcode() == C166::PseudoBitBranchRelax ||
      MI.getOpcode() == C166::PseudoBitBranchWritebackRelax) {
    unsigned Opcode = MI.getOperand(0).getImm();
    uint16_t Address =
        encodeBitAddress(MI.getOperand(1), C166::fixup_c166_bit_src, Fixups);
    const MCOperand &Target = MI.getOperand(2);
    assert(Target.isExpr() && "expected a C166 relaxable branch expression");

    uint32_t Encoding = Opcode | ((Address >> 4) << 8) |
                        ((Address & 0xf) << 28);
    support::endian::write(CB, Encoding, llvm::endianness::little);
    for (unsigned I = 4; I < MCII.get(MI.getOpcode()).getSize(); I += 2)
      support::endian::write(CB, static_cast<uint16_t>(0x00cc),
                             llvm::endianness::little);
    Fixups.push_back(MCFixup::create(
        0, Target.getExpr(),
        static_cast<MCFixupKind>(C166::fixup_c166_pc8_relax), true));
    return;
  }

  const bool IsUnconditional = MI.getOpcode() == C166::PseudoJMPRRelaxUC;
  const unsigned TargetIndex = IsUnconditional ? 0 : 1;
  const MCOperand &Target = MI.getOperand(TargetIndex);
  assert(Target.isExpr() && "expected a C166 relaxable branch expression");

  unsigned Opcode = IsUnconditional ? 0x0d : MI.getOperand(0).getImm();
  support::endian::write(CB, static_cast<uint16_t>(Opcode),
                         llvm::endianness::little);
  for (unsigned Size = 2; Size < MCII.get(MI.getOpcode()).getSize(); Size += 2)
    support::endian::write(CB, static_cast<uint16_t>(0x00cc),
                           llvm::endianness::little);

  Fixups.push_back(MCFixup::create(
      0, Target.getExpr(), static_cast<MCFixupKind>(C166::fixup_c166_pc8_relax),
      true));
}

unsigned
C166MCCodeEmitter::getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                                     SmallVectorImpl<MCFixup> &Fixups,
                                     const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  assert(MO.isExpr() && "expected a C166 expression operand");
  const auto *Expr = dyn_cast<MCSpecifierExpr>(MO.getExpr());
  if (!Expr) {
    // Relocatable absolute 16-bit fields occupy the high halfword of every
    // currently supported 32-bit instruction form.
    Fixups.push_back(MCFixup::create(2, MO.getExpr(), FK_Data_2));
    return 0;
  }
  unsigned Offset;
  C166::Fixups Kind;
  switch (Expr->getSpecifier()) {
  case C166::S_SEG:
    Offset = 2;
    Kind = C166::fixup_c166_seg8;
    break;
  case C166::S_SOF:
    Offset = 2;
    Kind = C166::fixup_c166_sof16;
    break;
  case C166::S_COF:
    Offset = 2;
    Kind = C166::fixup_c166_cof16;
    break;
  case C166::S_PAG:
    Offset = 2;
    Kind = C166::fixup_c166_pag10;
    break;
  case C166::S_POF:
    Offset = 2;
    Kind = C166::fixup_c166_pof14;
    break;
  case C166::S_DPP1:
    Offset = 2;
    Kind = C166::fixup_c166_dpp1_16;
    break;
  case C166::S_DPP2:
    Offset = 2;
    Kind = C166::fixup_c166_dpp2_16;
    break;
  default:
    llvm_unreachable("unsupported C166 instruction expression");
  }
  Fixups.push_back(MCFixup::create(Offset, Expr->getSubExpr(),
                                   static_cast<MCFixupKind>(Kind)));
  return 0;
}

void C166MCCodeEmitter::encodeInstruction(const MCInst &MI,
                                          SmallVectorImpl<char> &CB,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  if (MI.getOpcode() == C166::PseudoJMPRRelax ||
      MI.getOpcode() == C166::PseudoJMPRRelaxNET ||
      MI.getOpcode() == C166::PseudoJMPRRelaxUC ||
      MI.getOpcode() == C166::PseudoBitBranchRelax ||
      MI.getOpcode() == C166::PseudoBitBranchWritebackRelax) {
    encodeRelaxableBranch(MI, CB, Fixups);
    return;
  }

  const size_t FirstFixup = Fixups.size();
  uint64_t Encoding = getBinaryCodeForInstr(MI, Fixups, STI);
  if ((MI.getOpcode() == C166::CALLS || MI.getOpcode() == C166::JMPS ||
       MI.getOpcode() == C166::TAILJMPS) &&
      Fixups.size() == FirstFixup + 2) {
    const MCFixup &Segment = Fixups[FirstFixup];
    const MCFixup &Offset = Fixups[FirstFixup + 1];
    if (Segment.getKind() == C166::fixup_c166_seg8 &&
        Segment.getOffset() == 1 &&
        Offset.getKind() == C166::fixup_c166_sof16 && Offset.getOffset() == 2 &&
        haveSameRelocatableValue(Segment.getValue(), Offset.getValue())) {
      const MCExpr *Value = Segment.getValue();
      Fixups.resize(FirstFixup);
      Fixups.push_back(MCFixup::create(
          1, Value, static_cast<MCFixupKind>(C166::fixup_c166_seg24)));
    }
  }
  switch (MCII.get(MI.getOpcode()).getSize()) {
  case 2:
    support::endian::write(CB, static_cast<uint16_t>(Encoding),
                           llvm::endianness::little);
    return;
  case 4:
    support::endian::write(CB, static_cast<uint32_t>(Encoding),
                           llvm::endianness::little);
    return;
  default:
    llvm_unreachable("invalid C166 instruction size");
  }
}

#include "C166GenMCCodeEmitter.inc"

MCCodeEmitter *llvm::createC166MCCodeEmitter(const MCInstrInfo &MCII,
                                             MCContext &Ctx) {
  return new C166MCCodeEmitter(MCII, Ctx);
}
