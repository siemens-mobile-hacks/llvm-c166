//===-- C166TargetStreamer.h - C166 target streamer -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_MCTARGETDESC_C166TARGETSTREAMER_H
#define LLVM_LIB_TARGET_C166_MCTARGETDESC_C166TARGETSTREAMER_H

#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/CodeGen.h"

namespace llvm {

enum class C166DataClass : unsigned {
  Near,
  XNear,
  Far,
  Huge,
  SHuge,
};

class MCInstPrinter;
class MCSubtargetInfo;
class MCSymbol;
class formatted_raw_ostream;

class C166TargetStreamer : public MCTargetStreamer {
public:
  explicit C166TargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

  virtual void emitMemoryModel(CodeModel::Model Model) {}
  virtual void emitFunctionClass(MCSymbol &Symbol, bool IsNear) {}
  virtual void emitDataClass(MCSymbol &Symbol, C166DataClass Class) {}
};

MCTargetStreamer *createC166ObjectTargetStreamer(MCStreamer &S,
                                                 const MCSubtargetInfo &STI);
MCTargetStreamer *createC166AsmTargetStreamer(MCStreamer &S,
                                              formatted_raw_ostream &OS,
                                              MCInstPrinter *InstPrinter);
MCTargetStreamer *createC166NullTargetStreamer(MCStreamer &S);

} // namespace llvm

#endif
