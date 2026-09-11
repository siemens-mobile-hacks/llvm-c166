//===-- C166CFI.h - C166 DWARF call-frame helpers ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_C166CFI_H
#define LLVM_LIB_TARGET_C166_C166CFI_H

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/MC/MCDwarf.h"

namespace llvm {

class TargetInstrInfo;

namespace C166CFI {

void build(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
           const DebugLoc &DL, const TargetInstrInfo &TII,
           const MCCFIInstruction &Inst,
           MachineInstr::MIFlag Flag = MachineInstr::NoFlags);

MCCFIInstruction createUserStackValue(unsigned DwarfRegister, int64_t Offset,
                                      unsigned DwarfBaseRegister = 0);

MCCFIInstruction createUserStackLocation(unsigned DwarfRegister, int64_t Offset,
                                         unsigned DwarfDPP1,
                                         unsigned DwarfBaseRegister = 0);

MCCFIInstruction createNearReturnAddress(unsigned DwarfRA, unsigned DwarfCSP);

MCCFIInstruction createInterruptReturnAddress(unsigned DwarfRA);

} // namespace C166CFI
} // namespace llvm

#endif
