//===-- C166BitExpr.cpp - C166 bit operands -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166BitExpr.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

const C166BitExpr *C166BitExpr::create(const MCExpr *Word, const MCExpr *Bit,
                                       MCContext &Ctx) {
  return new (Ctx) C166BitExpr(Word, Bit);
}

void C166BitExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  MAI->printExpr(OS, *Word);
  OS << " . (";
  MAI->printExpr(OS, *Bit);
  OS << ')';
}

void C166BitExpr::visitUsedExpr(MCStreamer &Streamer) const {
  Streamer.visitUsedExpr(*Word);
  Streamer.visitUsedExpr(*Bit);
}
