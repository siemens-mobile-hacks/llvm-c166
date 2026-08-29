//===-- C166FrameLowering.cpp - C166 frame lowering ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166FrameLowering.h"
#include "C166.h"
#include "C166CFI.h"
#include "C166InstrInfo.h"
#include "C166MachineFunctionInfo.h"
#include "C166Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/TargetParser/C166TargetParser.h"

using namespace llvm;

static bool isInterruptHandler(const MachineFunction &MF) {
  return MF.getFunction().getCallingConv() == CallingConv::C166_Interrupt;
}

static bool hasNamedRegisterBank(const MachineFunction &MF) {
  return isInterruptHandler(MF) &&
         MF.getFunction().hasFnAttribute("c166-register-bank");
}

bool C166FrameLowering::needsFrameIndexResolution(
    const MachineFunction &MF) const {
  // Frame-index resolution also removes non-reserved call-frame pseudos.  A
  // leaf-sized caller may have an outgoing area but no local stack objects,
  // so the generic stack-object-only criterion is insufficient here.
  return TargetFrameLowering::needsFrameIndexResolution(MF) ||
         MF.getFrameInfo().adjustsStack();
}

bool C166FrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *TRI,
    std::vector<CalleeSavedInfo> &CSI) const {
  if (!isInterruptHandler(MF))
    return false;

  // Interrupt callee-saves are pushed on the CPU system stack, not reserved
  // in the R0 user-stack frame.  Fixed spill objects describe their actual
  // entry-SP-relative positions to PEI without increasing user StackSize.
  int64_t Offset = hasNamedRegisterBank(MF) ? -4 : -2;
  for (CalleeSavedInfo &Info : CSI) {
    int FrameIndex = MF.getFrameInfo().CreateFixedSpillStackObject(2, Offset);
    Info.setFrameIdx(FrameIndex);
    Offset -= 2;
  }
  return true;
}

static void emitR0CFI(MachineFunction &MF, MachineBasicBlock &MBB,
                      MachineBasicBlock::iterator I, const DebugLoc &DL,
                      const C166InstrInfo &TII, uint64_t CallerOffset,
                      MachineInstr::MIFlag Flag) {
  if (!MF.needsFrameMoves())
    return;
  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  const unsigned DwarfR0 = MRI->getDwarfRegNum(C166::R0, true);
  MCCFIInstruction Inst =
      CallerOffset ? C166CFI::createUserStackValue(DwarfR0, CallerOffset)
                   : MCCFIInstruction::createRestore(nullptr, DwarfR0);
  C166CFI::build(MBB, I, DL, TII, Inst, Flag);
}

static void emitNearSystemStackCFI(MachineFunction &MF, MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator I,
                                   const DebugLoc &DL,
                                   const C166InstrInfo &TII) {
  if (!MF.needsFrameMoves() ||
      MF.getFunction().getAddressSpace() != C166::NearAddressSpace)
    return;

  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  const unsigned DwarfSP = MRI->getDwarfRegNum(C166::SP, true);
  const unsigned DwarfCSP = MRI->getDwarfRegNum(C166::CSP, true);
  const unsigned DwarfRA = MRI->getDwarfRegNum(C166::RA, true);

  // The common CIE describes the default Large CALLS/RETS entry.  A near
  // function is entered through CALLA/CALLI/CALLR instead, which pushes only
  // IP.  Override every rule that depends on the system-stack delta at the
  // first address in this FDE.
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::cfiDefCfa(nullptr, DwarfSP, 2));
  C166CFI::build(MBB, I, DL, TII,
                 C166CFI::createNearReturnAddress(DwarfRA, DwarfCSP));
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::createSameValue(nullptr, DwarfCSP));
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::createValOffset(nullptr, DwarfSP, 0));
}

static void emitInterruptSystemStackCFI(MachineFunction &MF,
                                        MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator I,
                                        const DebugLoc &DL,
                                        const C166InstrInfo &TII) {
  if (!MF.needsFrameMoves() || !isInterruptHandler(MF))
    return;

  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  const unsigned DwarfSP = MRI->getDwarfRegNum(C166::SP, true);
  const unsigned DwarfCSP = MRI->getDwarfRegNum(C166::CSP, true);
  const unsigned DwarfRA = MRI->getDwarfRegNum(C166::RA, true);
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::cfiDefCfa(nullptr, DwarfSP, 6),
                 MachineInstr::FrameSetup);
  C166CFI::build(MBB, I, DL, TII,
                 C166CFI::createInterruptReturnAddress(DwarfRA),
                 MachineInstr::FrameSetup);
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::createOffset(nullptr, DwarfCSP, -4),
                 MachineInstr::FrameSetup);
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::createValOffset(nullptr, DwarfSP, 0),
                 MachineInstr::FrameSetup);
}

