//===-- C166PHIEdgeSplitting.cpp - Split profitable PHI edges ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "c166-phi-edge-splitting"

namespace {

static bool hasNonPHIUseInBlock(Register Reg, MachineBasicBlock &MBB,
                                MachineRegisterInfo &MRI) {
  if (!Reg.isVirtual())
    return false;
  return llvm::any_of(MRI.use_nodbg_instructions(Reg),
                      [&](const MachineInstr &Use) {
                        return Use.getParent() == &MBB && !Use.isPHI();
                      });
}

static unsigned countIncomingPHIs(Register Reg, MachineBasicBlock &Pred,
                                  MachineBasicBlock &Succ) {
  unsigned Count = 0;
  for (MachineInstr &Phi : Succ.phis())
    for (unsigned I = 1; I + 1 < Phi.getNumOperands(); I += 2)
      Count += Phi.getOperand(I).getReg() == Reg &&
               Phi.getOperand(I + 1).getMBB() == &Pred;
  return Count;
}

static bool shouldSplitEdge(MachineBasicBlock &Pred, MachineBasicBlock &Succ,
                            MachineRegisterInfo &MRI) {
  for (MachineInstr &Phi : Succ.phis())
    for (unsigned I = 1; I + 1 < Phi.getNumOperands(); I += 2)
      if (Phi.getOperand(I + 1).getMBB() == &Pred &&
          countIncomingPHIs(Phi.getOperand(I).getReg(), Pred, Succ) == 1 &&
          hasNonPHIUseInBlock(Phi.getOperand(I).getReg(), Succ, MRI))
        return true;
  return false;
}

class C166PHIEdgeSplitting : public MachineFunctionPass {
public:
  static char ID;

  C166PHIEdgeSplitting() : MachineFunctionPass(ID) {
    initializeC166PHIEdgeSplittingPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "C166 PHI edge splitting"; }

  bool runOnMachineFunction(MachineFunction &MF) override {
    if (!MF.getFunction().hasOptSize())
      return false;

    MachineRegisterInfo &MRI = MF.getRegInfo();
    MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
    SmallVector<std::pair<MachineBasicBlock *, MachineBasicBlock *>, 8> Edges;

    for (MachineBasicBlock &Succ : MF) {
      // Keep the transformation to a two-way merge where moving the PHI copy
      // past the branch shortens an otherwise overlapping live range. Splitting
      // wider fan-in blocks can add an out-of-line jump without reducing
      // register pressure.
      if (Succ.empty() || !Succ.front().isPHI() || Succ.pred_size() != 2)
        continue;

      const MachineLoop *Loop = MLI.getLoopFor(&Succ);
      bool IsLoopHeader = Loop && Loop->getHeader() == &Succ;
      for (MachineBasicBlock *Pred : Succ.predecessors()) {
        if (Pred->succ_size() < 2 || (IsLoopHeader && Loop->contains(Pred)) ||
            !shouldSplitEdge(*Pred, Succ, MRI) ||
            !Pred->canSplitCriticalEdge(&Succ, &MLI))
          continue;
        Edges.emplace_back(Pred, &Succ);
      }
    }

    bool Changed = false;
    for (auto [Pred, Succ] : Edges)
      Changed |= Pred->SplitCriticalEdge(Succ, *this) != nullptr;
    return Changed;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.addPreserved<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char C166PHIEdgeSplitting::ID = 0;

INITIALIZE_PASS_BEGIN(C166PHIEdgeSplitting, DEBUG_TYPE,
                      "C166 PHI edge splitting", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(C166PHIEdgeSplitting, DEBUG_TYPE, "C166 PHI edge splitting",
                    false, false)

FunctionPass *llvm::createC166PHIEdgeSplittingPass() {
  return new C166PHIEdgeSplitting();
}
