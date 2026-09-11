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
#include "C166TargetMachine.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
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

bool C166InstrInfo::isReMaterializableImpl(const MachineInstr &MI) const {
  if (MI.getOpcode() != C166::CONST32 && MI.getOpcode() != C166::LEAfi &&
      MI.getOpcode() != C166::FRAMEADDR32)
    return TargetInstrInfo::isReMaterializableImpl(MI);

  Register Value = MI.getOperand(0).getReg();
  if (!Value.isVirtual())
    return false;

  const MachineRegisterInfo &MRI = MI.getMF()->getRegInfo();
  for (const MachineInstr &Use : MRI.use_nodbg_instructions(Value)) {
    if (Use.isPHI() ||
        Use.getParent()->computeRegisterLiveness(
            &RI, C166::PSW, Use.getIterator()) != MachineBasicBlock::LQR_Dead)
      return false;
  }
  return true;
}

static bool definesWordAndZeroFlag(const MachineInstr &MI, Register Reg) {
  switch (MI.getOpcode()) {
  case C166::ADDrr:
  case C166::ADDri3:
  case C166::ADDri16:
  case C166::SUBrr:
  case C166::SUBri3:
  case C166::SUBri16:
  case C166::SHLrr:
  case C166::SHLri4:
  case C166::SHRrr:
  case C166::SHRri4:
  case C166::ASHRrr:
  case C166::ASHRri4:
  case C166::XORrr:
  case C166::XORri3:
  case C166::XORri16:
  case C166::ANDrr:
  case C166::ANDri3:
  case C166::ANDri16:
  case C166::ORrr:
  case C166::ORri3:
  case C166::ORri16:
  case C166::MOVrr:
  case C166::MOVri4:
  case C166::MOVri16:
  case C166::MOVrm:
  case C166::MOVrm16:
  case C166::MOVrmPostInc:
  case C166::MOVgd:
  case C166::MOVabsgd:
  case C166::MOVgsfr:
  case C166::MOVBZrr:
  case C166::MOVBSrr:
  case C166::MOVBZgd:
  case C166::MOVBZabsgd:
  case C166::MOVBSgd:
  case C166::MOVBSabsgd:
  case C166::MOVfi:
    return MI.getOperand(0).isReg() && MI.getOperand(0).isDef() &&
           MI.getOperand(0).getReg() == Reg && !MI.getOperand(0).getSubReg();
  default:
    return false;
  }
}

static bool movesWordToMemoryAndSetsZeroFlag(const MachineInstr &MI,
                                             Register Reg) {
  unsigned SourceOperand;
  switch (MI.getOpcode()) {
  case C166::MOVmr:
  case C166::MOVdg:
  case C166::MOVabsdg:
  case C166::MOVsfrg:
    SourceOperand = 1;
    break;
  case C166::MOVmr16:
  case C166::MOVfiStore:
    SourceOperand = 2;
    break;
  case C166::MOVmrPreDec:
    SourceOperand = 2;
    break;
  default:
    return false;
  }
  const MachineOperand &Source = MI.getOperand(SourceOperand);
  return Source.isReg() && Source.getReg() == Reg && !Source.getSubReg();
}

static MachineInstr *getReusableZeroFlagDef(MachineInstr &Branch) {
  if (Branch.getOpcode() != C166::CMPBRi ||
      Branch.getOperand(1).getImm() != 0 ||
      (Branch.getOperand(2).getImm() != C166::CC_EQ &&
       Branch.getOperand(2).getImm() != C166::CC_NE))
    return nullptr;

  MachineBasicBlock::iterator I = Branch.getIterator();
  while (I != Branch.getParent()->begin()) {
    --I;
    if (I->isMetaInstruction())
      continue;
    Register Reg = Branch.getOperand(0).getReg();
    if (definesWordAndZeroFlag(*I, Reg) ||
        movesWordToMemoryAndSetsZeroFlag(*I, Reg))
      return &*I;
    return nullptr;
  }
  return nullptr;
}

int C166InstrInfo::getSPAdjust(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case C166::ADJCALLSTACKDOWN:
    // The individual allocation and push pseudos below describe the actual
    // R0 displacement within the call sequence.
    return 0;
  case C166::ALLOCSP:
    return MI.getOperand(0).getImm();
  case C166::PUSHARG:
    return 2;
  default:
    return TargetInstrInfo::getSPAdjust(MI);
  }
}

unsigned C166InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  if (MI.isMetaInstruction())
    return 0;
  switch (MI.getOpcode()) {
  case C166::BR:
  case C166::FLAGSBR:
    return 2;
  case C166::CMPBR:
  case C166::CMPBBR:
  case C166::CMPBRm:
    return 4;
  case C166::CMPBBRi:
    return isUInt<3>(MI.getOperand(1).getImm()) ? 4 : 6;
  case C166::CMPBRi:
    return isUInt<3>(MI.getOperand(1).getImm()) ? 4 : 6;
  case C166::TEST32BR: {
    bool UsesR1 = MI.getOperand(0).getReg() == C166::R1 ||
                  MI.getOperand(1).getReg() == C166::R1;
    return UsesR1 ? 4 : 6;
  }
  case C166::MASK32BR: {
    uint32_t Mask = static_cast<uint32_t>(MI.getOperand(2).getImm());
    uint16_t LowMask = static_cast<uint16_t>(Mask);
    uint16_t HighMask = static_cast<uint16_t>(Mask >> 16);
    assert((LowMask == 0) != (HighMask == 0) &&
           "C166 masked branch must test exactly one word");
    uint16_t WordMask = LowMask != 0 ? LowMask : HighMask;
    return WordMask == 0xffffu || isUInt<3>(WordMask) ? 4 : 6;
  }
  case C166::SHL64ri4:
    return 8 * MI.getOperand(4).getImm();
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

bool C166InstrInfo::isRegisterOverwrittenBeforeUse(const MachineInstr &MI,
                                                   Register Reg) const {
  auto IsOtherHalfSuperregisterUse = [&](const MachineInstr &Instruction,
                                         const MachineOperand &MO) {
    if (!MO.isReg() || !MO.isUse() || !MO.isImplicit() || !MO.getReg() ||
        !RI.regsOverlap(MO.getReg(), Reg))
      return false;
    return llvm::any_of(
        Instruction.operands(), [&](const MachineOperand &Other) {
          return Other.isReg() && !Other.isImplicit() && Other.getReg() &&
                 RI.regsOverlap(Other.getReg(), MO.getReg()) &&
                 !RI.regsOverlap(Other.getReg(), Reg);
        });
  };

  auto ReadsValue = [&](const MachineInstr &Instruction) {
    return llvm::any_of(Instruction.operands(), [&](const MachineOperand &MO) {
      return MO.isReg() && MO.isUse() && MO.getReg() &&
             RI.regsOverlap(MO.getReg(), Reg) &&
             !IsOtherHalfSuperregisterUse(Instruction, MO);
    });
  };

  auto PreservesValue = [&](const MachineInstr &Instruction) {
    Register SuperReg;
    for (const MachineOperand &MO : Instruction.operands()) {
      if (!IsOtherHalfSuperregisterUse(Instruction, MO))
        continue;
      if (llvm::any_of(Instruction.operands(), [&](const MachineOperand &Def) {
            return Def.isReg() && Def.isDef() && Def.isImplicit() &&
                   Def.getReg() == MO.getReg();
          })) {
        SuperReg = MO.getReg();
        break;
      }
    }
    if (!SuperReg)
      return false;

    return llvm::none_of(
        Instruction.operands(), [&](const MachineOperand &Def) {
          if (Def.isRegMask())
            return Def.clobbersPhysReg(Reg);
          return Def.isReg() && Def.isDef() && Def.getReg() &&
                 RI.regsOverlap(Def.getReg(), Reg) &&
                 (!Def.isImplicit() || Def.getReg() != SuperReg);
        });
  };

  enum class ScanResult { Used, Overwritten, ReachesEnd };
  auto Scan = [&](MachineBasicBlock::const_iterator Begin,
                  MachineBasicBlock::const_iterator End) {
    for (auto I = Begin; I != End; ++I) {
      if (I->isDebugInstr()) {
        if (I->readsRegister(Reg, &RI))
          return ScanResult::Used;
        continue;
      }
      if (I->isMetaInstruction())
        continue;
      if (ReadsValue(*I))
        return ScanResult::Used;
      if (I->modifiesRegister(Reg, &RI) && !PreservesValue(*I))
        return ScanResult::Overwritten;
    }
    return ScanResult::ReachesEnd;
  };

  const MachineBasicBlock &MBB = *MI.getParent();
  ScanResult Initial = Scan(std::next(MI.getIterator()), MBB.end());
  if (Initial == ScanResult::Used)
    return false;
  if (Initial == ScanResult::Overwritten)
    return true;

  SmallPtrSet<const MachineBasicBlock *, 8> Visited;
  SmallVector<const MachineBasicBlock *, 8> Worklist(MBB.successors());
  while (!Worklist.empty()) {
    const MachineBasicBlock &Successor = *Worklist.pop_back_val();
    if (!Visited.insert(&Successor).second)
      continue;

    ScanResult Result = Scan(Successor.begin(), Successor.end());
    if (Result == ScanResult::Used)
      return false;
    if (Result == ScanResult::ReachesEnd)
      llvm::append_range(Worklist, Successor.successors());
  }
  return true;
}

bool C166InstrInfo::shouldHoist(const MachineInstr &MI,
                                const MachineLoop *FromLoop) const {
  if (MI.getOpcode() != C166::CONST32 ||
      !MI.getMF()->getFunction().hasOptSize())
    return true;

  Register Value = MI.getOperand(0).getReg();
  if (!Value.isVirtual())
    return true;
  const MachineRegisterInfo &MRI = MI.getMF()->getRegInfo();
  // A rematerializable pair used only by exit PHIs is cheaper on the exit
  // path than live across a loop on a target with overlapping register pairs.
  for (const MachineInstr &Use : MRI.use_nodbg_instructions(Value))
    if (!Use.isPHI() || FromLoop->contains(Use.getParent()))
      return true;
  return false;
}

static bool isC166ConditionalBranch(unsigned Opcode) {
  switch (Opcode) {
  case C166::JBreg:
  case C166::JNBreg:
  case C166::JMPR_EQ:
  case C166::JMPR_NE:
  case C166::JMPR_N:
  case C166::JMPR_NN:
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

static bool isC166RegisterBitBranch(unsigned Opcode) {
  return Opcode == C166::JBreg || Opcode == C166::JNBreg;
}

static bool isC166UnconditionalBranch(unsigned Opcode) {
  return Opcode == C166::JMPR_UC || Opcode == C166::JMPS;
}

static bool isC166ConditionalBranchPseudo(unsigned Opcode) {
  return Opcode == C166::CMPBR || Opcode == C166::CMPBBR ||
         Opcode == C166::CMPBBRi || Opcode == C166::CMPBRi ||
         Opcode == C166::FLAGSBR || Opcode == C166::TEST32BR ||
         Opcode == C166::MASK32BR || Opcode == C166::SUB32BR ||
         Opcode == C166::BITBR;
}

static bool isC166AnalyzableConditionalBranch(unsigned Opcode) {
  return isC166ConditionalBranch(Opcode) ||
         isC166ConditionalBranchPseudo(Opcode);
}

static bool isC166AnalyzableUnconditionalBranch(unsigned Opcode) {
  return isC166UnconditionalBranch(Opcode) || Opcode == C166::BR;
}

static unsigned getC166BranchTargetOperand(unsigned Opcode) {
  if (isC166RegisterBitBranch(Opcode))
    return 2;
  if (isC166ConditionalBranch(Opcode) || isC166UnconditionalBranch(Opcode) ||
      Opcode == C166::BR)
    return 0;
  if (Opcode == C166::FLAGSBR)
    return 2;
  if (Opcode == C166::MASK32BR || Opcode == C166::SUB32BR)
    return 4;
  if (Opcode == C166::BITBR)
    return 3;
  assert((Opcode == C166::CMPBR || Opcode == C166::CMPBBR ||
          Opcode == C166::CMPBBRi || Opcode == C166::CMPBRi ||
          Opcode == C166::TEST32BR) &&
         "not an analyzable C166 branch");
  return 3;
}

static void emitDynamicUserStackCFI(MachineInstr &MI, const C166InstrInfo &TII,
                                    uint64_t DynamicOffset) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  if (!MF.needsFrameMoves())
    return;

  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
  // Dynamic frames use a stable frame register. Their unwind expressions do
  // not change when R0 moves for an allocation or an outgoing call area.
  if (TFI->hasFP(MF))
    return;
  const uint64_t FixedOffset = MF.getFrameInfo().getStackSize();
  const unsigned DwarfR0 = MRI->getDwarfRegNum(C166::R0, true);
  C166CFI::build(
      MBB, MI, MI.getDebugLoc(), TII,
      C166CFI::createUserStackValue(DwarfR0, FixedOffset + DynamicOffset));

  const unsigned DwarfDPP1 = MRI->getDwarfRegNum(C166::DPP1, true);
  for (const CalleeSavedInfo &CSI : MF.getFrameInfo().getCalleeSavedInfo()) {
    if (CSI.isSpilledToReg())
      continue;
    Register FrameReg;
    StackOffset Ref =
        TFI->getFrameIndexReference(MF, CSI.getFrameIdx(), FrameReg);
    assert(FrameReg == C166::R0 && !Ref.getScalable() &&
           "C166 callee save is not in the fixed user-stack frame");
    unsigned DwarfReg = MRI->getDwarfRegNum(CSI.getReg(), true);
    C166CFI::build(MBB, MI, MI.getDebugLoc(), TII,
                   C166CFI::createUserStackLocation(
                       DwarfReg, Ref.getFixed() + DynamicOffset, DwarfDPP1));
  }
}

static bool isC166NearModelDataAddress(const MachineInstr &MI,
                                       const MachineOperand &Address) {
  return !isa<Function>(Address.getGlobal()) &&
         Address.getGlobal()->getAddressSpace() == C166::NearAddressSpace &&
         C166::hasNearData(static_cast<const C166TargetMachine &>(
                               MI.getParent()->getParent()->getTarget())
                               .getC166MemoryModel());
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
  if (isC166NearModelDataAddress(MI, Address))
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
  case C166::JBreg:
    return C166::JNBreg;
  case C166::JNBreg:
    return C166::JBreg;
  case C166::JMPR_EQ:
    return C166::JMPR_NE;
  case C166::JMPR_NE:
    return C166::JMPR_EQ;
  case C166::JMPR_N:
    return C166::JMPR_NN;
  case C166::JMPR_NN:
    return C166::JMPR_N;
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

static unsigned reverseC166ConditionCode(unsigned CC) {
  switch (CC) {
  case C166::CC_EQ:
    return C166::CC_NE;
  case C166::CC_NE:
    return C166::CC_EQ;
  case C166::CC_N:
    return C166::CC_NN;
  case C166::CC_NN:
    return C166::CC_N;
  case C166::CC_ULT:
    return C166::CC_UGE;
  case C166::CC_UGE:
    return C166::CC_ULT;
  case C166::CC_UGT:
    return C166::CC_ULE;
  case C166::CC_ULE:
    return C166::CC_UGT;
  case C166::CC_SGT:
    return C166::CC_SLE;
  case C166::CC_SLE:
    return C166::CC_SGT;
  case C166::CC_SLT:
    return C166::CC_SGE;
  case C166::CC_SGE:
    return C166::CC_SLT;
  default:
    llvm_unreachable("invalid C166 condition code");
  }
}

static bool hasSameRegisterOperands(const MachineInstr &Left,
                                    const MachineInstr &Right) {
  for (unsigned I = 0; I != 2; ++I) {
    const MachineOperand &LeftOperand = Left.getOperand(I);
    const MachineOperand &RightOperand = Right.getOperand(I);
    if (!LeftOperand.isReg() || !RightOperand.isReg() ||
        LeftOperand.getReg() != RightOperand.getReg() ||
        LeftOperand.getSubReg() != RightOperand.getSubReg())
      return false;
  }
  return true;
}

static bool hasSameCompareOperands(const MachineInstr &Left,
                                   const MachineInstr &Right) {
  if (Right.getOpcode() == C166::CMPBR)
    return (Left.getOpcode() == C166::CMPBR ||
            Left.getOpcode() == C166::CMPrr) &&
           hasSameRegisterOperands(Left, Right);

  if (Right.getOpcode() != C166::CMPBRi ||
      (Left.getOpcode() != C166::CMPBRi && Left.getOpcode() != C166::CMPri3 &&
       Left.getOpcode() != C166::CMPri16))
    return false;

  const MachineOperand &LeftRegister = Left.getOperand(0);
  const MachineOperand &RightRegister = Right.getOperand(0);
  const MachineOperand &LeftImmediate = Left.getOperand(1);
  const MachineOperand &RightImmediate = Right.getOperand(1);
  return LeftRegister.isReg() && RightRegister.isReg() &&
         LeftRegister.getReg() == RightRegister.getReg() &&
         LeftRegister.getSubReg() == RightRegister.getSubReg() &&
         LeftImmediate.isImm() && RightImmediate.isImm() &&
         LeftImmediate.getImm() == RightImmediate.getImm();
}

static bool getC166BranchCondition(const MachineInstr &Branch, unsigned &CC) {
  switch (Branch.getOpcode()) {
  case C166::CMPBR:
  case C166::CMPBRi:
    CC = Branch.getOperand(2).getImm();
    return true;
  case C166::JMPR_EQ:
    CC = C166::CC_EQ;
    return true;
  case C166::JMPR_NE:
    CC = C166::CC_NE;
    return true;
  case C166::JMPR_N:
    CC = C166::CC_N;
    return true;
  case C166::JMPR_NN:
    CC = C166::CC_NN;
    return true;
  case C166::JMPR_ULT:
    CC = C166::CC_ULT;
    return true;
  case C166::JMPR_ULE:
    CC = C166::CC_ULE;
    return true;
  case C166::JMPR_UGE:
    CC = C166::CC_UGE;
    return true;
  case C166::JMPR_UGT:
    CC = C166::CC_UGT;
    return true;
  case C166::JMPR_SLT:
    CC = C166::CC_SLT;
    return true;
  case C166::JMPR_SLE:
    CC = C166::CC_SLE;
    return true;
  case C166::JMPR_SGE:
    CC = C166::CC_SGE;
    return true;
  case C166::JMPR_SGT:
    CC = C166::CC_SGT;
    return true;
  default:
    return false;
  }
}

static bool conditionImpliesNotEqual(unsigned CC) {
  switch (CC) {
  case C166::CC_NE:
  case C166::CC_ULT:
  case C166::CC_UGT:
  case C166::CC_SLT:
  case C166::CC_SGT:
    return true;
  default:
    return false;
  }
}

struct ReusableCompareFlags {
  MachineInstr *Compare = nullptr;
  MachineInstr *LocalPSWDef = nullptr;
  unsigned CC = 0;
};

static bool explicitlyDefinesRegister(const MachineInstr &MI, Register Reg,
                                      const TargetRegisterInfo &TRI) {
  for (const MachineOperand &Operand : MI.explicit_operands())
    if (Operand.isReg() && Operand.isDef() && Operand.getReg() &&
        TRI.regsOverlap(Operand.getReg(), Reg))
      return true;
  return false;
}

static ReusableCompareFlags
getPredecessorCompareFlags(MachineInstr &Branch,
                           const TargetRegisterInfo &TRI) {
  // PSW.C is modeled separately because data movement updates other flags but
  // preserves carry.  Keep a predecessor comparison live when neither its
  // operands nor carry are changed before the repeated branch.
  ReusableCompareFlags Result;
  if (Branch.getOpcode() != C166::CMPBR && Branch.getOpcode() != C166::CMPBRi)
    return Result;

  MachineBasicBlock &MBB = *Branch.getParent();
  if (MBB.pred_size() != 1)
    return Result;

  Register LHS = Branch.getOperand(0).getReg();
  Register RHS = Branch.getOpcode() == C166::CMPBR
                     ? Branch.getOperand(1).getReg()
                     : Register();
  MachineBasicBlock::iterator BranchIterator(Branch);
  for (MachineInstr &MI : llvm::make_range(MBB.begin(), BranchIterator)) {
    if (MI.isMetaInstruction() || MI.isPHI())
      continue;
    if (MI.isCall() || MI.isInlineAsm() || MI.hasUnmodeledSideEffects() ||
        explicitlyDefinesRegister(MI, C166::PSW, TRI) ||
        MI.modifiesRegister(C166::C, &TRI) || MI.modifiesRegister(LHS, &TRI) ||
        (RHS && MI.modifiesRegister(RHS, &TRI)))
      return Result;
    if (MI.modifiesRegister(C166::PSW, &TRI))
      Result.LocalPSWDef = &MI;
  }

  MachineBasicBlock *Predecessor = *MBB.pred_begin();
  MachineInstr *PathBranch = nullptr;
  for (MachineInstr &MI : llvm::reverse(*Predecessor)) {
    if (MI.isMetaInstruction())
      continue;
    if (MI.getOpcode() == C166::CMPBR || MI.getOpcode() == C166::CMPBRi ||
        MI.getOpcode() == C166::CMPrr || MI.getOpcode() == C166::CMPri3 ||
        MI.getOpcode() == C166::CMPri16) {
      if (!hasSameCompareOperands(MI, Branch))
        return {};
      Result.Compare = &MI;
      if (MI.getOpcode() == C166::CMPBR || MI.getOpcode() == C166::CMPBRi) {
        if (PathBranch)
          return {};
        PathBranch = &MI;
      }
      break;
    }
    unsigned IgnoredCC;
    if (getC166BranchCondition(MI, IgnoredCC)) {
      if (PathBranch)
        return {};
      PathBranch = &MI;
      continue;
    }
    if (MI.modifiesRegister(C166::PSW, &TRI) ||
        MI.modifiesRegister(C166::C, &TRI))
      return {};
    if (!MI.isTerminator())
      return {};
  }
  if (!Result.Compare)
    return {};

  Result.CC = Branch.getOperand(2).getImm();
  if (!Result.LocalPSWDef)
    return Result;

  if (Result.CC == C166::CC_ULT || Result.CC == C166::CC_UGE)
    return Result;

  // ULE and UGT also depend on Z.  If the incoming edge proves that the
  // operands differ, they reduce to the carry-only ULT and UGE conditions.
  if ((Result.CC != C166::CC_ULE && Result.CC != C166::CC_UGT) || !PathBranch ||
      Predecessor->succ_size() != 2)
    return {};

  unsigned PathCC;
  if (!getC166BranchCondition(*PathBranch, PathCC))
    return {};
  unsigned TargetOperand = getC166BranchTargetOperand(PathBranch->getOpcode());
  if (PathBranch->getOperand(TargetOperand).getMBB() != &MBB)
    PathCC = reverseC166ConditionCode(PathCC);
  if (!conditionImpliesNotEqual(PathCC))
    return {};

  Result.CC = Result.CC == C166::CC_ULE ? C166::CC_ULT : C166::CC_UGE;
  return Result;
}

static bool conditionUsesCarry(unsigned CC) {
  return CC == C166::CC_ULT || CC == C166::CC_ULE || CC == C166::CC_UGE ||
         CC == C166::CC_UGT;
}

static unsigned getC166BranchOpcode(unsigned CC) {
  switch (CC) {
  case C166::CC_EQ:
    return C166::JMPR_EQ;
  case C166::CC_NE:
    return C166::JMPR_NE;
  case C166::CC_N:
    return C166::JMPR_N;
  case C166::CC_NN:
    return C166::JMPR_NN;
  case C166::CC_ULT:
    return C166::JMPR_ULT;
  case C166::CC_ULE:
    return C166::JMPR_ULE;
  case C166::CC_UGE:
    return C166::JMPR_UGE;
  case C166::CC_UGT:
    return C166::JMPR_UGT;
  case C166::CC_SLT:
    return C166::JMPR_SLT;
  case C166::CC_SLE:
    return C166::JMPR_SLE;
  case C166::CC_SGE:
    return C166::JMPR_SGE;
  case C166::CC_SGT:
    return C166::JMPR_SGT;
  default:
    llvm_unreachable("unsupported C166 branch condition");
  }
}

std::optional<DestSourcePair>
C166InstrInfo::isCopyInstrImpl(const MachineInstr &MI) const {
  if (MI.getOpcode() != C166::MOVrr || !MI.getOperand(0).isReg() ||
      !MI.getOperand(1).isReg())
    return std::nullopt;

  // Removing an instruction changes which operations an EXTP/EXTS prefix
  // covers.  Keep blocks containing an extension prefix opaque to the generic
  // copy propagator.
  if (llvm::any_of(*MI.getParent(), [](const MachineInstr &Other) {
        return Other.getOpcode() == C166::EXTPp ||
               Other.getOpcode() == C166::EXTPr ||
               Other.getOpcode() == C166::EXTSr;
      }))
    return std::nullopt;

  const MachineOperand *PSW = MI.findRegisterDefOperand(C166::PSW, &RI);
  if (!PSW)
    return std::nullopt;
  if (!PSW->isDead()) {
    bool OverwrittenBeforeUse = false;
    MachineBasicBlock::const_iterator I(MI.getIterator());
    for (++I; I != MI.getParent()->end(); ++I) {
      if (I->isDebugInstr() || I->isMetaInstruction())
        continue;
      if (I->readsRegister(C166::PSW, &RI))
        break;
      if (I->modifiesRegister(C166::PSW, &RI)) {
        OverwrittenBeforeUse = true;
        break;
      }
    }
    if (!OverwrittenBeforeUse)
      return std::nullopt;
  }
  return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
}

void C166InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                const DebugLoc &DL, Register DestReg,
                                Register SrcReg, bool KillSrc, bool,
                                bool) const {
  if (C166::GR16RegClass.contains(DestReg) &&
      C166::CARRYRegClass.contains(SrcReg)) {
    // MOV changes N/Z/E but preserves C.  ADDC can therefore materialize the
    // live carry flag as the integer value zero or one without a branch.
    BuildMI(MBB, I, DL, get(C166::MOVri4), DestReg).addImm(0);
    BuildMI(MBB, I, DL, get(C166::ADDCri3), DestReg).addReg(DestReg).addImm(0);
    return;
  }

  if (C166::CARRYRegClass.contains(DestReg) &&
      C166::GR16RegClass.contains(SrcReg)) {
    // Copy bit zero to PSW.C without changing a live source register.
    unsigned SourceAddress = (0xf0 | RI.getEncodingValue(SrcReg)) << 4;
    BuildMI(MBB, I, DL, get(C166::BMOV))
        .addImm((RI.getEncodingValue(C166::PSW) << 4) | 11)
        .addImm(SourceAddress)
        .addReg(SrcReg, RegState::Implicit | getKillRegState(KillSrc));
    return;
  }

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

    // REG_SEQUENCE commonly coalesces one of its inputs with the destination
    // pair.  If the following copy immediately replaces one word, emitting
    // that word here only creates a dead move after pseudo expansion.
    bool SkipLo = false;
    bool SkipHi = false;
    auto Next = std::next(I);
    while (Next != MBB.end() && Next->isDebugInstr())
      ++Next;
    if (Next != MBB.end() && Next->getOpcode() == TargetOpcode::COPY &&
        Next->getNumExplicitOperands() >= 2 && Next->getOperand(0).isReg() &&
        Next->getOperand(1).isReg() && !Next->getOperand(1).isUndef() &&
        Next->getOperand(0).getReg() != Next->getOperand(1).getReg()) {
      Register NextDest = Next->getOperand(0).getReg();
      SkipLo = NextDest == DestLo;
      SkipHi = NextDest == DestHi;
    }

    auto EmitWordCopy = [&](Register Dest, Register Src) {
      BuildMI(MBB, I, DL, get(C166::MOVrr), Dest)
          .addReg(Src, getKillRegState(KillSrc));
    };

    if (SkipLo || SkipHi) {
      if (!SkipLo)
        EmitWordCopy(DestLo, SrcLo);
      if (!SkipHi)
        EmitWordCopy(DestHi, SrcHi);
      return;
    }

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
  if (C166::CARRYRegClass.hasSubClassEq(RC)) {
    MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
    Register Value = MRI.createVirtualRegister(&C166::GR16RegClass);
    BuildMI(MBB, I, DebugLoc(), get(TargetOpcode::COPY), Value)
        .addReg(SrcReg, getKillRegState(IsKill));
    EmitWord(Value, 0, 0, true);
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
  if (C166::CARRYRegClass.hasSubClassEq(RC)) {
    MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
    Register Value = MRI.createVirtualRegister(&C166::GR16RegClass);
    EmitWord(Value, 0, 0, false);
    BuildMI(MBB, I, DebugLoc(), get(C166::SETCARRY), Value)
        .addDef(DestReg)
        .addReg(Value, RegState::Kill)
        .setMIFlags(Flags);
    return;
  }
  report_fatal_error("unsupported C166 reload register class");
}

static void copyImplicitRegisterLiveness(MachineInstr &Destination,
                                         const MachineInstr &Source) {
  for (MachineOperand &NewOperand : Destination.implicit_operands()) {
    if (!NewOperand.isReg())
      continue;
    for (const MachineOperand &OldOperand : Source.implicit_operands()) {
      if (!OldOperand.isReg() || NewOperand.getReg() != OldOperand.getReg() ||
          NewOperand.isDef() != OldOperand.isDef())
        continue;
      if (NewOperand.isDef())
        NewOperand.setIsDead(OldOperand.isDead());
      else
        NewOperand.setIsKill(OldOperand.isKill());
      NewOperand.setIsUndef(OldOperand.isUndef());
      break;
    }
  }
}

MachineInstr *C166InstrInfo::foldMemoryOperandImpl(
    MachineFunction &MF, MachineInstr &MI, ArrayRef<unsigned> Ops,
    MachineInstr &LoadMI, MachineInstr *&CopyMI, LiveIntervals *LIS,
    VirtRegMap *VRM) const {
  (void)CopyMI;
  (void)LIS;
  (void)VRM;

  bool PostIncrement = LoadMI.getOpcode() == C166::MOVrmPostInc;
  if (Ops.size() != 1 || (LoadMI.getOpcode() != C166::MOVrm && !PostIncrement))
    return nullptr;

  unsigned BaseOperand = PostIncrement ? 2 : 1;
  if (!LoadMI.getOperand(BaseOperand).isReg() ||
      LoadMI.getOperand(BaseOperand).getSubReg())
    return nullptr;

  const unsigned FoldedOperand = Ops.front();
  constexpr unsigned LoadBaseMarker = ~0U;
  unsigned FoldedOpcode = 0;
  SmallVector<unsigned, 5> PreservedOperands;
  auto FoldCommutative = [&](unsigned Opcode, unsigned Left, unsigned Right,
                             ArrayRef<unsigned> Prefix,
                             ArrayRef<unsigned> Suffix = {}) {
    if (FoldedOperand != Left && FoldedOperand != Right)
      return false;
    FoldedOpcode = Opcode;
    PreservedOperands.append(Prefix.begin(), Prefix.end());
    PreservedOperands.push_back(FoldedOperand == Left ? Right : Left);
    PreservedOperands.push_back(LoadBaseMarker);
    PreservedOperands.append(Suffix.begin(), Suffix.end());
    return true;
  };
  auto FoldRight = [&](unsigned Opcode, unsigned Right,
                       ArrayRef<unsigned> Prefix,
                       ArrayRef<unsigned> Suffix = {}) {
    if (FoldedOperand != Right)
      return false;
    FoldedOpcode = Opcode;
    PreservedOperands.append(Prefix.begin(), Prefix.end());
    PreservedOperands.push_back(LoadBaseMarker);
    PreservedOperands.append(Suffix.begin(), Suffix.end());
    return true;
  };

  switch (MI.getOpcode()) {
  case C166::ADDrr:
    FoldCommutative(PostIncrement ? C166::ADDrmPostInc : C166::ADDrm, 1, 2,
                    {0});
    break;
  case C166::ADDCrr:
    FoldCommutative(PostIncrement ? C166::ADDCrmPostInc : C166::ADDCrm, 1, 2,
                    {0});
    break;
  case C166::SUBrr:
    FoldRight(PostIncrement ? C166::SUBrmPostInc : C166::SUBrm, 2, {0, 1});
    break;
  case C166::SUBCrr:
    FoldRight(PostIncrement ? C166::SUBCrmPostInc : C166::SUBCrm, 2, {0, 1});
    break;
  case C166::XORrr:
    FoldCommutative(PostIncrement ? C166::XORrmPostInc : C166::XORrm, 1, 2,
                    {0});
    break;
  case C166::ANDrr:
    FoldCommutative(PostIncrement ? C166::ANDrmPostInc : C166::ANDrm, 1, 2,
                    {0});
    break;
  case C166::ORrr:
    FoldCommutative(PostIncrement ? C166::ORrmPostInc : C166::ORrm, 1, 2, {0});
    break;
  case C166::CMPrr:
    FoldRight(PostIncrement ? C166::CMPrmPostInc : C166::CMPrm, 1, {0});
    break;
  case C166::CMPBR:
    if (!PostIncrement)
      FoldRight(C166::CMPBRm, 1, {0}, {2, 3});
    break;
  case C166::ADDCarryrr:
    FoldCommutative(PostIncrement ? C166::ADDCarryrmPostInc : C166::ADDCarryrm,
                    2, 3, {0, 1});
    break;
  case C166::SUBCarryrr:
    FoldRight(PostIncrement ? C166::SUBCarryrmPostInc : C166::SUBCarryrm, 3,
              {0, 1, 2});
    break;
  case C166::ADDCCarryrr:
    FoldCommutative(PostIncrement ? C166::ADDCCarryrmPostInc
                                  : C166::ADDCCarryrm,
                    2, 3, {0, 1}, {4});
    break;
  case C166::SUBCCarryrr:
    FoldRight(PostIncrement ? C166::SUBCCarryrmPostInc : C166::SUBCCarryrm, 3,
              {0, 1, 2}, {4});
    break;
  case C166::ADDCCarryInrr:
    FoldCommutative(PostIncrement ? C166::ADDCCarryInrmPostInc
                                  : C166::ADDCCarryInrm,
                    1, 2, {0}, {3});
    break;
  case C166::SUBCCarryInrr:
    FoldRight(PostIncrement ? C166::SUBCCarryInrmPostInc : C166::SUBCCarryInrm,
              2, {0, 1}, {3});
    break;
  default:
    break;
  }
  if (!FoldedOpcode)
    return nullptr;

  MachineRegisterInfo &MRI = MF.getRegInfo();
  auto ConstrainBase = [&](Register Base) {
    return (Base.isVirtual() &&
            MRI.constrainRegClass(Base, &C166::GR16IndirectRegClass)) ||
           (Base.isPhysical() && C166::GR16IndirectRegClass.contains(Base));
  };
  Register Base = LoadMI.getOperand(BaseOperand).getReg();
  if (!ConstrainBase(Base) ||
      (PostIncrement && !ConstrainBase(LoadMI.getOperand(1).getReg())))
    return nullptr;

  MachineInstrBuilder Folded =
      BuildMI(*MI.getParent(), MI, MIMetadata(MI), get(FoldedOpcode));
  bool AddedWriteback = !PostIncrement;
  unsigned ExplicitDefs = MI.getNumExplicitDefs();
  for (unsigned Operand : PreservedOperands) {
    if (!AddedWriteback &&
        (Operand == LoadBaseMarker || Operand >= ExplicitDefs)) {
      Folded.add(LoadMI.getOperand(1));
      AddedWriteback = true;
    }
    if (Operand == LoadBaseMarker)
      Folded.add(LoadMI.getOperand(BaseOperand));
    else
      Folded.add(MI.getOperand(Operand));
  }
  assert(AddedWriteback && "post-increment fold omitted the updated base");
  copyImplicitRegisterLiveness(*Folded, MI);
  return Folded;
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
  for (const MachineInstr &MI : MBB) {
    // Reconstructing a folded memory comparison would also have to preserve
    // its MachineMemOperand, which the generic branch condition container
    // cannot represent.  Keep the block opaque until the pseudo is expanded.
    if (MI.getOpcode() == C166::CMPBRm)
      return true;
    BranchCount += MI.isBranch();
  }
  if (BranchCount > 2)
    return true;

  auto Last = MBB.getLastNonDebugInstr();
  if (Last == MBB.end())
    return false;
  if (!Last->isBranch())
    return Last->isBarrier();

  auto SetCondition = [&](const MachineInstr &Branch) {
    unsigned Opcode = Branch.getOpcode();
    Cond.push_back(MachineOperand::CreateImm(Opcode));
    if (!isC166ConditionalBranchPseudo(Opcode) &&
        !isC166RegisterBitBranch(Opcode))
      return;
    unsigned TargetOperand = getC166BranchTargetOperand(Opcode);
    for (unsigned I = 0; I != TargetOperand; ++I)
      Cond.push_back(Branch.getOperand(I));
  };

  if (isC166AnalyzableUnconditionalBranch(Last->getOpcode())) {
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
        isC166AnalyzableConditionalBranch(Previous->getOpcode())) {
      TBB = getBranchDestBlock(*Previous);
      SetCondition(*Previous);
      return false;
    }
    TBB = FBB;
    FBB = nullptr;
    return false;
  }

  if (!isC166AnalyzableConditionalBranch(Last->getOpcode()))
    return true;
  TBB = getBranchDestBlock(*Last);
  SetCondition(*Last);
  return false;
}

unsigned C166InstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  assert(TBB && "C166 branch requires a destination");
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
  assert(isC166AnalyzableConditionalBranch(Opcode) &&
         "invalid C166 branch opcode");
  if (isC166ConditionalBranchPseudo(Opcode)) {
    MachineInstrBuilder Branch = BuildMI(&MBB, DL, get(Opcode));
    for (const MachineOperand &Operand :
         ArrayRef<MachineOperand>(Cond).drop_front())
      Branch.add(Operand);
    Branch.addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(*Branch.getInstr());
    ++Count;
  } else if (isC166RegisterBitBranch(Opcode)) {
    assert(Cond.size() == 3 && "invalid C166 bit-branch condition");
    MachineInstrBuilder Branch = BuildMI(&MBB, DL, get(Opcode));
    Branch.add(Cond[1]);
    Branch.add(Cond[2]);
    Branch.addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += get(Opcode).getSize();
    ++Count;
  } else {
    assert(Cond.size() == 1 && "invalid C166 hardware branch condition");
    AddShortBranch(Opcode, TBB);
  }
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
    if (Last == MBB.end() ||
        (!isC166AnalyzableConditionalBranch(Last->getOpcode()) &&
         !isC166AnalyzableUnconditionalBranch(Last->getOpcode())))
      break;
    if (BytesRemoved)
      *BytesRemoved += getInstSizeInBytes(*Last);
    Last->eraseFromParent();
    ++Count;
  }
  return Count;
}

