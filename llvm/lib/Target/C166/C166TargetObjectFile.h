//===-- C166TargetObjectFile.h - C166 ELF object lowering -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_C166TARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_C166_C166TARGETOBJECTFILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {

class C166TargetObjectFile final : public TargetLoweringObjectFileELF {
  using Base = TargetLoweringObjectFileELF;

  MCSection *NearTextSection = nullptr;
  MCSection *NearDataSection = nullptr;
  MCSection *NearBSSSection = nullptr;
  MCSection *NearReadOnlySection = nullptr;
  MCSection *XNearDataSection = nullptr;
  MCSection *XNearBSSSection = nullptr;
  MCSection *XNearReadOnlySection = nullptr;
  MCSection *SmallDataSection = nullptr;
  MCSection *SmallBSSSection = nullptr;
  MCSection *SmallReadOnlySection = nullptr;
  MCSection *SmallFarDataSection = nullptr;
  MCSection *SmallFarBSSSection = nullptr;
  MCSection *SmallFarReadOnlySection = nullptr;
  MCSection *SmallHugeDataSection = nullptr;
  MCSection *SmallHugeBSSSection = nullptr;
  MCSection *SmallHugeReadOnlySection = nullptr;
  MCSection *SmallSHugeDataSection = nullptr;
  MCSection *SmallSHugeBSSSection = nullptr;
  MCSection *SmallSHugeReadOnlySection = nullptr;

public:
  void Initialize(MCContext &Ctx, const TargetMachine &TM) override;

  MCSection *SelectSectionForGlobal(const GlobalObject *GO, SectionKind Kind,
                                    const TargetMachine &TM) const override;

  MCSection *getSectionForJumpTable(const Function &F,
                                    const TargetMachine &TM) const override;
};

} // namespace llvm

#endif
