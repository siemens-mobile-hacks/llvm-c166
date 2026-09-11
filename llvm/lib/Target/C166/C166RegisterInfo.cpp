//===-- C166RegisterInfo.cpp - C166 register information -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166RegisterInfo.h"
#include "C166FrameLowering.h"
#include "C166InstrInfo.h"
#include "C166Subtarget.h"
#include "MCTargetDesc/C166MCTargetDesc.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/TargetParser/C166TargetParser.h"

#define GET_REGINFO_TARGET_DESC
#include "C166GenRegisterInfo.inc"

using namespace llvm;

C166RegisterInfo::C166RegisterInfo() : C166GenRegisterInfo(0) {}

const uint16_t *
C166RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  if (!MF || MF->getFunction().getCallingConv() != CallingConv::C166_Interrupt)
    return CSR_C166_SaveList;
  return MF->getFunction().hasFnAttribute("c166-register-bank")
             ? CSR_C166_InterruptBank_SaveList
             : CSR_C166_Interrupt_SaveList;
}

const uint32_t *
C166RegisterInfo::getCallPreservedMask(const MachineFunction &,
                                       CallingConv::ID CC) const {
  return CC == CallingConv::C166_Interrupt
             ? CSR_C166_InterruptCallPreserved_RegMask
             : CSR_C166_CallPreserved_RegMask;
}

BitVector C166RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  // R0 is the user-stack pointer. Its directly addressable
  // byte lanes belong to GR8, so reserve them explicitly as well; marking
  // super-registers alone does not walk down to subregisters.
  Reserved.set(C166::RL0);
  Reserved.set(C166::RH0);
  Reserved.set(C166::R0);
  Reserved.set(C166::DPP0);
  Reserved.set(C166::DPP1);
  Reserved.set(C166::DPP2);
  Reserved.set(C166::DPP3);
  Reserved.set(C166::CSP);
  Reserved.set(C166::MDH);
  Reserved.set(C166::MDL);
  Reserved.set(C166::CP);
  Reserved.set(C166::SP);
  Reserved.set(C166::STKOV);
  Reserved.set(C166::STKUN);
  Reserved.set(C166::MDC);
  Reserved.set(C166::PSW);
  for (MCRegister Reg :
       {C166::RL0, C166::RH0, C166::R0, C166::DPP0, C166::DPP1, C166::DPP2,
        C166::DPP3, C166::CSP, C166::MDH, C166::MDL, C166::CP, C166::SP,
        C166::STKOV, C166::STKUN, C166::MDC, C166::PSW})
    markSuperRegs(Reserved, Reg);
  const auto *TFI = MF.getSubtarget<C166Subtarget>().getFrameLowering();
  if (TFI->hasFP(MF)) {
    Reserved.set(C166::RL6);
    Reserved.set(C166::RH6);
    Reserved.set(C166::R6);
    markSuperRegs(Reserved, C166::R6);
  }
  return Reserved;
}

const TargetRegisterClass *
C166RegisterInfo::getPointerRegClass(unsigned Kind) const {
  return &C166::GR32RegClass;
}