bool C166InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(!Cond.empty() && "invalid C166 branch condition");
  unsigned Opcode = static_cast<unsigned>(Cond.front().getImm());
  if (isC166ConditionalBranchPseudo(Opcode)) {
    Cond.back().setImm(
        reverseC166ConditionCode(static_cast<unsigned>(Cond.back().getImm())));
  } else {
    const bool IsBitBranch = isC166RegisterBitBranch(Opcode);
    assert(isC166ConditionalBranch(Opcode) &&
           Cond.size() == (IsBitBranch ? 3u : 1u) &&
           "invalid C166 hardware branch condition");
    Cond.front().setImm(reverseC166BranchOpcode(Opcode));
  }
  return false;
}

MachineBasicBlock *
C166InstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  assert((isC166AnalyzableConditionalBranch(MI.getOpcode()) ||
          isC166AnalyzableUnconditionalBranch(MI.getOpcode())) &&
         "not a C166 branch");
  return MI.getOperand(getC166BranchTargetOperand(MI.getOpcode())).getMBB();
}

bool C166InstrInfo::isBranchOffsetInRange(unsigned BranchOpcode,
                                          int64_t BranchOffset) const {
  if (BranchOpcode == C166::JMPS)
    return true;
  assert((isC166ConditionalBranch(BranchOpcode) ||
          BranchOpcode == C166::JMPR_UC) &&
         "not a C166 direct branch");
  const int64_t DeltaFromNextInstruction =
      BranchOffset - get(BranchOpcode).getSize();
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

bool C166InstrInfo::expandBranchPseudo(MachineInstr &MI) const {
  if (MI.getOpcode() == C166::BR) {
    MachineBasicBlock &MBB = *MI.getParent();
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::JMPR_UC))
        .addMBB(MI.getOperand(0).getMBB());
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::BITBR) {
    MachineBasicBlock &MBB = *MI.getParent();
    unsigned CC = MI.getOperand(2).getImm();
    assert((CC == C166::CC_EQ || CC == C166::CC_NE) &&
           "invalid C166 bit-branch condition");
    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(CC == C166::CC_NE ? C166::JBreg : C166::JNBreg))
        .add(MI.getOperand(0))
        .add(MI.getOperand(1))
        .addMBB(MI.getOperand(3).getMBB());
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() != C166::CMPBR && MI.getOpcode() != C166::CMPBBR &&
      MI.getOpcode() != C166::CMPBBRi && MI.getOpcode() != C166::CMPBRm &&
      MI.getOpcode() != C166::CMPBRi && MI.getOpcode() != C166::FLAGSBR &&
      MI.getOpcode() != C166::TEST32BR && MI.getOpcode() != C166::MASK32BR &&
      MI.getOpcode() != C166::SUB32BR)
    return false;

  MachineBasicBlock &MBB = *MI.getParent();
  bool IsWideOperation =
      MI.getOpcode() == C166::MASK32BR || MI.getOpcode() == C166::SUB32BR;
  unsigned CCOperand = MI.getOpcode() == C166::FLAGSBR ? 1
                       : IsWideOperation               ? 3
                                                       : 2;
  unsigned TargetOperand = MI.getOpcode() == C166::FLAGSBR ? 2
                           : IsWideOperation               ? 4
                                                           : 3;
  unsigned CC = MI.getOperand(CCOperand).getImm();

  MachineInstr *ZeroFlagDef = getReusableZeroFlagDef(MI);
  ReusableCompareFlags CompareFlags =
      getPredecessorCompareFlags(MI, getRegisterInfo());
  bool ReuseZeroFlag = ZeroFlagDef != nullptr;
  bool ReuseCompareFlags = CompareFlags.Compare != nullptr;
  if (ZeroFlagDef)
    ZeroFlagDef->clearRegisterDeads(C166::PSW);
  if (ReuseCompareFlags) {
    CC = CompareFlags.CC;
    if (CompareFlags.LocalPSWDef) {
      CompareFlags.Compare->clearRegisterDeads(C166::C);
      CompareFlags.LocalPSWDef->clearRegisterDeads(C166::PSW);
      MBB.addLiveIn(C166::C);
    } else {
      CompareFlags.Compare->clearRegisterDeads(C166::PSW);
      MBB.addLiveIn(C166::PSW);
      if (conditionUsesCarry(CC)) {
        CompareFlags.Compare->clearRegisterDeads(C166::C);
        MBB.addLiveIn(C166::C);
      }
    }
    MBB.sortUniqueLiveIns();
  }
  if (MI.getOpcode() == C166::CMPBR && !ReuseCompareFlags)
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::CMPrr))
        .add(MI.getOperand(0))
        .add(MI.getOperand(1));
  else if (MI.getOpcode() == C166::CMPBBR) {
    const C166RegisterInfo &RI = getRegisterInfo();
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::CMPBrr))
        .addReg(RI.getSubReg(MI.getOperand(0).getReg(), sub_lo8),
                getKillRegState(MI.getOperand(0).isKill()))
        .addReg(RI.getSubReg(MI.getOperand(1).getReg(), sub_lo8),
                getKillRegState(MI.getOperand(1).isKill()));
  } else if (MI.getOpcode() == C166::CMPBBRi) {
    const C166RegisterInfo &RI = getRegisterInfo();
    int64_t Immediate = MI.getOperand(1).getImm();
    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(isUInt<3>(Immediate) ? C166::CMPBri3 : C166::CMPBri8))
        .addReg(RI.getSubReg(MI.getOperand(0).getReg(), sub_lo8),
                getKillRegState(MI.getOperand(0).isKill()))
        .addImm(Immediate);
  } else if (MI.getOpcode() == C166::CMPBRm)
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::CMPrm))
        .add(MI.getOperand(0))
        .add(MI.getOperand(1))
        .cloneMemRefs(MI);
  else if (MI.getOpcode() == C166::CMPBRi && !ReuseZeroFlag &&
           !ReuseCompareFlags) {
    int64_t Immediate = MI.getOperand(1).getImm();
    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(isUInt<3>(Immediate) ? C166::CMPri3 : C166::CMPri16))
        .add(MI.getOperand(0))
        .addImm(Immediate);
  } else if (MI.getOpcode() == C166::TEST32BR) {
    Register LHS = MI.getOperand(0).getReg();
    Register RHS = MI.getOperand(1).getReg();
    if (LHS == C166::R1 || RHS == C166::R1) {
      unsigned OtherOperand = LHS == C166::R1 ? 1 : 0;
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ORrr), C166::R1)
          .addReg(C166::R1)
          .add(MI.getOperand(OtherOperand));
    } else {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), C166::R1)
          .add(MI.getOperand(0));
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ORrr), C166::R1)
          .addReg(C166::R1)
          .add(MI.getOperand(1));
    }
  } else if (MI.getOpcode() == C166::MASK32BR) {
    const C166RegisterInfo &RI = getRegisterInfo();
    Register Scratch = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    uint32_t Mask = static_cast<uint32_t>(MI.getOperand(2).getImm());
    uint16_t LowMask = static_cast<uint16_t>(Mask);
    uint16_t HighMask = static_cast<uint16_t>(Mask >> 16);
    assert((LowMask == 0) != (HighMask == 0) &&
           "C166 masked branch must test exactly one word");
    unsigned SubReg = LowMask != 0 ? sub_lo16 : sub_hi16;
    uint16_t WordMask = LowMask != 0 ? LowMask : HighMask;
    Register DstWord = RI.getSubReg(Scratch, SubReg);
    Register LhsWord = RI.getSubReg(LHS, SubReg);
    if (WordMask == 0xffffu) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::CMPri3))
          .addReg(LhsWord)
          .addImm(0);
    } else {
      BuildMI(MBB, MI, MI.getDebugLoc(),
              get(isUInt<3>(WordMask) ? C166::ANDri3 : C166::ANDri16), DstWord)
          .addReg(LhsWord)
          .addImm(WordMask);
    }
  } else if (MI.getOpcode() == C166::SUB32BR) {
    const C166RegisterInfo &RI = getRegisterInfo();
    Register Scratch = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SUBrr),
            RI.getSubReg(Scratch, sub_lo16))
        .addReg(RI.getSubReg(LHS, sub_lo16))
        .addReg(RI.getSubReg(RHS, sub_lo16));
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SUBCrr),
            RI.getSubReg(Scratch, sub_hi16))
        .addReg(RI.getSubReg(LHS, sub_hi16))
        .addReg(RI.getSubReg(RHS, sub_hi16));
  }
  BuildMI(MBB, MI, MI.getDebugLoc(), get(getC166BranchOpcode(CC)))
      .addMBB(MI.getOperand(TargetOperand).getMBB());
  MI.eraseFromParent();
  return true;
}