static void adjustUserStack(MachineFunction &MF, MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator I, const DebugLoc &DL,
                            const C166InstrInfo &TII, uint64_t Amount,
                            bool Allocate, uint64_t CallerOffset) {
  MachineInstr::MIFlag Flag =
      Allocate ? MachineInstr::FrameSetup : MachineInstr::FrameDestroy;
  unsigned Opcode;
  if (Allocate)
    Opcode = Amount <= 7 ? C166::SUBri3 : C166::SUBri16;
  else
    Opcode = Amount <= 7 ? C166::ADDri3 : C166::ADDri16;
  BuildMI(MBB, I, DL, TII.get(Opcode), C166::R0)
      .addReg(C166::R0)
      .addImm(Amount)
      .setMIFlag(Flag);
  emitR0CFI(MF, MBB, I, DL, TII, CallerOffset, Flag);
}

void C166FrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  unsigned CSSize =
      MF.getInfo<C166MachineFunctionInfo>()->getCalleeSavedFrameSize();
  uint64_t LocalSize = StackSize - CSSize;
  const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
  MachineBasicBlock::iterator I = MBB.begin();
  const DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  const bool IsInterrupt = isInterruptHandler(MF);
  if (IsInterrupt) {
    emitInterruptSystemStackCFI(MF, MBB, I, DL, TII);
    if (hasNamedRegisterBank(MF)) {
      // Keep the synthetic hardware-frame CFI at the true entry point, then
      // switch CP before any generated callee-save or body instruction.
      I = MBB.begin();
      while (I != MBB.end() && I->isMetaInstruction())
        ++I;

      StringRef Bank = MF.getFunction()
                           .getFnAttribute("c166-register-bank")
                           .getValueAsString();
      const char *BankSymbol = MF.createExternalSymbolName(Bank);
      MBB.addLiveIn(C166::R0);
      MBB.addLiveIn(C166::CP);
      BuildMI(MBB, I, DL, TII.get(C166::MOVabsdg))
          .addExternalSymbol(BankSymbol)
          .addReg(C166::R0)
          .setMIFlag(MachineInstr::FrameSetup);
      MachineInstrBuilder Switch =
          BuildMI(MBB, I, DL, TII.get(C166::SCXTri16), C166::CP)
              .addReg(C166::CP, RegState::Kill)
              .addExternalSymbol(BankSymbol)
              .setMIFlag(MachineInstr::FrameSetup);

      // SCXT CP delays the new register-bank mapping by one instruction.  A
      // callee-save operation accesses only the system stack/SFR space and
      // fills that slot. A banked leaf needs NOP.
      if (MF.getFrameInfo().getCalleeSavedInfo().empty())
        BuildMI(MBB, I, DL, TII.get(C166::NOP))
            .setMIFlag(MachineInstr::FrameSetup);
      if (MF.needsFrameMoves()) {
        auto AfterSwitch = std::next(Switch->getIterator());
        C166CFI::build(MBB, AfterSwitch, DL, TII,
                       MCCFIInstruction::cfiDefCfaOffset(nullptr, 8),
                       MachineInstr::FrameSetup);
      }
    }
    I = MBB.begin();
    while (I != MBB.end() && I->getFlag(MachineInstr::FrameSetup))
      ++I;
  } else {
    emitNearSystemStackCFI(MF, MBB, I, DL, TII);
    while (I != MBB.end() && I->getOpcode() == C166::MOVmrPreDec &&
           I->getFlag(MachineInstr::FrameSetup))
      ++I;
  }

  if (!StackSize)
    return;
  // The user stack is in the 14-bit DPP1 page. A frame
  // may fill that page, but it must never make R0 address another DPP page.
  if (StackSize > 0x4000) {
    MF.getFunction().getContext().emitError(
        "C166 automatic data exceeds the 16K user stack");
    return;
  }
  if (LocalSize)
    adjustUserStack(MF, MBB, I, DL, TII, LocalSize, true, StackSize);
  else
    emitR0CFI(MF, MBB, I, DL, TII, CSSize, MachineInstr::FrameSetup);

  if (!MF.needsFrameMoves() || IsInterrupt)
    return;

  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  const unsigned DwarfDPP1 = MRI->getDwarfRegNum(C166::DPP1, true);
  ArrayRef<CalleeSavedInfo> CSI = MF.getFrameInfo().getCalleeSavedInfo();
  for (auto [Index, Info] : llvm::enumerate(CSI)) {
    if (Info.isSpilledToReg())
      continue;
    unsigned DwarfReg = MRI->getDwarfRegNum(Info.getReg(), true);
    int64_t Offset = LocalSize + 2 * (CSI.size() - Index - 1);
    C166CFI::build(
        MBB, I, DL, TII,
        C166CFI::createUserStackLocation(DwarfReg, Offset, DwarfDPP1),
        MachineInstr::FrameSetup);
  }
}

void C166FrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  unsigned CSSize =
      MF.getInfo<C166MachineFunctionInfo>()->getCalleeSavedFrameSize();
  uint64_t LocalSize = StackSize - CSSize;
  const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
  MachineBasicBlock::iterator I = MBB.getFirstTerminator();
  const DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  const bool IsInterrupt = isInterruptHandler(MF);
  if (StackSize && StackSize <= 0x4000 && IsInterrupt) {
    for (MachineBasicBlock::iterator Candidate = MBB.begin();
         Candidate != MBB.end(); ++Candidate) {
      if (Candidate->getOpcode() == C166::POP &&
          Candidate->getFlag(MachineInstr::FrameDestroy)) {
        I = Candidate;
        break;
      }
    }
  }

  if (StackSize && StackSize <= 0x4000) {
    if (!IsInterrupt && CSSize) {
      MachineBasicBlock::iterator FirstRestore = I;
      while (FirstRestore != MBB.begin()) {
        MachineBasicBlock::iterator Previous = std::prev(FirstRestore);
        if (Previous->getOpcode() != C166::MOVrmPostInc ||
            !Previous->getFlag(MachineInstr::FrameDestroy))
          break;
        FirstRestore = Previous;
      }
      I = FirstRestore;
    }

    if (LocalSize)
      adjustUserStack(MF, MBB, I, DL, TII, LocalSize, false, CSSize);

    if (!IsInterrupt && CSSize && MF.needsFrameMoves()) {
      const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
      unsigned Remaining = CSSize;
      for (MachineBasicBlock::iterator Restore = I;
           Restore != MBB.getFirstTerminator() && Remaining;) {
        if (Restore->getOpcode() != C166::MOVrmPostInc) {
          ++Restore;
          continue;
        }
        unsigned DwarfReg =
            MRI->getDwarfRegNum(Restore->getOperand(0).getReg(), true);
        MachineBasicBlock::iterator AfterRestore = std::next(Restore);
        C166CFI::build(MBB, AfterRestore, DL, TII,
                       MCCFIInstruction::createRestore(nullptr, DwarfReg),
                       MachineInstr::FrameDestroy);
        Remaining -= 2;
        emitR0CFI(MF, MBB, AfterRestore, DL, TII, Remaining,
                  MachineInstr::FrameDestroy);
        Restore = AfterRestore;
      }
      assert(!Remaining && "C166 callee-save restore was not found");
    }
  }

  if (hasNamedRegisterBank(MF)) {
    I = MBB.getFirstTerminator();
    MachineInstrBuilder Pop = BuildMI(MBB, I, DL, TII.get(C166::POP), C166::CP)
                                  .setMIFlag(MachineInstr::FrameDestroy);
    if (MF.needsFrameMoves()) {
      auto AfterPop = std::next(Pop->getIterator());
      C166CFI::build(MBB, AfterPop, DL, TII,
                     MCCFIInstruction::cfiDefCfaOffset(nullptr, 6),
                     MachineInstr::FrameDestroy);
    }
  }
}

