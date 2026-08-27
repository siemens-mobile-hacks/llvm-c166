//===-- C166InstrInfo.cpp - C166 instruction information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166InstrInfo.h"
#include "C166.h"
#include "C166CFI.h"
#include "C166Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/C166TargetParser.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "C166GenInstrInfo.inc"

using namespace llvm;

void C166InstrInfo::anchor() {}

C166InstrInfo::C166InstrInfo(const C166Subtarget &STI)
    : C166GenInstrInfo(STI, RI, C166::ADJCALLSTACKDOWN, C166::ADJCALLSTACKUP),
      RI() {}

unsigned C166InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  if (MI.isMetaInstruction())
    return 0;
  switch (MI.getOpcode()) {
  case TargetOpcode::BUNDLE:
    return getInstBundleSize(MI);
  case TargetOpcode::INLINEASM:
  case TargetOpcode::INLINEASM_BR:
    return getInlineAsmLength(
        MI.getOperand(0).getSymbolName(),
        MI.getParent()->getParent()->getTarget().getMCAsmInfo());
  default:
    return get(MI.getOpcode()).getSize();
  }
}

static bool isC166ConditionalBranch(unsigned Opcode) {
  switch (Opcode) {
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

static bool isC166UnconditionalBranch(unsigned Opcode) {
  return Opcode == C166::JMPR_UC || Opcode == C166::JMPS;
}

static bool isC166SmallDataAddress(const MachineInstr &MI,
                                   const MachineOperand &Address) {
  return !isa<Function>(Address.getGlobal()) &&
         Address.getGlobal()->getAddressSpace() == C166::NearAddressSpace &&
         MI.getParent()->getParent()->getTarget().getCodeModel() ==
             CodeModel::Small;
}

static unsigned getC166NearAddressFlag(const MachineInstr &MI,
                                       const MachineOperand &Address) {
  assert(Address.isGlobal() && "C166 near address must name a global");
  if (isa<Function>(Address.getGlobal()))
    return C166II::MO_COF;
  unsigned AddressSpace = Address.getGlobal()->getAddressSpace();
  assert((AddressSpace == C166::NearAddressSpace ||
          AddressSpace == C166::XNearDataAddressSpace) &&
         "C166 near global has an invalid address space");
  if (isC166SmallDataAddress(MI, Address))
    return C166II::MO_None;
  return AddressSpace == C166::NearAddressSpace ? C166II::MO_DPP2
                                                : C166II::MO_DPP1;
}

static unsigned getC166NearOpcode(const MachineInstr &MI,
                                  const MachineOperand &Address,
                                  unsigned AbsoluteOpcode, unsigned DPP1Opcode,
                                  unsigned DPP2Opcode) {
  unsigned Flag = getC166NearAddressFlag(MI, Address);
  if (Flag == C166II::MO_None)
    return AbsoluteOpcode;
  return Flag == C166II::MO_DPP1 ? DPP1Opcode : DPP2Opcode;
}

static unsigned reverseC166BranchOpcode(unsigned Opcode) {
  switch (Opcode) {
  case C166::JMPR_EQ:
    return C166::JMPR_NE;
  case C166::JMPR_NE:
    return C166::JMPR_EQ;
  case C166::JMPR_ULT:
    return C166::JMPR_UGE;
  case C166::JMPR_UGE:
    return C166::JMPR_ULT;
  case C166::JMPR_UGT:
    return C166::JMPR_ULE;
  case C166::JMPR_ULE:
    return C166::JMPR_UGT;
  case C166::JMPR_SGT:
    return C166::JMPR_SLE;
  case C166::JMPR_SLE:
    return C166::JMPR_SGT;
  case C166::JMPR_SLT:
    return C166::JMPR_SGE;
  case C166::JMPR_SGE:
    return C166::JMPR_SLT;
  default:
    llvm_unreachable("invalid C166 conditional branch");
  }
}

void C166InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                const DebugLoc &DL, Register DestReg,
                                Register SrcReg, bool KillSrc, bool,
                                bool) const {
  if (C166::GR16RegClass.contains(DestReg) &&
      C166::SFR16RegClass.contains(SrcReg)) {
    BuildMI(MBB, I, DL, get(C166::MOVgsfr), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }

  if (C166::SFR16RegClass.contains(DestReg) &&
      C166::GR16RegClass.contains(SrcReg)) {
    BuildMI(MBB, I, DL, get(C166::MOVsfrg), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }

  // EXTRACT_SUBREG is lowered to a physical COPY after register allocation.
  // A truncating i32-to-i16 operation therefore arrives here as a copy from a
  // GR32 pair to a GR16 register.  Copy the low word of the pair; in the usual
  // coalesced case the registers are identical and no instruction is needed.
  if (C166::GR16RegClass.contains(DestReg) &&
      C166::GR32RegClass.contains(SrcReg)) {
    Register SrcLo = RI.getSubReg(SrcReg, sub_lo16);
    if (DestReg != SrcLo)
      BuildMI(MBB, I, DL, get(C166::MOVrr), DestReg)
          .addReg(SrcLo, getKillRegState(KillSrc));
    return;
  }

  if (C166::GR32RegClass.contains(DestReg, SrcReg)) {
    Register DestLo = RI.getSubReg(DestReg, sub_lo16);
    Register DestHi = RI.getSubReg(DestReg, sub_hi16);
    Register SrcLo = RI.getSubReg(SrcReg, sub_lo16);
    Register SrcHi = RI.getSubReg(SrcReg, sub_hi16);

    auto EmitWordCopy = [&](Register Dest, Register Src) {
      BuildMI(MBB, I, DL, get(C166::MOVrr), Dest)
          .addReg(Src, getKillRegState(KillSrc));
    };

    // Adjacent register pairs overlap.  Preserve the source word that would
    // otherwise be overwritten by the first move.
    if (DestLo == SrcHi) {
      EmitWordCopy(DestHi, SrcHi);
      EmitWordCopy(DestLo, SrcLo);
    } else {
      EmitWordCopy(DestLo, SrcLo);
      EmitWordCopy(DestHi, SrcHi);
    }
    return;
  }

  unsigned Opcode;
  if (C166::GR16RegClass.contains(DestReg, SrcReg))
    Opcode = C166::MOVrr;
  else if (C166::GR8RegClass.contains(DestReg, SrcReg))
    Opcode = C166::MOVBrr;
  else
    llvm_unreachable("unsupported C166 physical register copy");

  BuildMI(MBB, I, DL, get(Opcode), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

void C166InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register SrcReg,
    bool IsKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  auto EmitWord = [&](Register Reg, unsigned SubReg, int64_t Offset,
                      bool Kill) {
    MachinePointerInfo PtrInfo =
        MachinePointerInfo::getFixedStack(*MBB.getParent(), FrameIndex, Offset);
    MachineMemOperand *MMO = MBB.getParent()->getMachineMemOperand(
        PtrInfo, MachineMemOperand::MOStore, 2, Align(2));
    BuildMI(MBB, I, DebugLoc(), get(C166::MOVfiStore))
        .addFrameIndex(FrameIndex)
        .addImm(Offset)
        .addReg(Reg, getKillRegState(Kill), SubReg)
        .addMemOperand(MMO)
        .setMIFlags(Flags);
  };

  if (C166::GR16RegClass.hasSubClassEq(RC)) {
    EmitWord(SrcReg, 0, 0, IsKill);
    return;
  }
  if (C166::GR8RegClass.hasSubClassEq(RC)) {
    MachinePointerInfo PtrInfo =
        MachinePointerInfo::getFixedStack(*MBB.getParent(), FrameIndex);
    MachineMemOperand *MMO = MBB.getParent()->getMachineMemOperand(
        PtrInfo, MachineMemOperand::MOStore, 1, Align(1));
    BuildMI(MBB, I, DebugLoc(), get(C166::MOVBfiStore))
        .addFrameIndex(FrameIndex)
        .addImm(0)
        .addReg(SrcReg, getKillRegState(IsKill))
        .addMemOperand(MMO)
        .setMIFlags(Flags);
    return;
  }
  if (C166::GR32RegClass.hasSubClassEq(RC)) {
    MachinePointerInfo PtrInfo =
        MachinePointerInfo::getFixedStack(*MBB.getParent(), FrameIndex);
    MachineMemOperand *MMO = MBB.getParent()->getMachineMemOperand(
        PtrInfo, MachineMemOperand::MOStore, 4, Align(2));
    BuildMI(MBB, I, DebugLoc(), get(C166::FRAMESTORE32))
        .addFrameIndex(FrameIndex)
        .addImm(0)
        .addReg(SrcReg, getKillRegState(IsKill))
        .addMemOperand(MMO)
        .setMIFlags(Flags);
    return;
  }
  report_fatal_error("unsupported C166 spill register class");
}

void C166InstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         Register DestReg, int FrameIndex,
                                         const TargetRegisterClass *RC,
                                         Register VReg, unsigned SubReg,
                                         MachineInstr::MIFlag Flags) const {
  // LLVM's generic register allocators and callee-save restoration request
  // whole-register reloads here; the only in-tree callers that provide the
  // optional tuple SubReg argument are target-specific code in other
  // backends, and they pass zero.  Keep this assertion so a future pipeline
  // change cannot silently turn a whole GR32 reload into a partial one.
  assert(!SubReg && "C166 register allocation requested a partial reload");
  if (SubReg)
    report_fatal_error("C166 partial reloads are not implemented");
  auto EmitWord = [&](Register Reg, unsigned SubReg, int64_t Offset,
                      bool Undef) {
    MachinePointerInfo PtrInfo =
        MachinePointerInfo::getFixedStack(*MBB.getParent(), FrameIndex, Offset);
    MachineMemOperand *MMO = MBB.getParent()->getMachineMemOperand(
        PtrInfo, MachineMemOperand::MOLoad, 2, Align(2));
    BuildMI(MBB, I, DebugLoc(), get(C166::MOVfi))
        .addReg(Reg, RegState::Define | getUndefRegState(Undef), SubReg)
        .addFrameIndex(FrameIndex)
        .addImm(Offset)
        .addMemOperand(MMO)
        .setMIFlags(Flags);
  };

  if (C166::GR16RegClass.hasSubClassEq(RC)) {
    EmitWord(DestReg, 0, 0, false);
    return;
  }
  if (C166::GR8RegClass.hasSubClassEq(RC)) {
    MachinePointerInfo PtrInfo =
        MachinePointerInfo::getFixedStack(*MBB.getParent(), FrameIndex);
    MachineMemOperand *MMO = MBB.getParent()->getMachineMemOperand(
        PtrInfo, MachineMemOperand::MOLoad, 1, Align(1));
    BuildMI(MBB, I, DebugLoc(), get(C166::MOVBfi), DestReg)
        .addFrameIndex(FrameIndex)
        .addImm(0)
        .addMemOperand(MMO)
        .setMIFlags(Flags);
    return;
  }
  if (C166::GR32RegClass.hasSubClassEq(RC)) {
    MachinePointerInfo PtrInfo =
        MachinePointerInfo::getFixedStack(*MBB.getParent(), FrameIndex);
    MachineMemOperand *MMO = MBB.getParent()->getMachineMemOperand(
        PtrInfo, MachineMemOperand::MOLoad, 4, Align(2));
    BuildMI(MBB, I, DebugLoc(), get(C166::FRAMELOAD32), DestReg)
        .addFrameIndex(FrameIndex)
        .addImm(0)
        .addMemOperand(MMO)
        .setMIFlags(Flags);
    return;
  }
  report_fatal_error("unsupported C166 reload register class");
}

bool C166InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                  MachineBasicBlock *&TBB,
                                  MachineBasicBlock *&FBB,
                                  SmallVectorImpl<MachineOperand> &Cond,
                                  bool AllowModify) const {
  (void)AllowModify;
  TBB = nullptr;
  FBB = nullptr;
  Cond.clear();

  // A lowered i32 comparison is a short decision tree containing more than
  // one CMP/JMPR pair.  Treat such blocks as opaque: describing only their
  // final conditional/unconditional pair would let generic branch folding
  // discard the earlier high-word decisions.
  unsigned BranchCount = 0;
  for (const MachineInstr &MI : MBB)
    BranchCount += MI.isBranch();
  if (BranchCount > 2)
    return true;

  auto Last = MBB.getLastNonDebugInstr();
  if (Last == MBB.end())
    return false;
  if (!Last->isBranch())
    return Last->isBarrier();

  if (isC166UnconditionalBranch(Last->getOpcode())) {
    FBB = getBranchDestBlock(*Last);
    auto Previous = Last;
    if (Previous == MBB.begin()) {
      TBB = FBB;
      FBB = nullptr;
      return false;
    }
    do {
      --Previous;
    } while (Previous != MBB.begin() && Previous->isDebugInstr());
    if (!Previous->isDebugInstr() &&
        isC166ConditionalBranch(Previous->getOpcode())) {
      TBB = getBranchDestBlock(*Previous);
      Cond.push_back(MachineOperand::CreateImm(Previous->getOpcode()));
      return false;
    }
    TBB = FBB;
    FBB = nullptr;
    return false;
  }

  if (!isC166ConditionalBranch(Last->getOpcode()))
    return true;
  TBB = getBranchDestBlock(*Last);
  Cond.push_back(MachineOperand::CreateImm(Last->getOpcode()));
  return false;
}

unsigned C166InstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  assert(TBB && "C166 branch requires a destination");
  assert(Cond.size() <= 1 && "invalid C166 branch condition");
  if (BytesAdded)
    *BytesAdded = 0;

  unsigned Count = 0;
  auto AddShortBranch = [&](unsigned Opcode, MachineBasicBlock *Dest) {
    BuildMI(&MBB, DL, get(Opcode)).addMBB(Dest);
    if (BytesAdded)
      *BytesAdded += get(Opcode).getSize();
    ++Count;
  };

  if (Cond.empty()) {
    assert(!FBB && "unconditional C166 branch has a false destination");
    AddShortBranch(C166::JMPR_UC, TBB);
    return Count;
  }

  unsigned Opcode = static_cast<unsigned>(Cond.front().getImm());
  assert(isC166ConditionalBranch(Opcode) && "invalid C166 branch opcode");
  AddShortBranch(Opcode, TBB);
  if (FBB)
    AddShortBranch(C166::JMPR_UC, FBB);
  return Count;
}

unsigned C166InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                     int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  unsigned Count = 0;
  while (true) {
    auto Last = MBB.getLastNonDebugInstr();
    if (Last == MBB.end() || (!isC166ConditionalBranch(Last->getOpcode()) &&
                              !isC166UnconditionalBranch(Last->getOpcode())))
      break;
    if (BytesRemoved)
      *BytesRemoved += get(Last->getOpcode()).getSize();
    Last->eraseFromParent();
    ++Count;
  }
  return Count;
}

