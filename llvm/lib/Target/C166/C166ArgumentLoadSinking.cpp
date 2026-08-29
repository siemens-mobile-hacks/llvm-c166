//===-- C166ArgumentLoadSinking.cpp - Sink immutable argument loads -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "c166-argument-load-sinking"

namespace {

class C166ArgumentLoadSinking : public MachineFunctionPass {
public:
  static char ID;

  C166ArgumentLoadSinking() : MachineFunctionPass(ID) {
    initializeC166ArgumentLoadSinkingPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "C166 argument load sinking";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char C166ArgumentLoadSinking::ID = 0;

INITIALIZE_PASS_BEGIN(C166ArgumentLoadSinking, DEBUG_TYPE,
                      "C166 argument load sinking", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(C166ArgumentLoadSinking, DEBUG_TYPE,
                    "C166 argument load sinking", false, false)

FunctionPass *llvm::createC166ArgumentLoadSinkingPass() {
  return new C166ArgumentLoadSinking();
}

static bool isImmutableFixedStackLoad(const MachineInstr &MI,
                                      const MachineFrameInfo &MFI) {
  switch (MI.getOpcode()) {
  case C166::MOVfi:
  case C166::MOVBfi:
  case C166::FRAMELOAD8Z:
  case C166::FRAMELOAD8S:
  case C166::FRAMELOAD32:
    break;
  default:
    return false;
  }

  if (!MI.getOperand(0).isReg() || MI.getOperand(0).getSubReg() ||
      !MI.getOperand(1).isFI())
    return false;
  int FI = MI.getOperand(1).getIndex();
  return MFI.isFixedObjectIndex(FI) && MFI.isImmutableObjectIndex(FI);
}

static MachineInstr *getSingleFullCopyUser(Register Value,
                                           MachineRegisterInfo &MRI) {
  MachineInstr *Copy = nullptr;
  for (MachineOperand &Use : MRI.use_nodbg_operands(Value)) {
    if (Copy && Copy != Use.getParent())
      return nullptr;
    Copy = Use.getParent();
  }
  if (!Copy || Copy->getOpcode() != TargetOpcode::COPY ||
      !Copy->getOperand(0).isReg() || Copy->getOperand(0).getSubReg() ||
      !Copy->getOperand(1).isReg() || Copy->getOperand(1).getReg() != Value ||
      Copy->getOperand(1).getSubReg())
    return nullptr;
  return Copy;
}

static MachineBasicBlock *getSingleUseBlock(Register Value,
                                            MachineRegisterInfo &MRI) {
  MachineBasicBlock *Destination = nullptr;
  for (MachineOperand &Use : MRI.use_nodbg_operands(Value)) {
    MachineInstr *User = Use.getParent();
    if (User->isPHI())
      return nullptr;
    if (!Destination)
      Destination = User->getParent();
    else if (Destination != User->getParent())
      return nullptr;
  }
  return Destination;
}

static bool sinkFixedStackPairs(MachineFunction &MF, MachineLoopInfo &MLI,
                                const TargetRegisterInfo *TRI) {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallVector<MachineInstr *, 4> Pairs;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.getOpcode() == TargetOpcode::REG_SEQUENCE)
        Pairs.push_back(&MI);

  bool Changed = false;
  for (MachineInstr *Pair : Pairs) {
    if (!Pair->getParent() || Pair->getNumOperands() != 5 ||
        !Pair->getOperand(0).isReg() || Pair->getOperand(0).getSubReg() ||
        !Pair->getOperand(1).isReg() || Pair->getOperand(1).getSubReg() ||
        !Pair->getOperand(3).isReg() || Pair->getOperand(3).getSubReg())
      continue;

    MachineInstr *First = MRI.getUniqueVRegDef(Pair->getOperand(1).getReg());
    MachineInstr *Second = MRI.getUniqueVRegDef(Pair->getOperand(3).getReg());
    MachineBasicBlock *Source = Pair->getParent();
    if (!First || !Second || First == Second || First->getParent() != Source ||
        Second->getParent() != Source ||
        !isImmutableFixedStackLoad(*First, MFI) ||
        !isImmutableFixedStackLoad(*Second, MFI) ||
        !MRI.hasOneNonDBGUse(Pair->getOperand(1).getReg()) ||
        !MRI.hasOneNonDBGUse(Pair->getOperand(3).getReg()))
      continue;

    SmallVector<MachineInstr *, 4> Construction = {First, Second, Pair};
    Register Value = Pair->getOperand(0).getReg();
    while (MachineInstr *Copy = getSingleFullCopyUser(Value, MRI)) {
      if (Copy->getParent() != Source)
        break;
      Construction.push_back(Copy);
      Value = Copy->getOperand(0).getReg();
    }

    MachineBasicBlock *Destination = getSingleUseBlock(Value, MRI);
    if (!Destination || Destination == Source ||
        MLI.getLoopDepth(Destination) > MLI.getLoopDepth(Source))
      continue;

    MachineBasicBlock::iterator Insert = Destination->getFirstNonPHI();
    if (Destination->computeRegisterLiveness(TRI, C166::PSW, Insert) !=
        MachineBasicBlock::LQR_Dead)
      continue;

    for (MachineInstr *MI : Construction) {
      MI->removeFromParent();
      Destination->insert(Insert, MI);
    }
    Changed = true;
  }
  return Changed;
}

bool C166ArgumentLoadSinking::runOnMachineFunction(MachineFunction &MF) {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  SmallVector<MachineInstr *, 8> Loads;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (isImmutableFixedStackLoad(MI, MFI))
        Loads.push_back(&MI);

  bool Changed = sinkFixedStackPairs(MF, MLI, TRI);
  for (MachineInstr *Load : Loads) {
    Register Value = Load->getOperand(0).getReg();
    if (!Value.isVirtual() || !MRI.hasOneDef(Value))
      continue;

    MachineBasicBlock *Source = Load->getParent();
    MachineBasicBlock *Destination = nullptr;
    bool CanMove = true;
    for (MachineOperand &Use : MRI.use_operands(Value)) {
      MachineInstr *User = Use.getParent();
      if (User->isPHI()) {
        CanMove = false;
        break;
      }
      MachineBasicBlock *UseMBB = User->getParent();
      if (!Destination)
        Destination = UseMBB;
      else if (Destination != UseMBB) {
        CanMove = false;
        break;
      }
    }

    if (!CanMove || !Destination || Destination == Source)
      continue;

    // Do not turn a loop-invariant argument load into work performed on each
    // iteration.  Moving within the same loop depth only shortens the live
    // range and does not change dynamic load frequency.
    if (MLI.getLoopDepth(Destination) > MLI.getLoopDepth(Source))
      continue;

    MachineBasicBlock::iterator Insert = Destination->getFirstNonPHI();
    if (Destination->computeRegisterLiveness(TRI, C166::PSW, Insert) !=
        MachineBasicBlock::LQR_Dead)
      continue;

    Load->removeFromParent();
    Destination->insert(Insert, Load);
    Changed = true;
  }

  return Changed;
}
