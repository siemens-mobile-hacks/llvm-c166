//===-- C166RegisterInfo.h - C166 register information ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_C166REGISTERINFO_H
#define LLVM_LIB_TARGET_C166_C166REGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "C166GenRegisterInfo.inc"

namespace llvm {

class C166RegisterInfo : public C166GenRegisterInfo {
public:
  C166RegisterInfo();

  const uint16_t *
  getCalleeSavedRegs(const MachineFunction *MF = nullptr) const override;
  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const override;
  BitVector getReservedRegs(const MachineFunction &MF) const override;
  const TargetRegisterClass *
  getPointerRegClass(unsigned Kind = 0) const override;
  bool eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;
  bool eliminateFrameIndicesBackwards() const override { return false; }
  Register getFrameRegister(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif
