//===-- C166InstPrinter.cpp - Print C166 MC instructions -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166InstPrinter.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"

using namespace llvm;

#define PRINT_ALIAS_INSTR
#include "C166GenAsmWriter.inc"

void C166InstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << getRegisterName(Reg);
}

void C166InstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                StringRef Annot, const MCSubtargetInfo &STI,
                                raw_ostream &OS) {
  if (!printAliasInstr(MI, Address, OS))
    printInstruction(MI, Address, OS);
  printAnnotation(OS, Annot);
}

void C166InstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    printRegName(OS, Op.getReg());
    return;
  }
  if (Op.isImm()) {
    OS << Op.getImm();
    return;
  }
  MAI.printExpr(OS, *Op.getExpr());
}

void C166InstPrinter::printImmediate(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  OS << '#';
  if (Op.isImm())
    OS << Op.getImm();
  else
    MAI.printExpr(OS, *Op.getExpr());
}

void C166InstPrinter::printShortRegister(const MCInst *MI, unsigned OpNo,
                                         raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg())
    printRegName(OS, Op.getReg());
  else
    OS << "sfr(" << Op.getImm() << ')';
}

void C166InstPrinter::printImmediate16(const MCInst *MI, unsigned OpNo,
                                       raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  OS << '#';
  if (Op.isImm())
    OS << static_cast<uint16_t>(Op.getImm());
  else
    MAI.printExpr(OS, *Op.getExpr());
}

static void printBitOffsetName(unsigned WordAddress, raw_ostream &OS) {
  switch (WordAddress) {
  case 0x87:
    OS << "mdc";
    break;
  case 0x88:
    OS << "psw";
    break;
  default:
    if (WordAddress >= 0xf0)
      OS << 'r' << (WordAddress - 0xf0);
    else
      OS << WordAddress;
    break;
  }
}

void C166InstPrinter::printBitAddress(const MCInst *MI, unsigned OpNo,
                                      raw_ostream &OS) {
  if (MI->getOperand(OpNo).isExpr()) {
    printAddress(MI, OpNo, OS);
    return;
  }
  uint64_t Packed = MI->getOperand(OpNo).getImm();
  unsigned WordAddress = Packed >> 4;
  printBitOffsetName(WordAddress, OS);
  unsigned Bit = Packed & 0xf;
  // Separate a numeric word address from the bit to avoid a real-number token.
  bool Named =
      WordAddress >= 0xf0 || WordAddress == 0x87 || WordAddress == 0x88;
  OS << (Named ? "." : " . ") << Bit;
}

void C166InstPrinter::printBitOffset(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &OS) {
  if (MI->getOperand(OpNo).isExpr()) {
    printAddress(MI, OpNo, OS);
    return;
  }
  printBitOffsetName(MI->getOperand(OpNo).getImm(), OS);
}

void C166InstPrinter::printAddress(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm())
    OS << Op.getImm();
  else
    MAI.printExpr(OS, *Op.getExpr());
}

void C166InstPrinter::printDPPAddress(const MCInst *MI, unsigned OpNo,
                                      StringRef Qualifier, raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  // Assembled symbolic operands already carry their dpp1/dpp2 MC specifier.
  // Disassembly produces an immediate, which must regain the qualifier so
  // that llvm-objdump output can be assembled without losing selector bits.
  if (Op.isExpr()) {
    MAI.printExpr(OS, *Op.getExpr());
    return;
  }
  OS << Qualifier << '(';
  printAddress(MI, OpNo, OS);
  OS << ')';
}

void C166InstPrinter::printDPP1Address(const MCInst *MI, unsigned OpNo,
                                       raw_ostream &OS) {
  printDPPAddress(MI, OpNo, "dpp1", OS);
}

void C166InstPrinter::printDPP2Address(const MCInst *MI, unsigned OpNo,
                                       raw_ostream &OS) {
  printDPPAddress(MI, OpNo, "dpp2", OS);
}

void C166InstPrinter::printBranchTarget(const MCInst *MI, uint64_t Address,
                                        unsigned OpNo, raw_ostream &OS) {
  printAddress(MI, OpNo, OS);
}
