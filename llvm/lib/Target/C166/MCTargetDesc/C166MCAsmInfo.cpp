//===-- C166MCAsmInfo.cpp - C166 assembly properties ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166MCAsmInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

void C166MCAsmInfo::anchor() {}

C166MCAsmInfo::C166MCAsmInfo(const Triple &TT, const MCTargetOptions &Options)
    : MCAsmInfoELF(Options) {
  CodePointerSize = 4;
  CalleeSaveStackSlotSize = 2;
  CommentString = ";";
  MinInstAlignment = 2;
  UsesELFSectionDirectiveForBSS = true;
  SupportsDebugInformation = true;
  // Emit debug-only CFI even though the C166 C ABI has no exception unwinding.
  // The target MC description supplies the system-stack CFA and virtual
  // CSP:IP return-address rules used by CALLS/RETS.
  UsesCFIWithoutEH = true;
  DwarfRegNumForCFI = true;
  ExceptionsType = ExceptionHandling::None;
}

void C166MCAsmInfo::printSpecifierExpr(raw_ostream &OS,
                                       const MCSpecifierExpr &Expr) const {
  switch (Expr.getSpecifier()) {
  case C166::S_SEG:
    OS << "seg";
    break;
  case C166::S_SOF:
    OS << "sof";
    break;
  case C166::S_COF:
    OS << "cof";
    break;
  case C166::S_PAG:
    OS << "pag";
    break;
  case C166::S_POF:
    OS << "pof";
    break;
  case C166::S_DPP1:
    OS << "dpp1";
    break;
  case C166::S_DPP2:
    OS << "dpp2";
    break;
  case C166::S_PAGED32:
    OS << "paged";
    break;
  default:
    llvm_unreachable("unknown C166 relocation specifier");
  }
  OS << '(';
  printExpr(OS, *Expr.getSubExpr());
  OS << ')';
}
