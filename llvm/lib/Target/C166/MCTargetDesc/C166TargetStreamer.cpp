//===-- C166TargetStreamer.cpp - C166 target streamer --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166TargetStreamer.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

namespace {

static StringRef getDataClassName(C166DataClass Class) {
  switch (Class) {
  case C166DataClass::Near:
    return "near";
  case C166DataClass::XNear:
    return "xnear";
  case C166DataClass::Far:
    return "far";
  case C166DataClass::Huge:
    return "huge";
  case C166DataClass::SHuge:
    return "shuge";
  }
  llvm_unreachable("unknown C166 data class");
}

static unsigned getDataClassOther(C166DataClass Class) {
  switch (Class) {
  case C166DataClass::Near:
    return ELF::STO_C166_DATA_NEAR;
  case C166DataClass::XNear:
    return ELF::STO_C166_DATA_XNEAR;
  case C166DataClass::Far:
    return ELF::STO_C166_DATA_FAR;
  case C166DataClass::Huge:
    return ELF::STO_C166_DATA_HUGE;
  case C166DataClass::SHuge:
    return ELF::STO_C166_DATA_SHUGE;
  }
  llvm_unreachable("unknown C166 data class");
}

class C166TargetAsmStreamer final : public C166TargetStreamer {
  formatted_raw_ostream &OS;

public:
  C166TargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS)
      : C166TargetStreamer(S), OS(OS) {}

  void emitMemoryModel(CodeModel::Model Model) override {
    StringRef Name = Model == CodeModel::Small    ? "small"
                     : Model == CodeModel::Medium ? "medium"
                                                  : "large";
    OS << "\t.c166_model\t" << Name << '\n';
  }

  void emitFunctionClass(MCSymbol &Symbol, bool IsNear) override {
    OS << "\t.c166_function\t" << (IsNear ? "near, " : "huge, ");
    Symbol.print(OS, Streamer.getContext().getAsmInfo());
    OS << '\n';
  }

  void emitDataClass(MCSymbol &Symbol, C166DataClass Class) override {
    OS << "\t.c166_data\t" << getDataClassName(Class) << ", ";
    Symbol.print(OS, Streamer.getContext().getAsmInfo());
    OS << '\n';
  }
};

class C166TargetELFStreamer final : public C166TargetStreamer {
public:
  explicit C166TargetELFStreamer(MCStreamer &S) : C166TargetStreamer(S) {}

  void emitMemoryModel(CodeModel::Model Model) override {
    auto &ELFStreamer = static_cast<MCELFStreamer &>(Streamer);
    unsigned Flags = Model == CodeModel::Small    ? ELF::EF_C166_SMALL
                     : Model == CodeModel::Medium ? ELF::EF_C166_MEDIUM
                                                  : ELF::EF_C166_LARGE;
    ELFStreamer.getWriter().setELFHeaderEFlags(Flags);
  }

  void emitFunctionClass(MCSymbol &Symbol, bool IsNear) override {
    auto &ELFSymbol = static_cast<MCSymbolELF &>(Symbol);
    unsigned Other = ELFSymbol.getOther() & ~ELF::STO_C166_CODE_MASK;
    ELFSymbol.setOther(
        Other | (IsNear ? ELF::STO_C166_CODE_NEAR : ELF::STO_C166_CODE_HUGE));
  }

  void emitDataClass(MCSymbol &Symbol, C166DataClass Class) override {
    auto &ELFSymbol = static_cast<MCSymbolELF &>(Symbol);
    unsigned Other = ELFSymbol.getOther() & ~ELF::STO_C166_DATA_MASK;
    ELFSymbol.setOther(Other | getDataClassOther(Class));
  }
};

} // namespace

MCTargetStreamer *
llvm::createC166ObjectTargetStreamer(MCStreamer &S,
                                     const MCSubtargetInfo &STI) {
  return new C166TargetELFStreamer(S);
}

MCTargetStreamer *
llvm::createC166AsmTargetStreamer(MCStreamer &S, formatted_raw_ostream &OS,
                                  MCInstPrinter *InstPrinter) {
  return new C166TargetAsmStreamer(S, OS);
}

MCTargetStreamer *llvm::createC166NullTargetStreamer(MCStreamer &S) {
  return new C166TargetStreamer(S);
}
