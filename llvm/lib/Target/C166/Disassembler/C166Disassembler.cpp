//===-- C166Disassembler.cpp - C166 disassembler -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/C166MCTargetDesc.h"
#include "TargetInfo/C166TargetInfo.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::MCD;

using DecodeStatus = MCDisassembler::DecodeStatus;

#define DEBUG_TYPE "c166-disassembler"

namespace {

class C166Disassembler : public MCDisassembler {
public:
  C166Disassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &MI, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};

} // namespace

static DecodeStatus DecodeGR16RegisterClass(MCInst &MI, uint64_t RegNo,
                                            uint64_t Address,
                                            const MCDisassembler *Decoder) {
  static const MCRegister Registers[] = {
      C166::R0,  C166::R1,  C166::R2,  C166::R3, C166::R4,  C166::R5,
      C166::R6,  C166::R7,  C166::R8,  C166::R9, C166::R10, C166::R11,
      C166::R12, C166::R13, C166::R14, C166::R15};
  if (RegNo >= std::size(Registers))
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createReg(Registers[RegNo]));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeGR8RegisterClass(MCInst &MI, uint64_t RegNo,
                                           uint64_t Address,
                                           const MCDisassembler *Decoder) {
  static const MCRegister Registers[] = {
      C166::RL0, C166::RH0, C166::RL1, C166::RH1, C166::RL2, C166::RH2,
      C166::RL3, C166::RH3, C166::RL4, C166::RH4, C166::RL5, C166::RH5,
      C166::RL6, C166::RH6, C166::RL7, C166::RH7};
  if (RegNo >= std::size(Registers))
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createReg(Registers[RegNo]));
  return MCDisassembler::Success;
}

static DecodeStatus decodeEXTP1r(MCInst &MI, uint64_t Insn, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  if (DecodeGR16RegisterClass(MI, (Insn >> 8) & 0xf, Address, Decoder) ==
      MCDisassembler::Fail)
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createImm(((Insn >> 12) & 0x3) + 1));
  return MCDisassembler::Success;
}

static DecodeStatus decodeEXTS1r(MCInst &MI, uint64_t Insn, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  return decodeEXTP1r(MI, Insn, Address, Decoder);
}

static DecodeStatus decodeEXTP1p(MCInst &MI, uint64_t Insn, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  MI.addOperand(MCOperand::createImm((Insn >> 16) & 0x3ff));
  MI.addOperand(MCOperand::createImm(((Insn >> 12) & 0x3) + 1));
  return MCDisassembler::Success;
}

static DecodeStatus decodeATOMIC(MCInst &MI, uint64_t Insn, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  if ((Insn & 0xcf00) != 0)
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createImm(((Insn >> 12) & 0x3) + 1));
  return MCDisassembler::Success;
}

static MCRegister decodeSFRShortAddress(unsigned ShortAddress);

static DecodeStatus decodePUSH(MCInst &MI, uint64_t Insn, uint64_t Address,
                               const MCDisassembler *Decoder) {
  uint64_t DirectAddress = (Insn >> 8) & 0xff;
  if ((DirectAddress & 0xf0) == 0xf0)
    return DecodeGR16RegisterClass(MI, DirectAddress & 0xf, Address, Decoder);
  MCRegister Reg = decodeSFRShortAddress(DirectAddress);
  if (!Reg)
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus decodeMOVBZgd(MCInst &MI, uint64_t Insn, uint64_t Address,
                                  const MCDisassembler *Decoder) {
  uint64_t DirectRegister = (Insn >> 8) & 0xff;
  if ((DirectRegister & 0xf0) != 0xf0 ||
      DecodeGR16RegisterClass(MI, DirectRegister & 0xf, Address, Decoder) ==
          MCDisassembler::Fail)
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createImm((Insn >> 16) & 0x3fff));
  return MCDisassembler::Success;
}

static DecodeStatus decodeMOVBSgd(MCInst &MI, uint64_t Insn, uint64_t Address,
                                  const MCDisassembler *Decoder) {
  return decodeMOVBZgd(MI, Insn, Address, Decoder);
}

static DecodeStatus decodeMOVBdg(MCInst &MI, uint64_t Insn, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  uint64_t DirectRegister = (Insn >> 8) & 0xff;
  if ((DirectRegister & 0xf0) != 0xf0)
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createImm((Insn >> 16) & 0x3fff));
  return DecodeGR8RegisterClass(MI, DirectRegister & 0xf, Address, Decoder);
}

