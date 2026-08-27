//===-- C166UnsupportedFeatures.cpp - Reject unsupported IR features -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {

class C166UnsupportedFeatures : public ModulePass {
public:
  static char ID;

  C166UnsupportedFeatures() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    bool HadMustTailCall = false;
    bool HadRuntimeUnwindTables = false;
    for (Function &F : M) {
      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          auto *Call = dyn_cast<CallInst>(&I);
          if (!Call || !Call->isMustTailCall())
            continue;
          HadMustTailCall = true;
          Call->setTailCallKind(CallInst::TCK_None);
        }
      }

      if (F.hasUWTable()) {
        HadRuntimeUnwindTables = true;

        // Keep the remainder of code generation structurally safe after the
        // diagnostic. An ELF .eh_frame CIE uses the version-1 one-byte return
        // register field and cannot encode the ABI's virtual register 301.
        // Debug unwinding is emitted separately in DWARF .debug_frame.
        F.removeFnAttr(Attribute::UWTable);
      }
    }

    if (HadMustTailCall)
      M.getContext().emitError("C166 does not support musttail calls");
    if (HadRuntimeUnwindTables)
      M.getContext().emitError(
          "C166 does not support runtime .eh_frame unwind tables; use -g "
          "for DWARF .debug_frame information");
    return HadMustTailCall || HadRuntimeUnwindTables;
  }

  StringRef getPassName() const override {
    return "C166 unsupported feature diagnostics";
  }
};

} // namespace

char C166UnsupportedFeatures::ID = 0;

INITIALIZE_PASS(C166UnsupportedFeatures, "c166-unsupported-features",
                "C166 unsupported feature diagnostics", false, false)

Pass *llvm::createC166UnsupportedFeaturesPass() {
  return new C166UnsupportedFeatures();
}
