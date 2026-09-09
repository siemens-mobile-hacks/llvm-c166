//===-- C166BitExpr.h - C166 bit operands --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_MCTARGETDESC_C166BITEXPR_H
#define LLVM_LIB_TARGET_C166_MCTARGETDESC_C166BITEXPR_H

#include "llvm/MC/MCExpr.h"

namespace llvm {

// Keep the components separate until encoding so each retains its range check.
class C166BitExpr final : public MCTargetExpr {
  const MCExpr *Word;
  const MCExpr *Bit;

  C166BitExpr(const MCExpr *Word, const MCExpr *Bit) : Word(Word), Bit(Bit) {}

public:
  static const C166BitExpr *create(const MCExpr *Word, const MCExpr *Bit,
                                   MCContext &Ctx);
  const MCExpr *getWord() const { return Word; }
  const MCExpr *getBit() const { return Bit; }
  void printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const override;
  bool evaluateAsRelocatableImpl(MCValue &,
                                 const MCAssembler *) const override {
    return false;
  }
  void visitUsedExpr(MCStreamer &Streamer) const override;
  MCFragment *findAssociatedFragment() const override { return nullptr; }
  static bool classof(const MCExpr *E) {
    return E->getKind() == MCExpr::Target;
  }
};

} // namespace llvm

#endif
