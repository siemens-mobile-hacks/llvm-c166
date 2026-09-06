//===-- C166LongBranchOpt.cpp - C166 long branch optimization ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166InstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "c166-long-branch-opt"

namespace {

static unsigned getDirectBranchOpcode(unsigned SkipOpcode) {
  switch (SkipOpcode) {
  case C166::JMPR_EQ:
    return C166::JMPA_NE;
  case C166::JMPR_NE:
    return C166::JMPA_EQ;
  case C166::JMPR_N:
    return C166::JMPA_NN;
  case C166::JMPR_NN:
    return C166::JMPA_N;
  case C166::JMPR_ULT:
    return C166::JMPA_UGE;
  case C166::JMPR_UGE:
    return C166::JMPA_ULT;
  case C166::JMPR_UGT:
    return C166::JMPA_ULE;
  case C166::JMPR_ULE:
    return C166::JMPA_UGT;
  case C166::JMPR_SGT:
    return C166::JMPA_SLE;
  case C166::JMPR_SLE:
    return C166::JMPA_SGT;
  case C166::JMPR_SLT:
    return C166::JMPA_SGE;
  case C166::JMPR_SGE:
    return C166::JMPA_SLT;
  default:
    return 0;
  }
}

static MachineInstr *getOnlyInstruction(MachineBasicBlock &MBB) {
  MachineInstr *Only = nullptr;
  for (MachineInstr &MI : MBB) {
    if (MI.isDebugInstr())
      continue;
    if (Only)
      return nullptr;
    Only = &MI;
  }
  return Only;
}

static bool foldLongConditionalBranch(MachineBasicBlock &MBB,
                                      const C166InstrInfo &TII) {
  MachineBasicBlock *JumpBB = MBB.getNextNode();
  if (!JumpBB || JumpBB->pred_size() != 1 || *JumpBB->pred_begin() != &MBB ||
      JumpBB->succ_size() != 1 || MBB.succ_size() != 2 ||
      MBB.getSectionID() != JumpBB->getSectionID())
    return false;

  MachineBasicBlock *SkipBB = JumpBB->getNextNode();
  MachineBasicBlock *DestBB = *JumpBB->succ_begin();
  if (!SkipBB || SkipBB == DestBB || !MBB.isSuccessor(JumpBB) ||
      !MBB.isSuccessor(SkipBB) ||
      MBB.getSectionID() != SkipBB->getSectionID() ||
      MBB.getSectionID() != DestBB->getSectionID())
    return false;

  MachineInstr *Jump = getOnlyInstruction(*JumpBB);
  if (!Jump || Jump->getOpcode() != C166::JMPS ||
      Jump->getNumExplicitOperands() != 2 || !Jump->getOperand(0).isMBB() ||
      !Jump->getOperand(1).isMBB() || Jump->getOperand(0).getMBB() != DestBB ||
      Jump->getOperand(1).getMBB() != DestBB)
    return false;

  auto Branch = MBB.getLastNonDebugInstr();
  if (Branch == MBB.end())
    return false;

  unsigned DirectOpcode = getDirectBranchOpcode(Branch->getOpcode());
  if (!DirectOpcode || Branch->getNumExplicitOperands() != 1 ||
      !Branch->getOperand(0).isMBB() ||
      Branch->getOperand(0).getMBB() != SkipBB)
    return false;

  BuildMI(MBB, Branch, Branch->getDebugLoc(), TII.get(DirectOpcode))
      .addMBB(DestBB)
      .setMIFlags(Branch->getFlags() | Jump->getFlags());
  Branch->eraseFromParent();

  MBB.replaceSuccessor(JumpBB, DestBB);
  JumpBB->removeSuccessor(DestBB);
  JumpBB->getParent()->erase(JumpBB);
  return true;
}

class C166LongBranchOpt : public MachineFunctionPass {
public:
  static char ID;

  C166LongBranchOpt() : MachineFunctionPass(ID) {
    initializeC166LongBranchOptPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "C166 long branch optimization";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    if (MF.empty())
      return false;

    const auto &TII =
        *static_cast<const C166InstrInfo *>(MF.getSubtarget().getInstrInfo());
    bool Changed = false;
    for (MachineBasicBlock *MBB = &MF.front(); MBB;) {
      Changed |= foldLongConditionalBranch(*MBB, TII);
      MBB = MBB->getNextNode();
    }
    return Changed;
  }
};

} // end anonymous namespace

char C166LongBranchOpt::ID = 0;

INITIALIZE_PASS(C166LongBranchOpt, DEBUG_TYPE, "C166 long branch optimization",
                false, false)

FunctionPass *llvm::createC166LongBranchOptPass() {
  return new C166LongBranchOpt();
}
