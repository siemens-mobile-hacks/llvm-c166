//===-- C166MCTargetDesc.cpp - C166 target descriptions ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166MCTargetDesc.h"
#include "C166InstPrinter.h"
#include "C166MCAsmInfo.h"
#include "C166TargetStreamer.h"
#include "TargetInfo/C166TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

namespace {

class C166MCObjectFileInfo final : public MCObjectFileInfo {
public:
  unsigned getTextSectionAlignment() const override { return 2; }
};

MCObjectFileInfo *createC166MCObjectFileInfo(MCContext &Ctx, bool PIC,
                                             bool LargeCodeModel = false) {
  auto *MOFI = new C166MCObjectFileInfo();
  MOFI->initMCObjectFileInfo(Ctx, PIC, LargeCodeModel);
  return MOFI;
}

} // namespace

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "C166GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "C166GenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "C166GenRegisterInfo.inc"

static MCInstrInfo *createC166MCInstrInfo() {
  auto *MII = new MCInstrInfo();
  InitC166MCInstrInfo(MII);
  return MII;
}

static MCRegisterInfo *createC166MCRegisterInfo(const Triple &TT) {
  auto *MRI = new MCRegisterInfo();
  InitC166MCRegisterInfo(MRI, C166::RA);
  return MRI;
}

static MCAsmInfo *createC166MCAsmInfo(const MCRegisterInfo &MRI,
                                      const Triple &TT,
                                      const MCTargetOptions &Options) {
  auto *MAI = new C166MCAsmInfo(TT, Options);

  // A far CALLS pushes CSP followed by IP onto the descending
  // hardware system stack.  On entry SP therefore points at the two-byte IP,
  // and the caller's system-stack value is SP+4.  Since the words are stored
  // little-endian, reading four bytes at CFA-4 produces the ABI's 32-bit
  // linear CSP:IP return address in virtual register 301.
  const unsigned DwarfSP = MRI.getDwarfRegNum(C166::SP, true);
  const unsigned DwarfCSP = MRI.getDwarfRegNum(C166::CSP, true);
  const unsigned DwarfRA = MRI.getDwarfRegNum(C166::RA, true);
  MAI->addInitialFrameState(MCCFIInstruction::cfiDefCfa(nullptr, DwarfSP, 4));
  MAI->addInitialFrameState(
      MCCFIInstruction::createOffset(nullptr, DwarfRA, -4));
  MAI->addInitialFrameState(
      MCCFIInstruction::createOffset(nullptr, DwarfCSP, -2));
  MAI->addInitialFrameState(
      MCCFIInstruction::createValOffset(nullptr, DwarfSP, 0));

  // R0 and the C166 register variables are callee-preserved until an FDE
  // replaces their rules with user-stack expressions in a function prologue.
  for (unsigned Reg :
       {C166::R0, C166::R6, C166::R7, C166::R8, C166::R9, C166::DPP1})
    MAI->addInitialFrameState(MCCFIInstruction::createSameValue(
        nullptr, MRI.getDwarfRegNum(Reg, true)));
  return MAI;
}

static MCSubtargetInfo *createC166MCSubtargetInfo(const Triple &TT,
                                                  StringRef CPU, StringRef FS) {
  if (CPU.empty())
    CPU = "c166";
  return createC166MCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

static MCInstPrinter *createC166MCInstPrinter(const Triple &TT,
                                              unsigned SyntaxVariant,
                                              const MCAsmInfo &MAI,
                                              const MCInstrInfo &MII,
                                              const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new C166InstPrinter(MAI, MII, MRI);
  return nullptr;
}

static MCStreamer *
createC166ELFStreamer(const Triple &TT, MCContext &Context,
                      std::unique_ptr<MCAsmBackend> &&MAB,
                      std::unique_ptr<MCObjectWriter> &&OW,
                      std::unique_ptr<MCCodeEmitter> &&Emitter) {
  auto *Streamer = static_cast<MCELFStreamer *>(createELFStreamer(
      Context, std::move(MAB), std::move(OW), std::move(Emitter)));
  Streamer->getWriter().setELFHeaderEFlags(ELF::EF_C166_LARGE);
  return Streamer;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeC166TargetMC() {
  Target &T = getTheC166Target();
  TargetRegistry::RegisterMCAsmInfo(T, createC166MCAsmInfo);
  TargetRegistry::RegisterMCObjectFileInfo(T, createC166MCObjectFileInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createC166MCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createC166MCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createC166MCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createC166MCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createC166MCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createC166MCAsmBackend);
  TargetRegistry::RegisterELFStreamer(T, createC166ELFStreamer);
  TargetRegistry::RegisterObjectTargetStreamer(T,
                                               createC166ObjectTargetStreamer);
  TargetRegistry::RegisterAsmTargetStreamer(T, createC166AsmTargetStreamer);
  TargetRegistry::RegisterNullTargetStreamer(T, createC166NullTargetStreamer);
}