bool C166InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "invalid C166 branch condition");
  Cond.front().setImm(
      reverseC166BranchOpcode(static_cast<unsigned>(Cond.front().getImm())));
  return false;
}

MachineBasicBlock *
C166InstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  assert((isC166ConditionalBranch(MI.getOpcode()) ||
          isC166UnconditionalBranch(MI.getOpcode())) &&
         "not a C166 branch");
  return MI.getOperand(0).getMBB();
}

bool C166InstrInfo::isBranchOffsetInRange(unsigned BranchOpcode,
                                          int64_t BranchOffset) const {
  if (BranchOpcode == C166::JMPS)
    return true;
  assert((isC166ConditionalBranch(BranchOpcode) ||
          BranchOpcode == C166::JMPR_UC) &&
         "not a C166 direct branch");
  const int64_t DeltaFromNextInstruction = BranchOffset - 2;
  return !(DeltaFromNextInstruction & 1) &&
         isInt<8>(DeltaFromNextInstruction / 2);
}

void C166InstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                         MachineBasicBlock &NewDestBB,
                                         MachineBasicBlock &RestoreBB,
                                         const DebugLoc &DL,
                                         int64_t BranchOffset,
                                         RegScavenger *RS) const {
  (void)RestoreBB;
  (void)BranchOffset;
  (void)RS;
  // Despite the generic API name, JMPS is a direct segmented branch.  It can
  // address any location representable by the C166 24-bit code pointer.
  BuildMI(&MBB, DL, get(C166::JMPS)).addMBB(&NewDestBB).addMBB(&NewDestBB);
}

