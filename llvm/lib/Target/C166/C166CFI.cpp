//===-- C166CFI.cpp - C166 DWARF call-frame helpers ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166CFI.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

void C166CFI::build(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                    const DebugLoc &DL, const TargetInstrInfo &TII,
                    const MCCFIInstruction &Inst, MachineInstr::MIFlag Flag) {
  MachineFunction &MF = *MBB.getParent();
  unsigned Index = MF.addFrameInst(Inst);
  MachineInstrBuilder MIB =
      BuildMI(MBB, I, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(Index);
  if (Flag != MachineInstr::NoFlags)
    MIB.setMIFlag(Flag);
}

static void emitBReg(raw_ostream &OS, unsigned DwarfRegister, int64_t Offset) {
  if (DwarfRegister < 32)
    OS << static_cast<uint8_t>(dwarf::DW_OP_breg0 + DwarfRegister);
  else {
    OS << static_cast<uint8_t>(dwarf::DW_OP_bregx);
    encodeULEB128(DwarfRegister, OS);
  }
  encodeSLEB128(Offset, OS);
}

static MCCFIInstruction createExpressionCFI(uint8_t Opcode,
                                            unsigned DwarfRegister,
                                            StringRef Expression,
                                            StringRef Comment) {
  SmallString<32> Encoded;
  raw_svector_ostream OS(Encoded);
  OS << Opcode;
  encodeULEB128(DwarfRegister, OS);
  encodeULEB128(Expression.size(), OS);
  OS << Expression;
  return MCCFIInstruction::createEscape(nullptr, Encoded, SMLoc(), Comment);
}

MCCFIInstruction C166CFI::createUserStackValue(unsigned DwarfRegister,
                                               int64_t Offset,
                                               unsigned DwarfBaseRegister) {
  SmallString<8> Expression;
  raw_svector_ostream OS(Expression);
  // A val-expression is used because the caller's R0 value, rather than
  // memory at that address, is the quantity being recovered. The base is R0
  // for a fixed frame and the frame-pointer register for a dynamic frame.
  emitBReg(OS, DwarfBaseRegister, Offset);
  return createExpressionCFI(dwarf::DW_CFA_val_expression, DwarfRegister,
                             Expression, "caller R0 from C166 user stack");
}

MCCFIInstruction C166CFI::createUserStackLocation(unsigned DwarfRegister,
                                                  int64_t Offset,
                                                  unsigned DwarfDPP1,
                                                  unsigned DwarfBaseRegister) {
  SmallString<24> Expression;
  raw_svector_ostream OS(Expression);

  // Automatic data is addressed through the DPP1 user-stack page. DWARF
  // memory locations are always 32-bit linear byte addresses, so make the
  // page calculation explicit instead of treating the 16-bit frame base as
  // a flat address.
  emitBReg(OS, DwarfDPP1, 0);
  OS << static_cast<uint8_t>(dwarf::DW_OP_lit0 + 14)
     << static_cast<uint8_t>(dwarf::DW_OP_shl);
  emitBReg(OS, DwarfBaseRegister, Offset);
  OS << static_cast<uint8_t>(dwarf::DW_OP_constu);
  encodeULEB128(0x3fff, OS);
  OS << static_cast<uint8_t>(dwarf::DW_OP_and)
     << static_cast<uint8_t>(dwarf::DW_OP_or);

  return createExpressionCFI(dwarf::DW_CFA_expression, DwarfRegister,
                             Expression,
                             "saved register in C166 DPP1 user stack");
}

MCCFIInstruction C166CFI::createNearReturnAddress(unsigned DwarfRA,
                                                  unsigned DwarfCSP) {
  SmallString<24> Expression;
  raw_svector_ostream OS(Expression);

  // CALLA/CALLI/CALLR push only IP.  The caller and callee therefore share
  // CSP, and the ABI's virtual return-address register must reconstruct the
  // 32-bit linear CSP:IP value instead of reading four bytes from the stack.
  emitBReg(OS, DwarfCSP, 0);
  OS << static_cast<uint8_t>(dwarf::DW_OP_lit0 + 16)
     << static_cast<uint8_t>(dwarf::DW_OP_shl)
     << static_cast<uint8_t>(dwarf::DW_OP_call_frame_cfa)
     << static_cast<uint8_t>(dwarf::DW_OP_lit0 + 2)
     << static_cast<uint8_t>(dwarf::DW_OP_minus)
     << static_cast<uint8_t>(dwarf::DW_OP_deref_size) << static_cast<uint8_t>(2)
     << static_cast<uint8_t>(dwarf::DW_OP_or);

  return createExpressionCFI(dwarf::DW_CFA_val_expression, DwarfRA, Expression,
                             "C166 near CSP plus saved IP return address");
}

MCCFIInstruction C166CFI::createInterruptReturnAddress(unsigned DwarfRA) {
  SmallString<32> Expression;
  raw_svector_ostream OS(Expression);

  // On interrupt entry the CPU has pushed IP, CSP and PSW.  With CFA defined
  // as the caller's system-stack value, IP is at CFA-6 and CSP at CFA-4.
  OS << static_cast<uint8_t>(dwarf::DW_OP_call_frame_cfa)
     << static_cast<uint8_t>(dwarf::DW_OP_lit0 + 4)
     << static_cast<uint8_t>(dwarf::DW_OP_minus)
     << static_cast<uint8_t>(dwarf::DW_OP_deref_size) << static_cast<uint8_t>(2)
     << static_cast<uint8_t>(dwarf::DW_OP_lit0 + 16)
     << static_cast<uint8_t>(dwarf::DW_OP_shl)
     << static_cast<uint8_t>(dwarf::DW_OP_call_frame_cfa)
     << static_cast<uint8_t>(dwarf::DW_OP_lit0 + 6)
     << static_cast<uint8_t>(dwarf::DW_OP_minus)
     << static_cast<uint8_t>(dwarf::DW_OP_deref_size) << static_cast<uint8_t>(2)
     << static_cast<uint8_t>(dwarf::DW_OP_or);

  return createExpressionCFI(dwarf::DW_CFA_val_expression, DwarfRA, Expression,
                             "C166 interrupt saved CSP:IP return address");
}
