//===-- C166MCAsmInfo.h - C166 assembly properties ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_MCTARGETDESC_C166MCASMINFO_H
#define LLVM_LIB_TARGET_C166_MCTARGETDESC_C166MCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"
#include "llvm/MC/MCExpr.h"

namespace llvm {

class Triple;

class C166MCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit C166MCAsmInfo(const Triple &TT, const MCTargetOptions &Options);
  void printSpecifierExpr(raw_ostream &OS,
                          const MCSpecifierExpr &Expr) const override;
};

namespace C166 {
using Specifier = uint16_t;
enum : Specifier {
  S_None,
  S_SEG = MCSymbolRefExpr::FirstTargetSpecifier,
  S_SOF,
  S_COF,
  S_PAG,
  S_POF,
  S_DPP1,
  S_DPP2,
  S_PAGED32,
};
} // namespace C166

} // namespace llvm

#endif
