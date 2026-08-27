//===-- C166Subtarget.h - Define the C166 subtarget ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_C166SUBTARGET_H
#define LLVM_LIB_TARGET_C166_C166SUBTARGET_H

#include "C166FrameLowering.h"
#include "C166ISelLowering.h"
#include "C166InstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

#define GET_SUBTARGETINFO_HEADER
#include "C166GenSubtargetInfo.inc"

namespace llvm {

class C166Subtarget : public C166GenSubtargetInfo {
  void anchor();

  C166InstrInfo InstrInfo;
  C166TargetLowering TLInfo;
  std::unique_ptr<const SelectionDAGTargetInfo> TSInfo;
  C166FrameLowering FrameLowering;

public:
  C166Subtarget(const Triple &TT, StringRef CPU, StringRef FS,
                const TargetMachine &TM);
  ~C166Subtarget() override;

  C166Subtarget &initializeSubtargetDependencies(StringRef CPU, StringRef FS);
  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  const C166InstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const C166RegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const C166TargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const TargetFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return TSInfo.get();
  }

  void initLibcallLoweringInfo(LibcallLoweringInfo &Info) const override;
};

} // namespace llvm

#endif