bool C166FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  MachineFunction &MF = *MBB.getParent();
  if (!isInterruptHandler(MF)) {
    if (CSI.empty())
      return false;
    const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
    MF.getInfo<C166MachineFunctionInfo>()->setCalleeSavedFrameSize(CSI.size() *
                                                                   2);
    DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
    for (const CalleeSavedInfo &Info : CSI) {
      MCRegister Reg = Info.getReg();
      MBB.addLiveIn(Reg);
      BuildMI(MBB, MI, DL, TII.get(C166::MOVmrPreDec), C166::R0)
          .addReg(C166::R0)
          .addReg(Reg, RegState::Kill)
          .setMIFlag(MachineInstr::FrameSetup);
    }
    return true;
  }
  const bool HasBank = hasNamedRegisterBank(MF);
  if (CSI.empty())
    return false;

  const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
  int64_t CFAOffset = HasBank ? 8 : 6;
  for (const CalleeSavedInfo &I : CSI) {
    MCRegister Reg = I.getReg();
    MBB.addLiveIn(Reg);
    MachineInstrBuilder Push =
        Reg == C166::MDC ? BuildMI(MBB, MI, DL, TII.get(C166::SCXTri16), Reg)
                               .addReg(Reg, RegState::Kill)
                               .addImm(0x10)
                               .setMIFlag(MachineInstr::FrameSetup)
                         : BuildMI(MBB, MI, DL, TII.get(C166::PUSH))
                               .addReg(Reg, RegState::Kill)
                               .setMIFlag(MachineInstr::FrameSetup);
    CFAOffset += 2;
    if (!MF.needsFrameMoves())
      continue;
    auto AfterPush = std::next(Push->getIterator());
    C166CFI::build(MBB, AfterPush, DL, TII,
                   MCCFIInstruction::cfiDefCfaOffset(nullptr, CFAOffset),
                   MachineInstr::FrameSetup);
    int DwarfReg = MRI->getDwarfRegNum(Reg, true);
    if (DwarfReg >= 0)
      C166CFI::build(
          MBB, AfterPush, DL, TII,
          MCCFIInstruction::createOffset(nullptr, DwarfReg, -CFAOffset),
          MachineInstr::FrameSetup);
  }
  return true;
}

bool C166FrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  MachineFunction &MF = *MBB.getParent();
  if (!isInterruptHandler(MF)) {
    if (CSI.empty())
      return false;
    const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
    DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
    for (const CalleeSavedInfo &Info : llvm::reverse(CSI))
      BuildMI(MBB, MI, DL, TII.get(C166::MOVrmPostInc), Info.getReg())
          .addReg(C166::R0, RegState::Define)
          .addReg(C166::R0)
          .setMIFlag(MachineInstr::FrameDestroy);
    return true;
  }
  const bool HasBank = hasNamedRegisterBank(MF);
  if (CSI.empty())
    return false;

  const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
  int64_t CFAOffset = 6 + 2 * CSI.size() + (HasBank ? 2 : 0);
  for (const CalleeSavedInfo &I : llvm::reverse(CSI)) {
    MCRegister Reg = I.getReg();
    MachineInstrBuilder Pop = BuildMI(MBB, MI, DL, TII.get(C166::POP), Reg)
                                  .setMIFlag(MachineInstr::FrameDestroy);
    if (MF.needsFrameMoves()) {
      auto AfterPop = std::next(Pop->getIterator());
      int DwarfReg = MRI->getDwarfRegNum(Reg, true);
      if (DwarfReg >= 0)
        C166CFI::build(MBB, AfterPop, DL, TII,
                       MCCFIInstruction::createRestore(nullptr, DwarfReg),
                       MachineInstr::FrameDestroy);
      CFAOffset -= 2;
      C166CFI::build(MBB, AfterPop, DL, TII,
                     MCCFIInstruction::cfiDefCfaOffset(nullptr, CFAOffset),
                     MachineInstr::FrameDestroy);
    } else {
      CFAOffset -= 2;
    }
  }
  return true;
}

MachineBasicBlock::iterator C166FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  // Caller cleanup and fixed-frame deallocation are separated only by the
  // call-sequence marker.  Merge them while PEI removes that marker.
  if (I->getOpcode() == C166::ADJCALLSTACKUP && I != MBB.begin()) {
    MachineBasicBlock::iterator Cleanup = std::prev(I);
    MachineBasicBlock::iterator Deallocate = std::next(I);
    if (Cleanup->getOpcode() == C166::ADJSP && Cleanup->getOperand(0).isImm() &&
        Deallocate != MBB.end() &&
        (Deallocate->getOpcode() == C166::ADDri3 ||
         Deallocate->getOpcode() == C166::ADDri16) &&
        Deallocate->getFlag(MachineInstr::FrameDestroy) &&
        Deallocate->getOperand(2).isImm()) {
      uint64_t CleanupSize = Cleanup->getOperand(0).getImm();
      uint64_t FrameSize = Deallocate->getOperand(2).getImm();
      if (CleanupSize == static_cast<uint64_t>(I->getOperand(0).getImm()) &&
          isUInt<16>(CleanupSize + FrameSize)) {
        uint64_t CombinedSize = CleanupSize + FrameSize;
        const C166InstrInfo &TII =
            *MF.getSubtarget<C166Subtarget>().getInstrInfo();
        Deallocate->setDesc(
            TII.get(CombinedSize <= 7 ? C166::ADDri3 : C166::ADDri16));
        Deallocate->getOperand(2).setImm(CombinedSize);
        Cleanup->eraseFromParent();
      }
    }
  }
  return MBB.erase(I);
}