bool C166RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                           int SPAdj, unsigned FIOperandNum,
                                           RegScavenger *) const {
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();

  int FI = MI.getOperand(FIOperandNum).getIndex();
  const Register FrameReg = getFrameRegister(MF);
  int64_t Offset = MFI.getObjectOffset(FI) + MFI.getStackSize();
  if (FrameReg == C166::R0)
    Offset += SPAdj;
  if (FIOperandNum + 1 < MI.getNumOperands() &&
      MI.getOperand(FIOperandNum + 1).isImm())
    Offset += MI.getOperand(FIOperandNum + 1).getImm();

  // The two high address bits select a DPP register.  User-stack references
  // must remain within DPP1 rather than wrapping into another 16K page.
  if (!isUInt<14>(Offset)) {
    if (MFI.getStackSize() <= 0x4000)
      MF.getFunction().getContext().emitError(
          "C166 user-stack offset exceeds the DPP1 page");
    // Keep the invalid instruction structurally well-formed while LLVM
    // finishes reporting the diagnostic; no object is emitted on error.
    Offset = 0;
  }

  if (MI.getOpcode() == C166::FRAMELOAD8Z ||
      MI.getOpcode() == C166::FRAMELOAD8S ||
      MI.getOpcode() == C166::FRAMESTORE8 ||
      MI.getOpcode() == C166::FRAMELOAD32 ||
      MI.getOpcode() == C166::FRAMESTORE32) {
    // Keep the byte-access pseudo until post-RA expansion, where the
    // allocated word register determines the addressable RL/RH subregister.
    // The frame index itself is already fully resolved to the C166 user
    // fixed-frame base here.
    unsigned Opcode;
    switch (MI.getOpcode()) {
    case C166::FRAMELOAD8Z:
      Opcode = C166::NEARLOAD8Z;
      break;
    case C166::FRAMELOAD8S:
      Opcode = C166::NEARLOAD8S;
      break;
    case C166::FRAMESTORE8:
      Opcode = C166::NEARSTORE8;
      break;
    case C166::FRAMELOAD32:
      Opcode = C166::NEARLOAD32;
      break;
    case C166::FRAMESTORE32:
      Opcode = C166::NEARSTORE32;
      break;
    default:
      llvm_unreachable("unexpected C166 frame pseudo");
    }
    MI.setDesc(TII.get(Opcode));
    // FRAMELOAD32 is allocated while its address is still a frame index, so
    // it does not need an early-clobber constraint.  NEARLOAD32 does; once
    // the frame index becomes a reserved physical register, mark the already
    // allocated result
    // consistently with the replacement instruction descriptor.
    if (Opcode == C166::NEARLOAD32)
      MI.getOperand(0).setIsEarlyClobber(true);
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
    MI.getOperand(FIOperandNum + 1).setImm(Offset);
    return false;
  }

  if (MI.getOpcode() == C166::FRAMEADDR32) {
    if (MI.getOperand(0).isDead()) {
      MI.eraseFromParent();
      return true;
    }
    Register Pair = MI.getOperand(0).getReg();
    Register Low = getSubReg(Pair, sub_lo16);
    Register High = getSubReg(Pair, sub_hi16);
    MachineBasicBlock &MBB = *MI.getParent();

    if (Offset >= 8 && Offset <= 15) {
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::MOVri4), Low)
          .addImm(Offset);
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::ADDrr), Low)
          .addReg(Low)
          .addReg(FrameReg);
    } else {
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::MOVrr), Low)
          .addReg(FrameReg);
    }
    if (Offset && (Offset < 8 || Offset > 15))
      BuildMI(MBB, II, MI.getDebugLoc(),
              TII.get(Offset <= 7 ? C166::ADDri3 : C166::ADDri16), Low)
          .addReg(Low)
          .addImm(Offset);
    BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::ANDri16), Low)
        .addReg(Low)
        .addImm(0x3fff);
    if (MI.getMF()->getDataLayout().getAllocaAddrSpace() ==
        C166::HugeDataAddressSpace) {
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::MOVgsfr), High)
          .addReg(C166::DPP1);
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::SHLri4), High)
          .addReg(High)
          .addImm(14);
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::ORrr), Low)
          .addReg(Low)
          .addReg(High);
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::MOVgsfr), High)
          .addReg(C166::DPP1);
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::SHRri4), High)
          .addReg(High)
          .addImm(2);
    } else {
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::MOVgsfr), High)
          .addReg(C166::DPP1);
    }
    MI.eraseFromParent();
    return true;
  }

  if (MI.getOpcode() == C166::LEAfi) {
    if (MI.getOperand(0).isDead()) {
      MI.eraseFromParent();
      return true;
    }
    Register Dst = MI.getOperand(0).getReg();
    if (Offset >= 8 && Offset <= 15) {
      MachineBasicBlock &MBB = *MI.getParent();
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::MOVri4), Dst)
          .addImm(Offset);
      BuildMI(MBB, II, MI.getDebugLoc(), TII.get(C166::ADDrr), Dst)
          .addReg(Dst)
          .addReg(FrameReg);
      MI.eraseFromParent();
      return true;
    }
    MI.setDesc(TII.get(C166::MOVrr));
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
    MI.removeOperand(FIOperandNum + 1);

    if (Offset) {
      assert(isUInt<16>(Offset) && "C166 frame address is out of range");
      MachineBasicBlock::iterator InsertAt = std::next(II);
      BuildMI(*MI.getParent(), InsertAt, MI.getDebugLoc(),
              TII.get(Offset <= 7 ? C166::ADDri3 : C166::ADDri16), Dst)
          .addReg(Dst)
          .addImm(Offset);
    }
    return false;
  }

  bool IsByteStore = MI.getOpcode() == C166::MOVBfiStore;
  bool IsByteLoad = MI.getOpcode() == C166::MOVBfi;
  if (IsByteStore || IsByteLoad) {
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
    if (Offset == 0) {
      MI.setDesc(TII.get(IsByteStore ? C166::MOVBmr : C166::MOVBrm));
      MI.removeOperand(FIOperandNum + 1);
    } else {
      MI.setDesc(TII.get(IsByteStore ? C166::MOVBmr16 : C166::MOVBrm16));
      MI.getOperand(FIOperandNum + 1).setImm(Offset);
    }
    return false;
  }

  bool IsStore = MI.getOpcode() == C166::MOVfiStore;
  assert((IsStore || MI.getOpcode() == C166::MOVfi) &&
         "unexpected C166 frame-index instruction");
  MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
  if (Offset == 0) {
    MI.setDesc(TII.get(IsStore ? C166::MOVmr : C166::MOVrm));
    MI.removeOperand(FIOperandNum + 1);
  } else {
    MI.setDesc(TII.get(IsStore ? C166::MOVmr16 : C166::MOVrm16));
    MI.getOperand(FIOperandNum + 1).setImm(Offset);
  }
  return false;
}

Register C166RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const auto *TFI = MF.getSubtarget<C166Subtarget>().getFrameLowering();
  return TFI->hasFP(MF) ? C166::R6 : C166::R0;
}