static MCRegister decodeSFRShortAddress(unsigned ShortAddress) {
  switch (ShortAddress) {
  case 0x00:
    return C166::DPP0;
  case 0x01:
    return C166::DPP1;
  case 0x02:
    return C166::DPP2;
  case 0x03:
    return C166::DPP3;
  case 0x04:
    return C166::CSP;
  case 0x06:
    return C166::MDH;
  case 0x07:
    return C166::MDL;
  case 0x08:
    return C166::CP;
  case 0x09:
    return C166::SP;
  case 0x0a:
    return C166::STKOV;
  case 0x0b:
    return C166::STKUN;
  case 0x87:
    return C166::MDC;
  case 0x88:
    return C166::PSW;
  default:
    return MCRegister();
  }
}

static DecodeStatus decodeSCXTri16(MCInst &MI, uint64_t Insn, uint64_t Address,
                                   const MCDisassembler *Decoder) {
  unsigned DirectAddress = (Insn >> 8) & 0xff;
  if ((DirectAddress & 0xf0) == 0xf0) {
    if (DecodeGR16RegisterClass(MI, DirectAddress & 0xf, Address, Decoder) ==
        MCDisassembler::Fail)
      return MCDisassembler::Fail;
  } else {
    MCRegister Reg = decodeSFRShortAddress(DirectAddress);
    if (!Reg)
      return MCDisassembler::Fail;
    MI.addOperand(MCOperand::createReg(Reg));
  }
  // SCXT has a tied def/use register operand.  MCInst keeps both explicit
  // operands even though the encoding carries the register only once.
  MI.addOperand(MI.getOperand(0));
  MI.addOperand(MCOperand::createImm((Insn >> 16) & 0xffff));
  return MCDisassembler::Success;
}

static MCRegister decodeSFRDirectAddress(uint16_t DirectAddress) {
  if (DirectAddress < 0xfe00 || (DirectAddress & 1))
    return MCRegister();
  return decodeSFRShortAddress((DirectAddress - 0xfe00) / 2);
}

static DecodeStatus decodeMOVsfri16(MCInst &MI, uint64_t Insn, uint64_t Address,
                                    const MCDisassembler *Decoder) {
  MCRegister Reg = decodeSFRShortAddress((Insn >> 8) & 0xff);
  if (!Reg)
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createReg(Reg));
  MI.addOperand(MCOperand::createImm((Insn >> 16) & 0xffff));
  return MCDisassembler::Success;
}

static DecodeStatus decodeMOVgsfr(MCInst &MI, uint64_t Insn, uint64_t Address,
                                  const MCDisassembler *Decoder) {
  if (DecodeGR16RegisterClass(MI, (Insn >> 8) & 0xf, Address, Decoder) ==
      MCDisassembler::Fail)
    return MCDisassembler::Fail;
  MCRegister Reg = decodeSFRDirectAddress(Insn >> 16);
  if (!Reg)
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus decodeMOVsfrg(MCInst &MI, uint64_t Insn, uint64_t Address,
                                  const MCDisassembler *Decoder) {
  unsigned DirectAddress = (Insn >> 16) & 0xffff;
  if (MCRegister Reg = decodeSFRDirectAddress(DirectAddress))
    MI.addOperand(MCOperand::createReg(Reg));
  else
    MI.addOperand(MCOperand::createImm(DirectAddress));
  return DecodeGR16RegisterClass(MI, (Insn >> 8) & 0xf, Address, Decoder);
}

#include "C166GenDisassemblerTables.inc"

DecodeStatus C166Disassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                              ArrayRef<uint8_t> Bytes,
                                              uint64_t Address,
                                              raw_ostream &CStream) const {
  if (Bytes.size() < 2) {
    Size = 0;
    return MCDisassembler::Fail;
  }

  const uint16_t Inst16 = support::endian::read16le(Bytes.data());
  DecodeStatus Result =
      decodeInstruction(DecoderTable16, MI, Inst16, Address, this, STI);
  if (Result != MCDisassembler::Fail) {
    Size = 2;
    return Result;
  }

  if (Bytes.size() < 4) {
    Size = 0;
    return MCDisassembler::Fail;
  }

  const uint32_t Inst32 = support::endian::read32le(Bytes.data());
  Result = decodeInstruction(DecoderTable32, MI, Inst32, Address, this, STI);
  Size = Result == MCDisassembler::Fail ? 0 : 4;
  return Result;
}

static MCDisassembler *createC166Disassembler(const Target &T,
                                              const MCSubtargetInfo &STI,
                                              MCContext &Ctx) {
  return new C166Disassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeC166Disassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheC166Target(),
                                         createC166Disassembler);
}