bool C166InstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  if (MI.getOpcode() == C166::NEARLOAD32 ||
      MI.getOpcode() == C166::NEARSTORE32) {
    MachineBasicBlock &MBB = *MI.getParent();
    bool IsStore = MI.getOpcode() == C166::NEARSTORE32;
    Register Pair = MI.getOperand(IsStore ? 2 : 0).getReg();
    Register Base = MI.getOperand(IsStore ? 0 : 1).getReg();
    uint64_t Disp = MI.getOperand(IsStore ? 1 : 2).getImm();
    if (!isUInt<14>(Disp + 2)) {
      MBB.getParent()->getFunction().getContext().emitError(
          "C166 32-bit stack access exceeds the DPP1 page");
      Disp = 0;
    }

    auto EmitWord = [&](Register Word, uint64_t WordDisp) {
      unsigned Opcode;
      if (IsStore)
        Opcode = WordDisp == 0 ? C166::MOVmr : C166::MOVmr16;
      else
        Opcode = WordDisp == 0 ? C166::MOVrm : C166::MOVrm16;

      MachineInstrBuilder MIB =
          IsStore ? BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode))
                  : BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), Word);
      MIB.addReg(Base);
      if (WordDisp != 0)
        MIB.addImm(WordDisp);
      if (IsStore)
        MIB.addReg(Word);
      MIB.cloneMemRefs(MI);
    };

    EmitWord(RI.getSubReg(Pair, sub_lo16), Disp);
    EmitWord(RI.getSubReg(Pair, sub_hi16), Disp + 2);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::MUL16rr || MI.getOpcode() == C166::MULHU16rr ||
      MI.getOpcode() == C166::MULHS16rr) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    const bool HighUnsigned = MI.getOpcode() == C166::MULHU16rr;
    const bool HighSigned = MI.getOpcode() == C166::MULHS16rr;
    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(HighUnsigned ? C166::MULUrr : C166::MULrr))
        .addReg(MI.getOperand(1).getReg())
        .addReg(MI.getOperand(2).getReg());
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgsfr), Dst)
        .addReg(HighUnsigned || HighSigned ? C166::MDH : C166::MDL);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::SDIV16rr || MI.getOpcode() == C166::UDIV16rr ||
      MI.getOpcode() == C166::SREM16rr || MI.getOpcode() == C166::UREM16rr) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    bool IsUnsigned =
        MI.getOpcode() == C166::UDIV16rr || MI.getOpcode() == C166::UREM16rr;
    bool IsRemainder =
        MI.getOpcode() == C166::SREM16rr || MI.getOpcode() == C166::UREM16rr;
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVsfrg), C166::MDL)
        .addReg(MI.getOperand(1).getReg());
    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(IsUnsigned ? C166::DIVUr : C166::DIVr))
        .addReg(MI.getOperand(2).getReg());
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgsfr), Dst)
        .addReg(IsRemainder ? C166::MDH : C166::MDL);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::CONST32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    uint32_t Value = static_cast<uint32_t>(MI.getOperand(1).getImm());
    Register Low = RI.getSubReg(Dst, sub_lo16);
    Register High = RI.getSubReg(Dst, sub_hi16);
    auto EmitWord = [&](Register Reg, uint16_t Word) {
      unsigned Opcode = Word < 16 ? C166::MOVri4 : C166::MOVri16;
      BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), Reg).addImm(Word);
    };
    EmitWord(Low, static_cast<uint16_t>(Value));
    EmitWord(High, static_cast<uint16_t>(Value >> 16));
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::GLOBALADDR32 ||
      MI.getOpcode() == C166::GLOBALDATAADDR32 ||
      MI.getOpcode() == C166::GLOBALHUGEDATAADDR32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    const MachineOperand &Address = MI.getOperand(1);
    Register Low = RI.getSubReg(Dst, sub_lo16);
    Register High = RI.getSubReg(Dst, sub_hi16);
    bool IsPagedDataAddress = MI.getOpcode() == C166::GLOBALDATAADDR32;

    MachineInstrBuilder LowMove =
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVri16), Low)
            .add(Address);
    LowMove->getOperand(1).setTargetFlags(IsPagedDataAddress ? C166II::MO_POF
                                                             : C166II::MO_SOF);
    MachineInstrBuilder HighMove =
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVri16), High)
            .add(Address);
    HighMove->getOperand(1).setTargetFlags(IsPagedDataAddress ? C166II::MO_PAG
                                                              : C166II::MO_SEG);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::NEARGLOBALADDR16) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    const MachineOperand &Address = MI.getOperand(1);
    MachineInstrBuilder Move =
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVri16), Dst)
            .add(Address);
    Move->getOperand(1).setTargetFlags(getC166NearAddressFlag(MI, Address));
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::BR) {
    MachineBasicBlock &MBB = *MI.getParent();
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::JMPR_UC))
        .addMBB(MI.getOperand(0).getMBB());
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::CMPBR || MI.getOpcode() == C166::FLAGSBR) {
    MachineBasicBlock &MBB = *MI.getParent();
    unsigned CCOperand = MI.getOpcode() == C166::CMPBR ? 2 : 0;
    unsigned TargetOperand = MI.getOpcode() == C166::CMPBR ? 3 : 1;
    unsigned BranchOpcode;
    switch (MI.getOperand(CCOperand).getImm()) {
    case C166::CC_EQ:
      BranchOpcode = C166::JMPR_EQ;
      break;
    case C166::CC_NE:
      BranchOpcode = C166::JMPR_NE;
      break;
    case C166::CC_ULT:
      BranchOpcode = C166::JMPR_ULT;
      break;
    case C166::CC_ULE:
      BranchOpcode = C166::JMPR_ULE;
      break;
    case C166::CC_UGE:
      BranchOpcode = C166::JMPR_UGE;
      break;
    case C166::CC_UGT:
      BranchOpcode = C166::JMPR_UGT;
      break;
    case C166::CC_SLT:
      BranchOpcode = C166::JMPR_SLT;
      break;
    case C166::CC_SLE:
      BranchOpcode = C166::JMPR_SLE;
      break;
    case C166::CC_SGE:
      BranchOpcode = C166::JMPR_SGE;
      break;
    case C166::CC_SGT:
      BranchOpcode = C166::JMPR_SGT;
      break;
    default:
      llvm_unreachable("unsupported C166 branch condition");
    }
    if (MI.getOpcode() == C166::CMPBR)
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::CMPrr))
          .addReg(MI.getOperand(0).getReg())
          .addReg(MI.getOperand(1).getReg());
    BuildMI(MBB, MI, MI.getDebugLoc(), get(BranchOpcode))
        .addMBB(MI.getOperand(TargetOperand).getMBB());
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::NEARLOAD8Z ||
      MI.getOpcode() == C166::NEARLOAD8S) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Base = MI.getOperand(1).getReg();
    uint64_t Disp = MI.getOperand(2).getImm();
    Register LowByte = RI.getSubReg(Dst, sub_lo8);

    if (Disp == 0) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVBrm), LowByte)
          .addReg(Base)
          .cloneMemRefs(MI);
    } else {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVBrm16), LowByte)
          .addReg(Base)
          .addImm(Disp)
          .cloneMemRefs(MI);
    }
    unsigned ExtendOpcode =
        MI.getOpcode() == C166::NEARLOAD8S ? C166::MOVBSrr : C166::MOVBZrr;
    BuildMI(MBB, MI, MI.getDebugLoc(), get(ExtendOpcode), Dst).addReg(LowByte);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::NEARSTORE8) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Base = MI.getOperand(0).getReg();
    uint64_t Disp = MI.getOperand(1).getImm();
    Register Value = MI.getOperand(2).getReg();
    Register LowByte = RI.getSubReg(Value, sub_lo8);

    if (Disp == 0) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVBmr))
          .addReg(Base)
          .addReg(LowByte)
          .cloneMemRefs(MI);
    } else {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVBmr16))
          .addReg(Base)
          .addImm(Disp)
          .addReg(LowByte)
          .cloneMemRefs(MI);
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::GLOBALSTORE16) {
    MachineBasicBlock &MBB = *MI.getParent();
    const MachineOperand &Address = MI.getOperand(0);
    Register Value = MI.getOperand(1).getReg();

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::EXTPp)).add(Address).addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVdg))
        .add(Address)
        .addReg(Value)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::NEARGLOBALSTORE16) {
    MachineBasicBlock &MBB = *MI.getParent();
    const MachineOperand &Address = MI.getOperand(0);
    Register Value = MI.getOperand(1).getReg();
    MachineInstrBuilder Store =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(getC166NearOpcode(MI, Address, C166::MOVabsdg,
                                      C166::MOVdgDPP1, C166::MOVdgDPP2)))
            .add(Address)
            .addReg(Value)
            .cloneMemRefs(MI);
    Store->getOperand(0).setTargetFlags(getC166NearAddressFlag(MI, Address));
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::GLOBALLOAD16) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    const MachineOperand &Address = MI.getOperand(1);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::EXTPp)).add(Address).addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgd), Dst)
        .add(Address)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::NEARGLOBALLOAD16) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    const MachineOperand &Address = MI.getOperand(1);
    MachineInstrBuilder Load =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(getC166NearOpcode(MI, Address, C166::MOVabsgd,
                                      C166::MOVgdDPP1, C166::MOVgdDPP2)),
                Dst)
            .add(Address)
            .cloneMemRefs(MI);
    Load->getOperand(1).setTargetFlags(getC166NearAddressFlag(MI, Address));
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::GLOBALLOAD8Z ||
      MI.getOpcode() == C166::GLOBALLOAD8S) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    const MachineOperand &Address = MI.getOperand(1);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::EXTPp)).add(Address).addImm(1);
    unsigned Opcode =
        MI.getOpcode() == C166::GLOBALLOAD8S ? C166::MOVBSgd : C166::MOVBZgd;
    BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), Dst)
        .add(Address)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::NEARGLOBALLOAD8Z ||
      MI.getOpcode() == C166::NEARGLOBALLOAD8S) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    const MachineOperand &Address = MI.getOperand(1);
    unsigned Opcode =
        MI.getOpcode() == C166::NEARGLOBALLOAD8S
            ? getC166NearOpcode(MI, Address, C166::MOVBSabsgd,
                                C166::MOVBSgdDPP1, C166::MOVBSgdDPP2)
            : getC166NearOpcode(MI, Address, C166::MOVBZabsgd,
                                C166::MOVBZgdDPP1, C166::MOVBZgdDPP2);
    MachineInstrBuilder Load =
        BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), Dst)
            .add(Address)
            .cloneMemRefs(MI);
    Load->getOperand(1).setTargetFlags(getC166NearAddressFlag(MI, Address));
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::GLOBALSTORE8) {
    MachineBasicBlock &MBB = *MI.getParent();
    const MachineOperand &Address = MI.getOperand(0);
    Register Value = MI.getOperand(1).getReg();
    Register LowByte = RI.getSubReg(Value, sub_lo8);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::EXTPp)).add(Address).addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVBdg))
        .add(Address)
        .addReg(LowByte)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::NEARGLOBALSTORE8) {
    MachineBasicBlock &MBB = *MI.getParent();
    const MachineOperand &Address = MI.getOperand(0);
    Register Value = MI.getOperand(1).getReg();
    Register LowByte = RI.getSubReg(Value, sub_lo8);
    MachineInstrBuilder Store =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(getC166NearOpcode(MI, Address, C166::MOVBabsdg,
                                      C166::MOVBdgDPP1, C166::MOVBdgDPP2)))
            .add(Address)
            .addReg(LowByte)
            .cloneMemRefs(MI);
    Store->getOperand(0).setTargetFlags(getC166NearAddressFlag(MI, Address));
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::GLOBALSTORE32) {
    MachineBasicBlock &MBB = *MI.getParent();
    MachineOperand LowAddress = MI.getOperand(0);
    MachineOperand HighAddress = LowAddress;
    HighAddress.setOffset(HighAddress.getOffset() + 2);
    Register Value = MI.getOperand(1).getReg();
    Register Low = RI.getSubReg(Value, sub_lo16);
    Register High = RI.getSubReg(Value, sub_hi16);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::EXTPp))
        .add(LowAddress)
        .addImm(2);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVdg))
        .add(LowAddress)
        .addReg(Low)
        .cloneMemRefs(MI);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVdg))
        .add(HighAddress)
        .addReg(High)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::NEARGLOBALSTORE32) {
    MachineBasicBlock &MBB = *MI.getParent();
    MachineOperand LowAddress = MI.getOperand(0);
    MachineOperand HighAddress = LowAddress;
    HighAddress.setOffset(HighAddress.getOffset() + 2);
    unsigned AddressFlag = getC166NearAddressFlag(MI, LowAddress);
    Register Value = MI.getOperand(1).getReg();
    Register Low = RI.getSubReg(Value, sub_lo16);
    Register High = RI.getSubReg(Value, sub_hi16);

    MachineInstrBuilder LowStore =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(getC166NearOpcode(MI, LowAddress, C166::MOVabsdg,
                                      C166::MOVdgDPP1, C166::MOVdgDPP2)))
            .add(LowAddress)
            .addReg(Low)
            .cloneMemRefs(MI);
    LowStore->getOperand(0).setTargetFlags(AddressFlag);
    MachineInstrBuilder HighStore =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(getC166NearOpcode(MI, HighAddress, C166::MOVabsdg,
                                      C166::MOVdgDPP1, C166::MOVdgDPP2)))
            .add(HighAddress)
            .addReg(High)
            .cloneMemRefs(MI);
    HighStore->getOperand(0).setTargetFlags(AddressFlag);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::GLOBALLOAD32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    MachineOperand LowAddress = MI.getOperand(1);
    MachineOperand HighAddress = LowAddress;
    HighAddress.setOffset(HighAddress.getOffset() + 2);
    Register Low = RI.getSubReg(Dst, sub_lo16);
    Register High = RI.getSubReg(Dst, sub_hi16);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::EXTPp))
        .add(LowAddress)
        .addImm(2);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgd), Low)
        .add(LowAddress)
        .cloneMemRefs(MI);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgd), High)
        .add(HighAddress)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::NEARGLOBALLOAD32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    MachineOperand LowAddress = MI.getOperand(1);
    MachineOperand HighAddress = LowAddress;
    HighAddress.setOffset(HighAddress.getOffset() + 2);
    unsigned AddressFlag = getC166NearAddressFlag(MI, LowAddress);
    Register Low = RI.getSubReg(Dst, sub_lo16);
    Register High = RI.getSubReg(Dst, sub_hi16);

    MachineInstrBuilder LowLoad =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(getC166NearOpcode(MI, LowAddress, C166::MOVabsgd,
                                      C166::MOVgdDPP1, C166::MOVgdDPP2)),
                Low)
            .add(LowAddress)
            .cloneMemRefs(MI);
    LowLoad->getOperand(1).setTargetFlags(AddressFlag);
    MachineInstrBuilder HighLoad =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(getC166NearOpcode(MI, HighAddress, C166::MOVabsgd,
                                      C166::MOVgdDPP1, C166::MOVgdDPP2)),
                High)
            .add(HighAddress)
            .cloneMemRefs(MI);
    HighLoad->getOperand(1).setTargetFlags(AddressFlag);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARLOAD8Z || MI.getOpcode() == C166::FARLOAD8S ||
      MI.getOpcode() == C166::SEGLOAD8Z || MI.getOpcode() == C166::SEGLOAD8S) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Address = MI.getOperand(1).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register PageOrSegment = RI.getSubReg(Address, sub_hi16);
    Register LowByte = RI.getSubReg(Dst, sub_lo8);

    bool IsSegmented =
        MI.getOpcode() == C166::SEGLOAD8Z || MI.getOpcode() == C166::SEGLOAD8S;
    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(IsSegmented ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVBrm), LowByte)
        .addReg(Offset)
        .cloneMemRefs(MI);
    unsigned ExtendOpcode =
        MI.getOpcode() == C166::FARLOAD8S || MI.getOpcode() == C166::SEGLOAD8S
            ? C166::MOVBSrr
            : C166::MOVBZrr;
    BuildMI(MBB, MI, MI.getDebugLoc(), get(ExtendOpcode), Dst).addReg(LowByte);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARSTORE8 || MI.getOpcode() == C166::SEGSTORE8) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Address = MI.getOperand(0).getReg();
    Register Value = MI.getOperand(1).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register PageOrSegment = RI.getSubReg(Address, sub_hi16);
    Register LowByte = RI.getSubReg(Value, sub_lo8);

    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(MI.getOpcode() == C166::SEGSTORE8 ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVBmr))
        .addReg(Offset)
        .addReg(LowByte)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARLOAD32 ||
      MI.getOpcode() == C166::SHUGELOAD32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Address = MI.getOperand(1).getReg();
    Register DstLow = RI.getSubReg(Dst, sub_lo16);
    Register DstHigh = RI.getSubReg(Dst, sub_hi16);
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register PageOrSegment = RI.getSubReg(Address, sub_hi16);

    BuildMI(
        MBB, MI, MI.getDebugLoc(),
        get(MI.getOpcode() == C166::SHUGELOAD32 ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(2);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrm), DstLow)
        .addReg(Offset)
        .cloneMemRefs(MI);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrm16), DstHigh)
        .addReg(Offset)
        .addImm(2)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARSTORE32 ||
      MI.getOpcode() == C166::SHUGESTORE32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Address = MI.getOperand(0).getReg();
    Register Value = MI.getOperand(1).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register PageOrSegment = RI.getSubReg(Address, sub_hi16);
    Register ValueLow = RI.getSubReg(Value, sub_lo16);
    Register ValueHigh = RI.getSubReg(Value, sub_hi16);

    BuildMI(
        MBB, MI, MI.getDebugLoc(),
        get(MI.getOpcode() == C166::SHUGESTORE32 ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(2);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVmr16))
        .addReg(Offset)
        .addImm(2)
        .addReg(ValueHigh)
        .cloneMemRefs(MI);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVmr))
        .addReg(Offset)
        .addReg(ValueLow)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARSTORE16 ||
      MI.getOpcode() == C166::SEGSTORE16) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Address = MI.getOperand(0).getReg();
    Register Value = MI.getOperand(1).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register PageOrSegment = RI.getSubReg(Address, sub_hi16);

    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(MI.getOpcode() == C166::SEGSTORE16 ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVmr))
        .addReg(Offset)
        .addReg(Value)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARLOAD16 || MI.getOpcode() == C166::SEGLOAD16) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Address = MI.getOperand(1).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register PageOrSegment = RI.getSubReg(Address, sub_hi16);

    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(MI.getOpcode() == C166::SEGLOAD16 ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrm), Dst)
        .addReg(Offset)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::ADD32rr) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Lhs = MI.getOperand(1).getReg();
    Register Rhs = MI.getOperand(2).getReg();
    Register DstLo = RI.getSubReg(Dst, sub_lo16);
    Register DstHi = RI.getSubReg(Dst, sub_hi16);
    Register LhsLo = RI.getSubReg(Lhs, sub_lo16);
    Register LhsHi = RI.getSubReg(Lhs, sub_hi16);
    Register RhsLo = RI.getSubReg(Rhs, sub_lo16);
    Register RhsHi = RI.getSubReg(Rhs, sub_hi16);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDrr), DstLo)
        .addReg(LhsLo)
        .addReg(RhsLo);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDCrr), DstHi)
        .addReg(LhsHi)
        .addReg(RhsHi);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::SEXT8rr) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    Register SrcByte = RI.getSubReg(Src, sub_lo8);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVBSrr), Dst).addReg(SrcByte);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::ZEXT16_32 || MI.getOpcode() == C166::SEXT16_32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    Register DstLow = RI.getSubReg(Dst, sub_lo16);
    Register DstHigh = RI.getSubReg(Dst, sub_hi16);

    // Adjacent pairs may allocate Src to DstHigh.  Preserve it in DstLow
    // before clearing the high word.
    if (DstLow != Src)
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), DstLow).addReg(Src);
    if (MI.getOpcode() == C166::ZEXT16_32) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVri4), DstHigh).addImm(0);
    } else {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), DstHigh)
          .addReg(DstLow);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ASHRri4), DstHigh)
          .addReg(DstHigh)
          .addImm(15);
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::SUB32rr) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Lhs = MI.getOperand(1).getReg();
    Register Rhs = MI.getOperand(2).getReg();
    Register DstLo = RI.getSubReg(Dst, sub_lo16);
    Register DstHi = RI.getSubReg(Dst, sub_hi16);
    Register LhsLo = RI.getSubReg(Lhs, sub_lo16);
    Register LhsHi = RI.getSubReg(Lhs, sub_hi16);
    Register RhsLo = RI.getSubReg(Rhs, sub_lo16);
    Register RhsHi = RI.getSubReg(Rhs, sub_hi16);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SUBrr), DstLo)
        .addReg(LhsLo)
        .addReg(RhsLo);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SUBCrr), DstHi)
        .addReg(LhsHi)
        .addReg(RhsHi);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::SHL32ri4) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Low = RI.getSubReg(Dst, sub_lo16);
    Register High = RI.getSubReg(Dst, sub_hi16);
    unsigned Amount = MI.getOperand(2).getImm();

    for (unsigned I = 0; I != Amount; ++I) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHLri4), Low)
          .addReg(Low)
          .addImm(1);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDCrr), High)
          .addReg(High)
          .addReg(High);
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARADD32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Offset = MI.getOperand(2).getReg();
    Register Low = RI.getSubReg(Dst, sub_lo16);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDrr), Low)
        .addReg(Low)
        .addReg(Offset);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::ADJSP || MI.getOpcode() == C166::ALLOCSP) {
    MachineBasicBlock &MBB = *MI.getParent();
    MachineFunction &MF = *MBB.getParent();
    const bool IsAllocation = MI.getOpcode() == C166::ALLOCSP;
    unsigned Opcode = IsAllocation ? C166::SUBri3 : C166::ADDri3;
    unsigned Remaining = MI.getOperand(0).getImm();
    const unsigned Total = Remaining;
    auto EmitCFI = [&](uint64_t DynamicOffset) {
      if (!MF.needsFrameMoves())
        return;
      const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
      const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
      const uint64_t FixedOffset = MF.getFrameInfo().getStackSize();
      const unsigned DwarfR0 = MRI->getDwarfRegNum(C166::R0, true);
      C166CFI::build(
          MBB, MI, MI.getDebugLoc(), *this,
          C166CFI::createUserStackValue(DwarfR0, FixedOffset + DynamicOffset));

      const unsigned DwarfDPP1 = MRI->getDwarfRegNum(C166::DPP1, true);
      for (const CalleeSavedInfo &CSI :
           MF.getFrameInfo().getCalleeSavedInfo()) {
        if (CSI.isSpilledToReg())
          continue;
        Register FrameReg;
        StackOffset Ref =
            TFI->getFrameIndexReference(MF, CSI.getFrameIdx(), FrameReg);
        assert(FrameReg == C166::R0 && !Ref.getScalable() &&
               "C166 callee save is not in the fixed user-stack frame");
        unsigned DwarfReg = MRI->getDwarfRegNum(CSI.getReg(), true);
        C166CFI::build(
            MBB, MI, MI.getDebugLoc(), *this,
            C166CFI::createUserStackLocation(
                DwarfReg, Ref.getFixed() + DynamicOffset, DwarfDPP1));
      }
    };
    unsigned Processed = 0;
    while (Remaining) {
      unsigned Chunk = std::min(Remaining, 6u);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), C166::R0)
          .addReg(C166::R0)
          .addImm(Chunk);
      Remaining -= Chunk;
      Processed += Chunk;
      EmitCFI(IsAllocation ? Processed : Total - Processed);
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::STOREARG) {
    MachineBasicBlock &MBB = *MI.getParent();
    unsigned Offset = MI.getOperand(0).getImm();
    if (Offset == 0) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVmr))
          .addReg(C166::R0)
          .add(MI.getOperand(1));
    } else {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVmr16))
          .addReg(C166::R0)
          .addImm(Offset)
          .add(MI.getOperand(1));
    }
    MI.eraseFromParent();
    return true;
  }

  return false;
}