bool C166InstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  if (MI.getOpcode() == C166::BCLResfr ||
      MI.getOpcode() == C166::BSETesfr ||
      MI.getOpcode() == C166::BMOVesfrreg ||
      MI.getOpcode() == C166::BMOVregesfr) {
    MachineBasicBlock &MBB = *MI.getParent();
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::EXTR)).addImm(1);

    MachineInstrBuilder Bit;
    if (MI.getOpcode() == C166::BMOVesfrreg) {
      Bit = BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::BMOVsfrreg))
                .add(MI.getOperand(0))
                .add(MI.getOperand(1))
                .add(MI.getOperand(2));
    } else if (MI.getOpcode() == C166::BMOVregesfr) {
      Bit = BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::BMOVregsfr))
                .add(MI.getOperand(0))
                .add(MI.getOperand(1))
                .add(MI.getOperand(2))
                .add(MI.getOperand(3));
    } else {
      unsigned Opcode =
          MI.getOpcode() == C166::BCLResfr ? C166::BCLR : C166::BSET;
      Bit = BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode))
                .add(MI.getOperand(0));
    }
    Bit.cloneMemRefs(MI);
    copyImplicitRegisterLiveness(*Bit, MI);
    Bit->setFlags(MI.getFlags());
    MI.eraseFromParent();
    return true;
  }

  if (expandBranchPseudo(MI))
    return true;

  if (MI.getOpcode() == C166::SUB64Carryrr ||
      MI.getOpcode() == C166::SUB64Borrowrr) {
    MachineBasicBlock &MBB = *MI.getParent();
    bool MaterializeBorrow = MI.getOpcode() == C166::SUB64Borrowrr;
    Register Result = MI.getOperand(0).getReg();
    Register Low = MI.getOperand(1).getReg();
    Register High = MI.getOperand(2).getReg();
    Register RhsLow = MI.getOperand(5).getReg();
    Register RhsHigh = MI.getOperand(6).getReg();
    const TargetRegisterInfo &RI =
        *MBB.getParent()->getSubtarget().getRegisterInfo();
    if (MaterializeBorrow)
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVri4), Result).addImm(0);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SUBrr),
            RI.getSubReg(Low, sub_lo16))
        .addReg(RI.getSubReg(Low, sub_lo16))
        .addReg(RI.getSubReg(RhsLow, sub_lo16));
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SUBCrr),
            RI.getSubReg(Low, sub_hi16))
        .addReg(RI.getSubReg(Low, sub_hi16))
        .addReg(RI.getSubReg(RhsLow, sub_hi16));
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SUBCrr),
            RI.getSubReg(High, sub_lo16))
        .addReg(RI.getSubReg(High, sub_lo16))
        .addReg(RI.getSubReg(RhsHigh, sub_lo16));
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SUBCrr),
            RI.getSubReg(High, sub_hi16))
        .addReg(RI.getSubReg(High, sub_hi16))
        .addReg(RI.getSubReg(RhsHigh, sub_hi16));
    if (MaterializeBorrow)
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDCri3), Result)
          .addReg(Result)
          .addImm(0);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::SETCARRY) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Scratch = MI.getOperand(0).getReg();
    Register Value = MI.getOperand(2).getReg();
    bool ScratchIsDead = MI.getOperand(0).isDead();
    MachineInstrBuilder Expanded =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(ScratchIsDead ? C166::SHRri4 : C166::ADDri16), Scratch)
            .addReg(Value, getKillRegState(MI.getOperand(2).isKill()))
            .addImm(ScratchIsDead ? 1 : 0xffff);
    Expanded->getOperand(0).setIsDead(ScratchIsDead);
    copyImplicitRegisterLiveness(*Expanded, MI);
    if (MachineOperand *Carry = Expanded->findRegisterDefOperand(C166::C, &RI))
      Carry->setIsDead(MI.getOperand(1).isDead());
    Expanded->setFlags(MI.getFlags());
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::NEARLOAD32 ||
      MI.getOpcode() == C166::NEARSTORE32) {
    MachineBasicBlock &MBB = *MI.getParent();
    bool IsStore = MI.getOpcode() == C166::NEARSTORE32;
    Register Pair = MI.getOperand(IsStore ? 2 : 0).getReg();
    Register Base = MI.getOperand(IsStore ? 0 : 1).getReg();
    uint16_t Disp =
        static_cast<uint16_t>(MI.getOperand(IsStore ? 1 : 2).getImm());

    auto EmitWord = [&](Register Word, uint16_t WordDisp) {
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
    EmitWord(RI.getSubReg(Pair, sub_hi16), static_cast<uint16_t>(Disp + 2));
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

  if (MI.getOpcode() == C166::SMUL16_32 || MI.getOpcode() == C166::UMUL16_32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register DstLo = RI.getSubReg(Dst, sub_lo16);
    Register DstHi = RI.getSubReg(Dst, sub_hi16);
    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(MI.getOpcode() == C166::UMUL16_32 ? C166::MULUrr : C166::MULrr))
        .addReg(MI.getOperand(1).getReg())
        .addReg(MI.getOperand(2).getReg());
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgsfr), DstLo)
        .addReg(C166::MDL);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgsfr), DstHi)
        .addReg(C166::MDH);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::SMUL16_ADD32 ||
      MI.getOpcode() == C166::UMUL16_ADD32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register DstLo = RI.getSubReg(Dst, sub_lo16);
    Register DstHi = RI.getSubReg(Dst, sub_hi16);
    BuildMI(
        MBB, MI, MI.getDebugLoc(),
        get(MI.getOpcode() == C166::UMUL16_ADD32 ? C166::MULUrr : C166::MULrr))
        .addReg(MI.getOperand(2).getReg())
        .addReg(MI.getOperand(3).getReg());
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDgsfr), DstLo)
        .addReg(DstLo)
        .addReg(C166::MDL);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDCgsfr), DstHi)
        .addReg(DstHi)
        .addReg(C166::MDH);
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

  if (MI.getOpcode() == C166::UDIVREM32_16) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Dividend = MI.getOperand(1).getReg();
    Register Divisor = MI.getOperand(2).getReg();
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVsfrg), C166::MDH)
        .addReg(RI.getSubReg(Dividend, sub_hi16));
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVsfrg), C166::MDL)
        .addReg(RI.getSubReg(Dividend, sub_lo16));
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::DIVLUr)).addReg(Divisor);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgsfr),
            RI.getSubReg(Dst, sub_lo16))
        .addReg(C166::MDL);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgsfr),
            RI.getSubReg(Dst, sub_hi16))
        .addReg(C166::MDH);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::UDIVREM32_16_FULL) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Quotient = MI.getOperand(0).getReg();
    Register Remainder = MI.getOperand(1).getReg();
    Register Dividend = MI.getOperand(2).getReg();
    Register Divisor = MI.getOperand(3).getReg();
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVsfrg), C166::MDL)
        .addReg(RI.getSubReg(Dividend, sub_hi16));
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::DIVUr)).addReg(Divisor);
    if (!MI.getOperand(0).isDead()) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgsfr),
              RI.getSubReg(Quotient, sub_hi16))
          .addReg(C166::MDL);
    }
    // DIVU leaves its remainder in MDH, already in the high half required by
    // DIVLU.  Only replace MDL with the next dividend word.
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVsfrg), C166::MDL)
        .addReg(RI.getSubReg(Dividend, sub_lo16));
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::DIVLUr)).addReg(Divisor);
    if (!MI.getOperand(0).isDead()) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgsfr),
              RI.getSubReg(Quotient, sub_lo16))
          .addReg(C166::MDL);
    }
    if (!MI.getOperand(1).isDead())
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVgsfr), Remainder)
          .addReg(C166::MDH);
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

  if (MI.getOpcode() == C166::NEARLOAD8Z_POSTINC ||
      MI.getOpcode() == C166::NEARLOAD8S_POSTINC) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Base = MI.getOperand(2).getReg();
    Register LowByte = RI.getSubReg(Dst, sub_lo8);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVBrmPostInc), LowByte)
        .addDef(Base)
        .addReg(Base)
        .cloneMemRefs(MI);
    unsigned ExtendOpcode = MI.getOpcode() == C166::NEARLOAD8S_POSTINC
                                ? C166::MOVBSrr
                                : C166::MOVBZrr;
    BuildMI(MBB, MI, MI.getDebugLoc(), get(ExtendOpcode), Dst).addReg(LowByte);
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

  if (MI.getOpcode() == C166::FARLOAD8Z_POSTINC ||
      MI.getOpcode() == C166::FARLOAD8S_POSTINC) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Address = MI.getOperand(2).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register Page = RI.getSubReg(Address, sub_hi16);
    Register LowByte = RI.getSubReg(Dst, sub_lo8);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::EXTPr)).addReg(Page).addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVBrmPostInc), LowByte)
        .addDef(Offset)
        .addReg(Offset)
        .cloneMemRefs(MI);
    unsigned ExtendOpcode = MI.getOpcode() == C166::FARLOAD8S_POSTINC
                                ? C166::MOVBSrr
                                : C166::MOVBZrr;
    BuildMI(MBB, MI, MI.getDebugLoc(), get(ExtendOpcode), Dst).addReg(LowByte);
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
    uint16_t Disp = IsSegmented ? 0 : MI.getOperand(2).getImm();
    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(IsSegmented ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(1);
    MachineInstrBuilder Load =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(Disp == 0 ? C166::MOVBrm : C166::MOVBrm16), LowByte)
            .addReg(Offset);
    if (Disp != 0)
      Load.addImm(Disp);
    Load.cloneMemRefs(MI);
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
    bool IsSegmented = MI.getOpcode() == C166::SEGSTORE8;
    uint16_t Disp = IsSegmented ? 0 : MI.getOperand(1).getImm();
    Register Value = MI.getOperand(IsSegmented ? 1 : 2).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register PageOrSegment = RI.getSubReg(Address, sub_hi16);
    Register LowByte = RI.getSubReg(Value, sub_lo8);

    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(IsSegmented ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(1);
    MachineInstrBuilder Store =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(Disp == 0 ? C166::MOVBmr : C166::MOVBmr16))
            .addReg(Offset);
    if (Disp != 0)
      Store.addImm(Disp);
    Store.addReg(LowByte).cloneMemRefs(MI);
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
    bool IsSegmented = MI.getOpcode() == C166::SHUGELOAD32;
    uint16_t Disp = IsSegmented ? 0 : MI.getOperand(2).getImm();
    uint16_t HighDisp = static_cast<uint16_t>(Disp + 2);

    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(IsSegmented ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(2);
    MachineInstrBuilder LowLoad =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(Disp == 0 ? C166::MOVrm : C166::MOVrm16), DstLow)
            .addReg(Offset);
    if (Disp != 0)
      LowLoad.addImm(Disp);
    LowLoad.cloneMemRefs(MI);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrm16), DstHigh)
        .addReg(Offset)
        .addImm(HighDisp)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARSTORE32 ||
      MI.getOpcode() == C166::SHUGESTORE32) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Address = MI.getOperand(0).getReg();
    bool IsSegmented = MI.getOpcode() == C166::SHUGESTORE32;
    uint16_t Disp = IsSegmented ? 0 : MI.getOperand(1).getImm();
    uint16_t HighDisp = static_cast<uint16_t>(Disp + 2);
    Register Value = MI.getOperand(IsSegmented ? 1 : 2).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register PageOrSegment = RI.getSubReg(Address, sub_hi16);
    Register ValueLow = RI.getSubReg(Value, sub_lo16);
    Register ValueHigh = RI.getSubReg(Value, sub_hi16);

    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(IsSegmented ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(2);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVmr16))
        .addReg(Offset)
        .addImm(HighDisp)
        .addReg(ValueHigh)
        .cloneMemRefs(MI);
    MachineInstrBuilder LowStore =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(Disp == 0 ? C166::MOVmr : C166::MOVmr16))
            .addReg(Offset);
    if (Disp != 0)
      LowStore.addImm(Disp);
    LowStore.addReg(ValueLow).cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARSTORE16 ||
      MI.getOpcode() == C166::SEGSTORE16) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Address = MI.getOperand(0).getReg();
    bool IsSegmented = MI.getOpcode() == C166::SEGSTORE16;
    uint16_t Disp = IsSegmented ? 0 : MI.getOperand(1).getImm();
    Register Value = MI.getOperand(IsSegmented ? 1 : 2).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register PageOrSegment = RI.getSubReg(Address, sub_hi16);

    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(IsSegmented ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(1);
    MachineInstrBuilder Store =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(Disp == 0 ? C166::MOVmr : C166::MOVmr16))
            .addReg(Offset);
    if (Disp != 0)
      Store.addImm(Disp);
    Store.addReg(Value).cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARLOAD16 || MI.getOpcode() == C166::SEGLOAD16) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Address = MI.getOperand(1).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register PageOrSegment = RI.getSubReg(Address, sub_hi16);
    bool IsSegmented = MI.getOpcode() == C166::SEGLOAD16;
    uint16_t Disp = IsSegmented ? 0 : MI.getOperand(2).getImm();

    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(IsSegmented ? C166::EXTSr : C166::EXTPr))
        .addReg(PageOrSegment)
        .addImm(1);
    MachineInstrBuilder Load =
        BuildMI(MBB, MI, MI.getDebugLoc(),
                get(Disp == 0 ? C166::MOVrm : C166::MOVrm16), Dst)
            .addReg(Offset);
    if (Disp != 0)
      Load.addImm(Disp);
    Load.cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARLOAD16_POSTINC) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Address = MI.getOperand(2).getReg();
    Register Offset = RI.getSubReg(Address, sub_lo16);
    Register Page = RI.getSubReg(Address, sub_hi16);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::EXTPr)).addReg(Page).addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrmPostInc), Dst)
        .addReg(Offset, RegState::Define)
        .addReg(Offset)
        .cloneMemRefs(MI);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::ADD32Carryrr ||
      MI.getOpcode() == C166::ADD32CCarryrr ||
      MI.getOpcode() == C166::SUB32Carryrr ||
      MI.getOpcode() == C166::SUB32CCarryrr) {
    MachineBasicBlock &MBB = *MI.getParent();
    bool IsAdd = MI.getOpcode() == C166::ADD32Carryrr ||
                 MI.getOpcode() == C166::ADD32CCarryrr;
    bool HasCarryIn = MI.getOpcode() == C166::ADD32CCarryrr ||
                      MI.getOpcode() == C166::SUB32CCarryrr;
    Register Dst = MI.getOperand(0).getReg();
    Register Lhs = MI.getOperand(2).getReg();
    Register Rhs = MI.getOperand(3).getReg();
    Register DstLo = RI.getSubReg(Dst, sub_lo16);
    Register DstHi = RI.getSubReg(Dst, sub_hi16);
    Register LhsLo = RI.getSubReg(Lhs, sub_lo16);
    Register LhsHi = RI.getSubReg(Lhs, sub_hi16);
    Register RhsLo = RI.getSubReg(Rhs, sub_lo16);
    Register RhsHi = RI.getSubReg(Rhs, sub_hi16);

    unsigned FirstOpcode;
    if (HasCarryIn)
      FirstOpcode = IsAdd ? C166::ADDCCarryrr : C166::SUBCCarryrr;
    else
      FirstOpcode = IsAdd ? C166::ADDCarryrr : C166::SUBCarryrr;
    MachineInstrBuilder Low =
        BuildMI(MBB, MI, MI.getDebugLoc(), get(FirstOpcode), DstLo)
            .addDef(C166::C)
            .addReg(LhsLo)
            .addReg(RhsLo);
    if (HasCarryIn)
      Low.add(MI.getOperand(4));

    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(IsAdd ? C166::ADDCCarryrr : C166::SUBCCarryrr), DstHi)
        .add(MI.getOperand(1))
        .addReg(LhsHi)
        .addReg(RhsHi)
        .addReg(C166::C, RegState::Kill);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::ADDCarryValuerr ||
      MI.getOpcode() == C166::ADDCCarryValuerr ||
      MI.getOpcode() == C166::SUBCarryValuerr ||
      MI.getOpcode() == C166::SUBCCarryValuerr ||
      MI.getOpcode() == C166::ADD32CarryValuerr ||
      MI.getOpcode() == C166::ADD32CCarryValuerr ||
      MI.getOpcode() == C166::SUB32CarryValuerr ||
      MI.getOpcode() == C166::SUB32CCarryValuerr) {
    MachineBasicBlock &MBB = *MI.getParent();
    bool IsWide = MI.getOpcode() == C166::ADD32CarryValuerr ||
                  MI.getOpcode() == C166::ADD32CCarryValuerr ||
                  MI.getOpcode() == C166::SUB32CarryValuerr ||
                  MI.getOpcode() == C166::SUB32CCarryValuerr;
    bool IsAdd = MI.getOpcode() == C166::ADDCarryValuerr ||
                 MI.getOpcode() == C166::ADDCCarryValuerr ||
                 MI.getOpcode() == C166::ADD32CarryValuerr ||
                 MI.getOpcode() == C166::ADD32CCarryValuerr;
    bool HasCarryIn = MI.getOpcode() == C166::ADDCCarryValuerr ||
                      MI.getOpcode() == C166::SUBCCarryValuerr ||
                      MI.getOpcode() == C166::ADD32CCarryValuerr ||
                      MI.getOpcode() == C166::SUB32CCarryValuerr;
    Register Dst = MI.getOperand(0).getReg();
    Register CarryValue = MI.getOperand(1).getReg();
    Register Lhs = MI.getOperand(2).getReg();
    Register Rhs = MI.getOperand(3).getReg();

    auto EmitCarryOperation = [&](Register Result, Register Left,
                                  Register Right, bool ConsumeCarry) {
      unsigned Opcode;
      if (ConsumeCarry)
        Opcode = IsAdd ? C166::ADDCCarryrr : C166::SUBCCarryrr;
      else
        Opcode = IsAdd ? C166::ADDCarryrr : C166::SUBCarryrr;
      MachineInstrBuilder Arithmetic =
          BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), Result)
              .addDef(C166::C)
              .addReg(Left)
              .addReg(Right);
      if (ConsumeCarry)
        Arithmetic.addReg(C166::C, RegState::Kill);
    };

    if (IsWide) {
      Register DstLo = RI.getSubReg(Dst, sub_lo16);
      Register DstHi = RI.getSubReg(Dst, sub_hi16);
      Register LhsLo = RI.getSubReg(Lhs, sub_lo16);
      Register LhsHi = RI.getSubReg(Lhs, sub_hi16);
      Register RhsLo = RI.getSubReg(Rhs, sub_lo16);
      Register RhsHi = RI.getSubReg(Rhs, sub_hi16);
      EmitCarryOperation(DstLo, LhsLo, RhsLo, HasCarryIn);
      EmitCarryOperation(DstHi, LhsHi, RhsHi, true);
    } else {
      EmitCarryOperation(Dst, Lhs, Rhs, HasCarryIn);
    }

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVri4), CarryValue).addImm(0);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDCri3), CarryValue)
        .addReg(CarryValue)
        .addImm(0);
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

  if (MI.getOpcode() == C166::ADD32ri || MI.getOpcode() == C166::SUB32ri ||
      MI.getOpcode() == C166::XOR32ri || MI.getOpcode() == C166::AND32ri ||
      MI.getOpcode() == C166::OR32ri) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Lhs = MI.getOperand(1).getReg();
    uint32_t Immediate = static_cast<uint32_t>(MI.getOperand(2).getImm());
    Register DstLo = RI.getSubReg(Dst, sub_lo16);
    Register DstHi = RI.getSubReg(Dst, sub_hi16);
    Register LhsLo = RI.getSubReg(Lhs, sub_lo16);
    Register LhsHi = RI.getSubReg(Lhs, sub_hi16);
    assert(DstLo == LhsLo && DstHi == LhsHi &&
           "C166 i32 immediate operation must be two-address");

    auto EmitImmediate = [&](Register Reg, uint16_t Word, unsigned ShortOpcode,
                             unsigned FullOpcode) {
      BuildMI(MBB, MI, MI.getDebugLoc(),
              get(Word <= 7 ? ShortOpcode : FullOpcode), Reg)
          .addReg(Reg)
          .addImm(Word);
    };

    auto EmitBitUpdate = [&](Register Reg, uint16_t Word, bool IsSet) {
      uint16_t ChangedBit = IsSet ? Word : static_cast<uint16_t>(~Word);
      if (!isPowerOf2_32(ChangedBit) || (IsSet && Word <= 7))
        return false;
      unsigned Opcode = IsSet ? C166::BSETreg : C166::BCLRreg;
      BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), Reg)
          .addReg(Reg)
          .addImm(countr_zero(ChangedBit));
      return true;
    };

    uint16_t Low = static_cast<uint16_t>(Immediate);
    uint16_t High = static_cast<uint16_t>(Immediate >> 16);
    switch (MI.getOpcode()) {
    case C166::ADD32ri:
      EmitImmediate(DstLo, Low, C166::ADDri3, C166::ADDri16);
      EmitImmediate(DstHi, High, C166::ADDCri3, C166::ADDCri16);
      break;
    case C166::SUB32ri:
      EmitImmediate(DstLo, Low, C166::SUBri3, C166::SUBri16);
      EmitImmediate(DstHi, High, C166::SUBCri3, C166::SUBCri16);
      break;
    case C166::XOR32ri:
      if (Low == UINT16_MAX)
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::CPL), DstLo).addReg(DstLo);
      else if (Low != 0)
        EmitImmediate(DstLo, Low, C166::XORri3, C166::XORri16);
      if (High == UINT16_MAX)
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::CPL), DstHi).addReg(DstHi);
      else if (High != 0)
        EmitImmediate(DstHi, High, C166::XORri3, C166::XORri16);
      break;
    case C166::AND32ri:
      if (Low != UINT16_MAX && !EmitBitUpdate(DstLo, Low, false))
        EmitImmediate(DstLo, Low, C166::ANDri3, C166::ANDri16);
      if (High != UINT16_MAX && !EmitBitUpdate(DstHi, High, false))
        EmitImmediate(DstHi, High, C166::ANDri3, C166::ANDri16);
      break;
    case C166::OR32ri:
      if (Low != 0 && !EmitBitUpdate(DstLo, Low, true))
        EmitImmediate(DstLo, Low, C166::ORri3, C166::ORri16);
      if (High != 0 && !EmitBitUpdate(DstHi, High, true))
        EmitImmediate(DstHi, High, C166::ORri3, C166::ORri16);
      break;
    default:
      llvm_unreachable("unexpected C166 i32 immediate operation");
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::ROTL32ri5) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Scratch = MI.getOperand(1).getReg();
    Register Lhs = MI.getOperand(2).getReg();
    unsigned Amount = MI.getOperand(3).getImm();
    Register DstLo = RI.getSubReg(Dst, sub_lo16);
    Register DstHi = RI.getSubReg(Dst, sub_hi16);
    Register LhsLo = RI.getSubReg(Lhs, sub_lo16);
    Register LhsHi = RI.getSubReg(Lhs, sub_hi16);

    assert(Amount >= 1 && Amount <= 31 && "invalid C166 rotate amount");
    Register LowSource = Amount < 16 ? LhsLo : LhsHi;
    Register HighSource = Amount < 16 ? LhsHi : LhsLo;
    unsigned WordAmount = Amount & 15;
    if (WordAmount == 0) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), DstLo)
          .addReg(LowSource);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), DstHi)
          .addReg(HighSource);
    } else {
      auto EmitHalf = [&](Register DstWord, Register ShiftedSource,
                          Register WrappedSource) {
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), Scratch)
            .addReg(WrappedSource);
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), DstWord)
            .addReg(ShiftedSource);
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHRri4), Scratch)
            .addReg(Scratch)
            .addImm(16 - WordAmount);
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHLri4), DstWord)
            .addReg(DstWord)
            .addImm(WordAmount);
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ORrr), DstWord)
            .addReg(DstWord)
            .addReg(Scratch);
      };
      EmitHalf(DstLo, LowSource, HighSource);
      EmitHalf(DstHi, HighSource, LowSource);
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::ZEXT8rr || MI.getOpcode() == C166::SEXT8rr) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    Register SrcByte = RI.getSubReg(Src, sub_lo8);

    unsigned Opcode =
        MI.getOpcode() == C166::ZEXT8rr ? C166::MOVBZrr : C166::MOVBSrr;
    BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), Dst).addReg(SrcByte);
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

  if (MI.getOpcode() == C166::SRL32ri1 || MI.getOpcode() == C166::SRA32ri1) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Low = RI.getSubReg(Dst, sub_lo16);
    Register High = RI.getSubReg(Dst, sub_hi16);

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHRri4), Low)
        .addReg(Low)
        .addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::BMOVreg), Low)
        .addReg(Low)
        .addReg(High)
        .addImm(15)
        .addImm(0);
    BuildMI(
        MBB, MI, MI.getDebugLoc(),
        get(MI.getOpcode() == C166::SRL32ri1 ? C166::SHRri4 : C166::ASHRri4),
        High)
        .addReg(High)
        .addImm(1);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::SRLPAIRri1 ||
      MI.getOpcode() == C166::SRAPAIRri1) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Low = MI.getOperand(0).getReg();
    Register High = MI.getOperand(1).getReg();

    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHRri4), Low)
        .addReg(Low)
        .addImm(1);
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::BMOVreg), Low)
        .addReg(Low)
        .addReg(High)
        .addImm(15)
        .addImm(0);
    BuildMI(
        MBB, MI, MI.getDebugLoc(),
        get(MI.getOpcode() == C166::SRLPAIRri1 ? C166::SHRri4 : C166::ASHRri4),
        High)
        .addReg(High)
        .addImm(1);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::SHL64ri4) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register LowPair = MI.getOperand(0).getReg();
    Register HighPair = MI.getOperand(1).getReg();
    Register Word0 = RI.getSubReg(LowPair, sub_lo16);
    Register Word1 = RI.getSubReg(LowPair, sub_hi16);
    Register Word2 = RI.getSubReg(HighPair, sub_lo16);
    Register Word3 = RI.getSubReg(HighPair, sub_hi16);
    unsigned Amount = MI.getOperand(4).getImm();
    assert((Amount == 2 || Amount == 3) && "invalid C166 i64 shift amount");

    for (unsigned I = 0; I != Amount; ++I) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHLri4), Word0)
          .addReg(Word0)
          .addImm(1);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDCrr), Word1)
          .addReg(Word1)
          .addReg(Word1);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDCrr), Word2)
          .addReg(Word2)
          .addReg(Word2);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDCrr), Word3)
          .addReg(Word3)
          .addReg(Word3);
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::SRL64ri1 || MI.getOpcode() == C166::SRA64ri1) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register LowPair = MI.getOperand(0).getReg();
    Register HighPair = MI.getOperand(1).getReg();
    Register HighPairForLow = MI.getOperand(4).getReg();
    Register Word0 = RI.getSubReg(LowPair, sub_lo16);
    Register Word1 = RI.getSubReg(LowPair, sub_hi16);
    Register Word2 = RI.getSubReg(HighPair, sub_lo16);
    Register Word3 = RI.getSubReg(HighPair, sub_hi16);
    Register Word2ForLow = RI.getSubReg(HighPairForLow, sub_lo16);

    if (!MI.getOperand(0).isDead()) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHRri4), Word0)
          .addReg(Word0)
          .addImm(1);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::BMOVreg), Word0)
          .addReg(Word0)
          .addReg(Word1)
          .addImm(15)
          .addImm(0);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHRri4), Word1)
          .addReg(Word1)
          .addImm(1);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::BMOVreg), Word1)
          .addReg(Word1)
          .addReg(Word2ForLow)
          .addImm(15)
          .addImm(0);
    }
    if (!MI.getOperand(1).isDead()) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHRri4), Word2)
          .addReg(Word2)
          .addImm(1);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::BMOVreg), Word2)
          .addReg(Word2)
          .addReg(Word3)
          .addImm(15)
          .addImm(0);
      BuildMI(
          MBB, MI, MI.getDebugLoc(),
          get(MI.getOpcode() == C166::SRL64ri1 ? C166::SHRri4 : C166::ASHRri4),
          Word3)
          .addReg(Word3)
          .addImm(1);
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::SHL32ri5 || MI.getOpcode() == C166::SRL32ri5 ||
      MI.getOpcode() == C166::SRA32ri5) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Scratch = MI.getOperand(1).getReg();
    unsigned Amount = MI.getOperand(3).getImm();
    Register Low = RI.getSubReg(Dst, sub_lo16);
    Register High = RI.getSubReg(Dst, sub_hi16);

    assert(Amount >= 1 && Amount <= 31 && "invalid C166 shift amount");
    if (Amount >= 16) {
      unsigned WordAmount = Amount - 16;
      if (MI.getOpcode() == C166::SHL32ri5) {
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), High).addReg(Low);
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVri4), Low).addImm(0);
        if (WordAmount)
          BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHLri4), High)
              .addReg(High)
              .addImm(WordAmount);
      } else {
        BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), Low).addReg(High);
        if (MI.getOpcode() == C166::SRL32ri5)
          BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVri4), High).addImm(0);
        else
          BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ASHRri4), High)
              .addReg(High)
              .addImm(15);
        if (WordAmount) {
          unsigned Opcode =
              MI.getOpcode() == C166::SRL32ri5 ? C166::SHRri4 : C166::ASHRri4;
          BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), Low)
              .addReg(Low)
              .addImm(WordAmount);
        }
      }
    } else if (MI.getOpcode() == C166::SHL32ri5) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), Scratch).addReg(Low);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHLri4), Low)
          .addReg(Low)
          .addImm(Amount);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHRri4), Scratch)
          .addReg(Scratch)
          .addImm(16 - Amount);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHLri4), High)
          .addReg(High)
          .addImm(Amount);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ORrr), High)
          .addReg(High)
          .addReg(Scratch);
    } else {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVrr), Scratch)
          .addReg(High);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHRri4), Low)
          .addReg(Low)
          .addImm(Amount);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::SHLri4), Scratch)
          .addReg(Scratch)
          .addImm(16 - Amount);
      unsigned Opcode =
          MI.getOpcode() == C166::SRL32ri5 ? C166::SHRri4 : C166::ASHRri4;
      BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), High)
          .addReg(High)
          .addImm(Amount);
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ORrr), Low)
          .addReg(Low)
          .addReg(Scratch);
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::FARADD32 || MI.getOpcode() == C166::FARADD32i) {
    MachineBasicBlock &MBB = *MI.getParent();
    Register Dst = MI.getOperand(0).getReg();
    Register Low = RI.getSubReg(Dst, sub_lo16);

    if (MI.getOpcode() == C166::FARADD32) {
      BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::ADDrr), Low)
          .addReg(Low)
          .addReg(MI.getOperand(2).getReg());
    } else {
      uint64_t Offset = MI.getOperand(2).getImm();
      BuildMI(MBB, MI, MI.getDebugLoc(),
              get(Offset <= 7 ? C166::ADDri3 : C166::ADDri16), Low)
          .addReg(Low)
          .addImm(Offset);
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::ADJSP) {
    MachineBasicBlock &MBB = *MI.getParent();
    unsigned Amount = MI.getOperand(0).getImm();
    unsigned Opcode = Amount <= 7 ? C166::ADDri3 : C166::ADDri16;
    BuildMI(MBB, MI, MI.getDebugLoc(), get(Opcode), C166::R0)
        .addReg(C166::R0)
        .addImm(Amount);
    emitDynamicUserStackCFI(MI, *this, 0);
    MI.eraseFromParent();
    return true;
  }

  return false;
}

bool C166InstrInfo::expandCallFramePseudo(MachineInstr &MI) const {
  if (MI.getOpcode() == C166::ALLOCSP) {
    MachineBasicBlock &MBB = *MI.getParent();
    const uint64_t Amount = MI.getOperand(0).getImm();
    const uint64_t DynamicOffset = MI.getOperand(1).getImm();
    BuildMI(MBB, MI, MI.getDebugLoc(),
            get(Amount <= 7 ? C166::SUBri3 : C166::SUBri16), C166::R0)
        .addReg(C166::R0)
        .addImm(Amount);
    emitDynamicUserStackCFI(MI, *this, DynamicOffset);
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::PUSHARG) {
    MachineBasicBlock &MBB = *MI.getParent();
    const uint64_t DynamicOffset = MI.getOperand(0).getImm();
    BuildMI(MBB, MI, MI.getDebugLoc(), get(C166::MOVmrPreDec), C166::R0)
        .addReg(C166::R0)
        .add(MI.getOperand(1));
    emitDynamicUserStackCFI(MI, *this, DynamicOffset);
    MI.eraseFromParent();
    return true;
  }

  return false;
}
