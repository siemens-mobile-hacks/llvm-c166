//===-- C166TargetMachine.h - Define the C166 target machine ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_C166TARGETMACHINE_H
#define LLVM_LIB_TARGET_C166_C166TARGETMACHINE_H

#include "C166Subtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/TargetParser/C166TargetParser.h"

namespace llvm {

class C166TargetMachine : public CodeGenTargetMachineImpl {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  C166Subtarget Subtarget;

public:
  C166TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                    StringRef FS, const TargetOptions &Options,
                    std::optional<Reloc::Model> RM,
                    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                    bool JIT);
  ~C166TargetMachine() override;

  const C166Subtarget *getSubtargetImpl(const Function &) const override {
    return &Subtarget;
  }
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;
  void registerPassBuilderCallbacks(PassBuilder &PB) override;
  bool shouldDefaultToNewPM() const override { return false; }

  bool isNoopAddrSpaceCast(unsigned SrcAS, unsigned DestAS) const override {
    // Huge code pointers and far data pointers have the same two-word storage
    // representation. This also lets static computed-goto tables emit
    // ordinary blockaddress relocations.
    return (SrcAS == C166::HugeCodeAddressSpace &&
            DestAS == C166::FarDataAddressSpace) ||
           (SrcAS == C166::FarDataAddressSpace &&
            DestAS == C166::HugeCodeAddressSpace);
  }
};

} // namespace llvm

#endif
