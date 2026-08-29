//===-- C166PostISel.cpp - C166 post-selection optimizations -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166InstrInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "c166-post-isel"

namespace {

static MachineInstr *getAddImmediateDef(Register Reg,
                                        MachineRegisterInfo &MRI) {
  if (!Reg.isVirtual())
    return nullptr;
  MachineInstr *Def = MRI.getUniqueVRegDef(Reg);
  if (!Def ||
      (Def->getOpcode() != C166::ADDri3 && Def->getOpcode() != C166::ADDri16))
    return nullptr;
  return Def;
}

static MachineInstr *getCompareBranch(Register Reg, MachineRegisterInfo &MRI) {
  for (MachineInstr &Use : MRI.use_nodbg_instructions(Reg)) {
    if ((Use.getOpcode() == C166::CMPBR || Use.getOpcode() == C166::CMPBRi) &&
        Use.getOperand(0).getReg() == Reg)
      return &Use;
  }
  return nullptr;
}

static bool foldShortImmediatePhis(MachineFunction &MF,
                                   const C166InstrInfo &TII) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &Phi : MBB.phis()) {
      if (Phi.getNumOperands() != 5)
        continue;

      for (unsigned BaselineOperand : {1u, 3u}) {
        unsigned OtherOperand = BaselineOperand == 1 ? 3 : 1;
        Register BaselineReg = Phi.getOperand(BaselineOperand).getReg();
        Register OtherReg = Phi.getOperand(OtherOperand).getReg();
        MachineInstr *Baseline = getAddImmediateDef(BaselineReg, MRI);
        MachineInstr *Other = getAddImmediateDef(OtherReg, MRI);
        MachineInstr *Branch = getCompareBranch(BaselineReg, MRI);
        if (!Baseline || !Other || !Branch ||
            Baseline->getParent() != Other->getParent() ||
            Baseline->getParent() != Branch->getParent() ||
            Baseline->getOperand(1).getReg() != Other->getOperand(1).getReg() ||
            !MRI.hasOneNonDBGUse(OtherReg))
          continue;

        uint16_t BaselineImmediate =
            static_cast<uint16_t>(Baseline->getOperand(2).getImm());
        uint16_t OtherImmediate =
            static_cast<uint16_t>(Other->getOperand(2).getImm());
        uint16_t Adjustment = OtherImmediate - BaselineImmediate;
        if (Adjustment == 0 || Adjustment > 7)
          continue;

        // C166 has a two-byte ADD immediate for 0..7.  Express the alternate
        // PHI value relative to the value already needed by the comparison;
        // MachineSink can then place this short adjustment on its CFG edge.
        Other->setDesc(TII.get(C166::ADDri3));
        Other->getOperand(1).setReg(BaselineReg);
        Other->getOperand(2).setImm(Adjustment);
        Other->clearFlag(MachineInstr::NoSWrap);
        Other->clearFlag(MachineInstr::NoUWrap);
        Other->moveBefore(Branch);
        Changed = true;
        break;
      }
    }
  }
  return Changed;
}

class C166PostISel : public MachineFunctionPass {
public:
  static char ID;

  C166PostISel() : MachineFunctionPass(ID) {
    initializeC166PostISelPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "C166 post-selection optimizations";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char C166PostISel::ID = 0;

INITIALIZE_PASS(C166PostISel, DEBUG_TYPE, "C166 post-selection optimizations",
                false, false)

FunctionPass *llvm::createC166PostISelPass() { return new C166PostISel(); }

bool C166PostISel::runOnMachineFunction(MachineFunction &MF) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto &TII =
      *static_cast<const C166InstrInfo *>(MF.getSubtarget().getInstrInfo());
  SmallVector<MachineInstr *, 8> DeadConstants;
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() != C166::FARADD32)
        continue;

      Register Offset = MI.getOperand(2).getReg();
      if (!Offset.isVirtual())
        continue;
      MachineInstr *Def = MRI.getUniqueVRegDef(Offset);
      if (!Def || (Def->getOpcode() != C166::MOVri4 &&
                   Def->getOpcode() != C166::MOVri16))
        continue;

      MI.setDesc(TII.get(C166::FARADD32i));
      MI.getOperand(2).ChangeToImmediate(Def->getOperand(1).getImm());
      if (MRI.use_empty(Offset))
        DeadConstants.push_back(Def);
      Changed = true;
    }
  }

  for (MachineInstr *MI : DeadConstants)
    if (MI->getParent())
      MI->eraseFromParent();

  Changed |= foldShortImmediatePhis(MF, TII);
  return Changed;
}
