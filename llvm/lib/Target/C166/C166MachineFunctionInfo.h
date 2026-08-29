//===-- C166MachineFunctionInfo.h - C166 machine function info -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_C166MACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_C166_C166MACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class C166MachineFunctionInfo : public MachineFunctionInfo {
  virtual void anchor();

  Register SRetAddressReg = 0;
  int VarArgsFrameIndex = 0;
  bool HasVarArgsFrameIndex = false;
  unsigned CalleeSavedFrameSize = 0;

public:
  C166MachineFunctionInfo() = default;
  C166MachineFunctionInfo(const Function &, const TargetSubtargetInfo *) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override;

  Register getSRetAddressReg() const { return SRetAddressReg; }
  void setSRetAddressReg(Register Reg) { SRetAddressReg = Reg; }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  bool hasVarArgsFrameIndex() const { return HasVarArgsFrameIndex; }
  void setVarArgsFrameIndex(int FI) {
    VarArgsFrameIndex = FI;
    HasVarArgsFrameIndex = true;
  }

  unsigned getCalleeSavedFrameSize() const { return CalleeSavedFrameSize; }
  void setCalleeSavedFrameSize(unsigned Size) { CalleeSavedFrameSize = Size; }
};

} // namespace llvm

#endif
