//===-- C166InstPrinter.h - Print C166 MC instructions ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_MCTARGETDESC_C166INSTPRINTER_H
#define LLVM_LIB_TARGET_C166_MCTARGETDESC_C166INSTPRINTER_H

#include "llvm/MC/MCInstPrinter.h"

namespace llvm {

class C166InstPrinter : public MCInstPrinter {
public:
  C166InstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                  const MCRegisterInfo &MRI)
      : MCInstPrinter(MAI, MII, MRI) {}

  void printRegName(raw_ostream &OS, MCRegister Reg) override;
  void printInst(const MCInst *MI, uint64_t Address, StringRef Annot,
                 const MCSubtargetInfo &STI, raw_ostream &OS) override;

  std::pair<const char *, uint64_t>
  getMnemonic(const MCInst &MI) const override;
  void printInstruction(const MCInst *MI, uint64_t Address, raw_ostream &OS);
  bool printAliasInstr(const MCInst *MI, uint64_t Address, raw_ostream &OS);
  void printCustomAliasOperand(const MCInst *MI, uint64_t Address,
                               unsigned OpIdx, unsigned PrintMethodIdx,
                               raw_ostream &OS);
  static const char *getRegisterName(MCRegister Reg);

private:
  void printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &OS);
  void printImmediate(const MCInst *MI, unsigned OpNo, raw_ostream &OS);
  void printImmediate16(const MCInst *MI, unsigned OpNo, raw_ostream &OS);
  void printBitAddress(const MCInst *MI, unsigned OpNo, raw_ostream &OS);
  void printBitOffset(const MCInst *MI, unsigned OpNo, raw_ostream &OS);
  void printAddress(const MCInst *MI, unsigned OpNo, raw_ostream &OS);
  void printDPP1Address(const MCInst *MI, unsigned OpNo, raw_ostream &OS);
  void printDPP2Address(const MCInst *MI, unsigned OpNo, raw_ostream &OS);
  void printDPPAddress(const MCInst *MI, unsigned OpNo, StringRef Qualifier,
                       raw_ostream &OS);
  void printBranchTarget(const MCInst *MI, uint64_t Address, unsigned OpNo,
                         raw_ostream &OS);
};

} // namespace llvm

#endif
