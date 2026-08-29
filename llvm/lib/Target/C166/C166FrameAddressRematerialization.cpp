//===-- C166FrameAddressRematerialization.cpp - Shorten frame addresses ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166InstrInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "c166-frame-address-rematerialization"

namespace {

static bool isSupportedUse(const MachineInstr &MI) {
  return MI.isCopy() || MI.getOpcode() == C166::PUSHARG ||
         MI.getOpcode() == C166::ADDrr;
}

static bool getExtractedWord(const MachineInstr &MI, Register Address,
                             Register &Word, unsigned &SubReg) {
  if (!MI.isCopy() || !MI.getOperand(0).getReg().isVirtual() ||
      MI.getOperand(1).getReg() != Address)
    return false;
  SubReg = MI.getOperand(1).getSubReg();
  if (SubReg != sub_lo16 && SubReg != sub_hi16)
    return false;
  Word = MI.getOperand(0).getReg();
  return true;
}

static bool rematerializeFrameAddresses(MachineFunction &MF,
                                        const C166InstrInfo &TII) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  SmallVector<MachineInstr *, 8> Addresses;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.getOpcode() == C166::LEAfi || MI.getOpcode() == C166::FRAMEADDR32)
        Addresses.push_back(&MI);

  bool Changed = false;
  for (MachineInstr *Address : Addresses) {
    Register Original = Address->getOperand(0).getReg();
    if (!Original.isVirtual())
      continue;

    MachineBasicBlock *MBB = Address->getParent();
    DenseMap<Register, unsigned> ExtractedWords;
    bool Supported = true;
    for (const MachineInstr &Use : MRI.use_nodbg_instructions(Original)) {
      Register Word;
      unsigned SubReg;
      if (getExtractedWord(Use, Original, Word, SubReg)) {
        ExtractedWords.insert({Word, SubReg});
        continue;
      }
      if (Use.getParent() != MBB || Use.isPHI() || !isSupportedUse(Use)) {
        Supported = false;
        break;
      }
    }
    for (const auto &[Word, SubReg] : ExtractedWords) {
      (void)SubReg;
      if (!llvm::all_of(MRI.use_nodbg_instructions(Word),
                        [&](const MachineInstr &Use) {
                          return Use.getParent() == MBB && !Use.isPHI() &&
                                 isSupportedUse(Use);
                        })) {
        Supported = false;
        break;
      }
    }
    if (!Supported)
      continue;

    Register CurrentAddress = Original;
    DenseMap<unsigned, Register> CurrentWords;
    bool SawCall = false;
    for (MachineBasicBlock::iterator I = std::next(Address->getIterator());
         I != MBB->end(); ++I) {
      MachineInstr &MI = *I;
      if (MI.isCall()) {
        SawCall = true;
        CurrentAddress = Original;
        CurrentWords.clear();
        continue;
      }

      bool UsesAddress = false;
      bool UsesExtractedWord = false;
      for (MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isUse())
          continue;
        if (MO.getReg() == Original)
          UsesAddress = true;
        auto Word = ExtractedWords.find(MO.getReg());
        if (Word != ExtractedWords.end())
          UsesExtractedWord = true;
      }
      if (!UsesAddress && !UsesExtractedWord)
        continue;

      if (SawCall &&
          MBB->computeRegisterLiveness(TRI, C166::PSW,
                                       MachineBasicBlock::const_iterator(I)) ==
              MachineBasicBlock::LQR_Dead) {
        CurrentAddress = MRI.createVirtualRegister(MRI.getRegClass(Original));
        BuildMI(*MBB, I, MI.getDebugLoc(), TII.get(Address->getOpcode()),
                CurrentAddress)
            .add(Address->getOperand(1))
            .add(Address->getOperand(2));
        CurrentWords.clear();
        SawCall = false;
        Changed = true;
      }

      if (CurrentAddress == Original)
        continue;

      for (MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isUse())
          continue;
        if (MO.getReg() == Original) {
          MO.setReg(CurrentAddress);
          continue;
        }
        auto Word = ExtractedWords.find(MO.getReg());
        if (Word == ExtractedWords.end())
          continue;
        Register &CurrentWord = CurrentWords[Word->second];
        if (!CurrentWord) {
          CurrentWord = MRI.createVirtualRegister(MRI.getRegClass(Word->first));
          BuildMI(*MBB, I, MI.getDebugLoc(), TII.get(TargetOpcode::COPY),
                  CurrentWord)
              .addReg(CurrentAddress, RegState{}, Word->second);
        }
        MO.setReg(CurrentWord);
      }
    }
  }
  return Changed;
}

class C166FrameAddressRematerialization : public MachineFunctionPass {
public:
  static char ID;

  C166FrameAddressRematerialization() : MachineFunctionPass(ID) {
    initializeC166FrameAddressRematerializationPass(
        *PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "C166 frame address rematerialization";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const auto &TII =
        *static_cast<const C166InstrInfo *>(MF.getSubtarget().getInstrInfo());
    return rematerializeFrameAddresses(MF, TII);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char C166FrameAddressRematerialization::ID = 0;

INITIALIZE_PASS(C166FrameAddressRematerialization, DEBUG_TYPE,
                "C166 frame address rematerialization", false, false)

FunctionPass *llvm::createC166FrameAddressRematerializationPass() {
  return new C166FrameAddressRematerialization();
}
