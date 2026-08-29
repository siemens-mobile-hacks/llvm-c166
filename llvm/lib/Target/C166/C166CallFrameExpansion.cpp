//===-- C166CallFrameExpansion.cpp - Expand user call frames ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166InstrInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "c166-call-frame-expansion"

namespace {

class C166CallFrameExpansion : public MachineFunctionPass {
public:
  static char ID;

  C166CallFrameExpansion() : MachineFunctionPass(ID) {
    initializeC166CallFrameExpansionPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "C166 call frame expansion"; }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const auto &TII =
        *static_cast<const C166InstrInfo *>(MF.getSubtarget().getInstrInfo());
    bool Changed = false;
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : llvm::make_early_inc_range(MBB))
        Changed |= TII.expandCallFramePseudo(MI);
    return Changed;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char C166CallFrameExpansion::ID = 0;

INITIALIZE_PASS(C166CallFrameExpansion, DEBUG_TYPE, "C166 call frame expansion",
                false, false)

FunctionPass *llvm::createC166CallFrameExpansionPass() {
  return new C166CallFrameExpansion();
}
