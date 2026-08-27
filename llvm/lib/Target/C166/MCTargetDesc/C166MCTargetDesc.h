//===-- C166MCTargetDesc.h - C166 target descriptions ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_MCTARGETDESC_C166MCTARGETDESC_H
#define LLVM_LIB_TARGET_C166_MCTARGETDESC_C166MCTARGETDESC_H

#include <cstdint>
#include <memory>

namespace llvm {

class Target;
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;

MCCodeEmitter *createC166MCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx);

MCAsmBackend *createC166MCAsmBackend(const Target &T,
                                     const MCSubtargetInfo &STI,
                                     const MCRegisterInfo &MRI,
                                     const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createC166ELFObjectWriter(uint8_t OSABI);

} // namespace llvm

#define GET_REGINFO_ENUM
#include "C166GenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "C166GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "C166GenSubtargetInfo.inc"

#endif
