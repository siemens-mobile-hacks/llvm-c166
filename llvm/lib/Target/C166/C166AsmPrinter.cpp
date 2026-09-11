//===-- C166AsmPrinter.cpp - C166 LLVM assembly writer -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166TargetMachine.h"
#include "MCTargetDesc/C166InstPrinter.h"
#include "MCTargetDesc/C166MCAsmInfo.h"
#include "MCTargetDesc/C166MCTargetDesc.h"
#include "MCTargetDesc/C166TargetStreamer.h"
#include "TargetInfo/C166TargetInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/TargetParser/C166TargetParser.h"

#include <optional>

using namespace llvm;

#define DEBUG_TYPE "c166-asm-printer"

namespace {

class C166AsmPrinter : public AsmPrinter {
  bool EmittingGlobalInitializer = false;
  unsigned ConstantLoweringDepth = 0;

  MCSymbol *getC166JumpTableSymbol(unsigned JTI) {
    SmallString<48> Name;
    raw_svector_ostream(Name)
        << "__c166_jt." << MF->getFunctionNumber() << '.' << JTI;
    MCSymbol *Symbol = OutContext.getOrCreateSymbol(Name);
    static_cast<MCSymbolELF *>(Symbol)->setBinding(ELF::STB_LOCAL);
    return Symbol;
  }

public:
  static char ID;

  C166AsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  StringRef getPassName() const override { return "C166 Assembly Printer"; }

  void emitStartOfAsmFile(Module &M) override {
    AsmPrinter::emitStartOfAsmFile(M);
    if (MCTargetStreamer *TS = OutStreamer->getTargetStreamer())
      static_cast<C166TargetStreamer *>(TS)->emitMemoryModel(
          static_cast<const C166TargetMachine &>(TM).getC166MemoryModel());
  }

  void emitFunctionEntryLabel() override {
    if (MCTargetStreamer *TS = OutStreamer->getTargetStreamer())
      static_cast<C166TargetStreamer *>(TS)->emitFunctionClass(
          *CurrentFnSym,
          MF->getFunction().getAddressSpace() == C166::NearAddressSpace);
    AsmPrinter::emitFunctionEntryLabel();
  }

  void emitGlobalVariable(const GlobalVariable *GV) override {
    if (GV->hasInitializer()) {
      std::optional<C166DataClass> Class;
      switch (GV->getAddressSpace()) {
      case C166::FarDataAddressSpace:
        Class = C166DataClass::Far;
        break;
      case C166::NearAddressSpace:
        Class = C166DataClass::Near;
        break;
      case C166::XNearDataAddressSpace:
        Class = C166DataClass::XNear;
        break;
      case C166::HugeDataAddressSpace:
        Class = C166DataClass::Huge;
        break;
      case C166::SHugeDataAddressSpace:
        Class = C166DataClass::SHuge;
        break;
      default:
        break;
      }
      if (Class)
        if (MCTargetStreamer *TS = OutStreamer->getTargetStreamer())
          static_cast<C166TargetStreamer *>(TS)->emitDataClass(*getSymbol(GV),
                                                               *Class);
    }
    EmittingGlobalInitializer = true;
    AsmPrinter::emitGlobalVariable(GV);
    EmittingGlobalInitializer = false;
  }

  const MCExpr *lowerConstant(const Constant *CV, const Constant *BaseCV,
                              uint64_t Offset) override {
    const bool IsOutermost = ConstantLoweringDepth++ == 0;
    bool IsCodeBlockAddress = false;
    const MCExpr *Expr = nullptr;
    if (const auto *CE = dyn_cast<ConstantExpr>(CV);
        CE && CE->getOpcode() == Instruction::AddrSpaceCast &&
        CE->getType()->getPointerAddressSpace() ==
            getDataLayout().getDefaultGlobalsAddressSpace()) {
      const Constant *Source = CE->getOperand(0);
      if (isa<BlockAddress>(Source)) {
        // GNU label addresses have C type void * and are therefore stored in
        // the default data-pointer representation. Emit the underlying code
        // address without converting it to a data address. A near label is
        // zero-extended into a far slot and stored directly in a near slot.
        IsCodeBlockAddress = true;
        Expr = AsmPrinter::lowerConstant(Source, BaseCV, Offset);
      }
    }
    if (!Expr)
      Expr = AsmPrinter::lowerConstant(CV, BaseCV, Offset);
    --ConstantLoweringDepth;

    // A 32-bit C166 far-data pointer is stored as offset:page, not as a
    // linear integer address.  Keep the distinction in LLVM's own ELF ABI so
    // LLD can encode static pointer initializers without changing ordinary
    // i32, huge-data, or function-pointer relocations.  Medium computed-goto
    // labels deliberately retain their code segment:offset representation.
    if (EmittingGlobalInitializer && IsOutermost && !IsCodeBlockAddress &&
        CV->getType()->isPointerTy() &&
        CV->getType()->getPointerAddressSpace() == C166::FarDataAddressSpace)
      Expr = MCSpecifierExpr::create(Expr, C166::S_PAGED32, OutContext);
    return Expr;
  }

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override {
    if (ExtraCode && ExtraCode[0])
      return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS);

    const MachineOperand &MO = MI->getOperand(OpNo);
    if (MO.isReg()) {
      OS << C166InstPrinter::getRegisterName(MO.getReg());
      return false;
    }
    if (MO.isImm()) {
      OS << MO.getImm();
      return false;
    }
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    SetupMachineFunction(MF);
    emitFunctionBody();
    return false;
  }

  void emitJumpTableInfo() override {
    const MachineJumpTableInfo *MJTI = MF->getJumpTableInfo();
    if (!MJTI || MJTI->getJumpTables().empty())
      return;
    assert(MJTI->getEntryKind() == MachineJumpTableInfo::EK_Inline &&
           "unexpected C166 jump-table encoding");

    const Function &F = MF->getFunction();
    OutStreamer->switchSection(
        getObjFileLowering().getSectionForJumpTable(F, TM));
    emitAlignment(Align(2));

    C166::MemoryModel Model =
        static_cast<const C166TargetMachine &>(TM).getC166MemoryModel();
    const bool IsNearCode = C166::hasNearCode(Model);
    const C166DataClass DataClass =
        C166::hasNearData(Model)           ? C166DataClass::Near
        : Model == C166::MemoryModel::Huge ? C166DataClass::Huge
                                           : C166DataClass::Far;
    for (unsigned JTI = 0; JTI != MJTI->getJumpTables().size(); ++JTI) {
      ArrayRef<MachineBasicBlock *> Entries = MJTI->getJumpTables()[JTI].MBBs;
      if (Entries.empty())
        continue;

      MCSymbol *Table = getC166JumpTableSymbol(JTI);
      OutStreamer->emitSymbolAttribute(Table, MCSA_ELF_TypeObject);
      if (MCTargetStreamer *TS = OutStreamer->getTargetStreamer())
        static_cast<C166TargetStreamer *>(TS)->emitDataClass(*Table, DataClass);
      OutStreamer->emitLabel(Table);
      for (const MachineBasicBlock *MBB : Entries) {
        const MCExpr *Entry =
            MCSymbolRefExpr::create(MBB->getSymbol(), OutContext);
        if (!IsNearCode)
          Entry = MCSpecifierExpr::create(Entry, C166::S_SOF, OutContext);
        OutStreamer->emitValue(Entry, 2);
      }
      OutStreamer->emitELFSize(
          Table, MCConstantExpr::create(Entries.size() * 2, OutContext));
    }
  }

  void emitInstruction(const MachineInstr *MI) override {
    C166_MC::verifyInstructionPredicates(MI->getOpcode(),
                                         getSubtargetInfo().getFeatureBits());
    MCInst Out;
    const bool IsRegisterBitBranch =
        MI->getOpcode() == C166::JBreg || MI->getOpcode() == C166::JNBreg;
    const bool IsRegisterBitUpdate =
        MI->getOpcode() == C166::BCLRreg || MI->getOpcode() == C166::BSETreg;
    const bool IsRegisterBitFieldUpdate =
        MI->getOpcode() == C166::BFLDLreg || MI->getOpcode() == C166::BFLDHreg;
    const bool IsRegisterBitBinary =
        MI->getOpcode() == C166::BMOVreg || MI->getOpcode() == C166::BMOVNreg ||
        MI->getOpcode() == C166::BANDreg || MI->getOpcode() == C166::BORreg ||
        MI->getOpcode() == C166::BXORreg;
    const bool IsPSWBitMove = MI->getOpcode() == C166::BMOVPSWreg;
    const bool IsSFRBitWrite = MI->getOpcode() == C166::BMOVsfrreg;
    const bool IsSFRBitRead = MI->getOpcode() == C166::BMOVregsfr;
    const bool IsRegisterBitCompare = MI->getOpcode() == C166::BCMPreg;
    unsigned MCOpcode = MI->getOpcode();
    if (IsRegisterBitBranch)
      MCOpcode = MI->getOpcode() == C166::JBreg ? C166::JB : C166::JNB;
    else if (IsRegisterBitUpdate)
      MCOpcode = MI->getOpcode() == C166::BCLRreg ? C166::BCLR : C166::BSET;
    else if (IsRegisterBitFieldUpdate)
      MCOpcode = MI->getOpcode() == C166::BFLDLreg ? C166::BFLDL : C166::BFLDH;
    else if (IsRegisterBitBinary || IsPSWBitMove || IsSFRBitWrite ||
             IsSFRBitRead) {
      switch (MI->getOpcode()) {
      case C166::BMOVreg:
      case C166::BMOVPSWreg:
      case C166::BMOVsfrreg:
      case C166::BMOVregsfr:
        MCOpcode = C166::BMOV;
        break;
      case C166::BMOVNreg:
        MCOpcode = C166::BMOVN;
        break;
      case C166::BANDreg:
        MCOpcode = C166::BAND;
        break;
      case C166::BORreg:
        MCOpcode = C166::BOR;
        break;
      case C166::BXORreg:
        MCOpcode = C166::BXOR;
        break;
      default:
        llvm_unreachable("unexpected C166 register bit instruction");
      }
    } else if (IsRegisterBitCompare)
      MCOpcode = C166::BCMP;
    Out.setOpcode(MCOpcode);
    const bool IsSegmentedControl = MI->getOpcode() == C166::CALLS ||
                                    MI->getOpcode() == C166::JMPS ||
                                    MI->getOpcode() == C166::TAILJMPS;
    const bool IsAbsoluteControl = [&] {
      switch (MI->getOpcode()) {
      case C166::CALLA:
      case C166::JMPA:
      case C166::JMPA_EQ:
      case C166::JMPA_NE:
      case C166::JMPA_N:
      case C166::JMPA_NN:
      case C166::JMPA_ULT:
      case C166::JMPA_UGE:
      case C166::JMPA_SGT:
      case C166::JMPA_SLE:
      case C166::JMPA_SLT:
      case C166::JMPA_SGE:
      case C166::JMPA_UGT:
      case C166::JMPA_ULE:
      case C166::TAILJMPA:
        return true;
      default:
        return false;
      }
    }();
    auto GetSpecifier = [&](const MachineOperand &MO,
                            unsigned OperandIndex) -> C166::Specifier {
      switch (MO.getTargetFlags()) {
      case C166II::MO_SEG:
        return C166::S_SEG;
      case C166II::MO_SOF:
        return C166::S_SOF;
      case C166II::MO_PAG:
        return C166::S_PAG;
      case C166II::MO_POF:
        return C166::S_POF;
      case C166II::MO_DPP1:
        return C166::S_DPP1;
      case C166II::MO_DPP2:
        return C166::S_DPP2;
      case C166II::MO_COF:
        return C166::S_COF;
      default:
        break;
      }
      if (IsSegmentedControl)
        return OperandIndex == 0 ? C166::S_SEG : C166::S_SOF;
      if (IsAbsoluteControl)
        return C166::S_COF;
      if (MI->getOpcode() == C166::EXTPp)
        return C166::S_PAG;
      if (MI->getOpcode() == C166::MOVgd || MI->getOpcode() == C166::MOVdg ||
          MI->getOpcode() == C166::MOVBZgd ||
          MI->getOpcode() == C166::MOVBSgd || MI->getOpcode() == C166::MOVBdg)
        return C166::S_POF;
      return C166::S_None;
    };
    unsigned FirstOperand = 0;
    if (IsRegisterBitBinary || IsRegisterBitCompare || IsPSWBitMove ||
        IsSFRBitWrite || IsSFRBitRead) {
      const MCRegisterInfo *MRI = OutContext.getRegisterInfo();
      auto AddBitAddress = [&](unsigned RegisterOperand, unsigned BitOperand) {
        unsigned WordAddress =
            0xf0 |
            MRI->getEncodingValue(MI->getOperand(RegisterOperand).getReg());
        unsigned Bit = MI->getOperand(BitOperand).getImm();
        Out.addOperand(MCOperand::createImm((WordAddress << 4) | Bit));
      };
      if (IsRegisterBitBinary) {
        AddBitAddress(1, 3);
        AddBitAddress(2, 4);
        FirstOperand = 5;
      } else if (IsPSWBitMove) {
        AddBitAddress(1, 2);
        unsigned WordAddress = MRI->getEncodingValue(C166::PSW);
        unsigned Bit = MI->getOperand(3).getImm();
        Out.addOperand(MCOperand::createImm((WordAddress << 4) | Bit));
        FirstOperand = 4;
      } else if (IsSFRBitWrite) {
        Out.addOperand(MCOperand::createImm(MI->getOperand(0).getImm()));
        unsigned WordAddress =
            0xf0 | MRI->getEncodingValue(MI->getOperand(1).getReg());
        unsigned Bit = MI->getOperand(2).getImm();
        Out.addOperand(MCOperand::createImm((WordAddress << 4) | Bit));
        FirstOperand = 3;
      } else if (IsSFRBitRead) {
        AddBitAddress(1, 2);
        Out.addOperand(MCOperand::createImm(MI->getOperand(3).getImm()));
        FirstOperand = 4;
      } else {
        AddBitAddress(0, 2);
        AddBitAddress(1, 3);
        FirstOperand = 4;
      }
    } else if (IsRegisterBitFieldUpdate) {
      const MCRegisterInfo *MRI = OutContext.getRegisterInfo();
      unsigned WordAddress =
          0xf0 | MRI->getEncodingValue(MI->getOperand(1).getReg());
      Out.addOperand(MCOperand::createImm(WordAddress));
      FirstOperand = 2;
    } else if (IsRegisterBitBranch || IsRegisterBitUpdate) {
      const MCRegisterInfo *MRI = OutContext.getRegisterInfo();
      unsigned RegisterOperand = IsRegisterBitUpdate ? 1 : 0;
      unsigned BitOperand = IsRegisterBitUpdate ? 2 : 1;
      unsigned WordAddress =
          0xf0 |
          MRI->getEncodingValue(MI->getOperand(RegisterOperand).getReg());
      unsigned Bit = MI->getOperand(BitOperand).getImm();
      Out.addOperand(MCOperand::createImm((WordAddress << 4) | Bit));
      FirstOperand = IsRegisterBitUpdate ? 3 : 2;
    }
    unsigned NumMCOperands =
        IsSegmentedControl ? 2 : MI->getDesc().getNumOperands();
    for (unsigned I = FirstOperand; I != NumMCOperands; ++I) {
      const MachineOperand &MO = MI->getOperand(I);
      if (MO.isRegMask() || (MO.isReg() && MO.isImplicit()))
        continue;
      if (MO.isReg()) {
        Out.addOperand(MCOperand::createReg(MO.getReg()));
      } else if (MO.isImm()) {
        Out.addOperand(MCOperand::createImm(MO.getImm()));
      } else if (MO.isMBB()) {
        const MCExpr *Expr =
            MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), OutContext);
        if (IsSegmentedControl)
          Expr = MCSpecifierExpr::create(
              Expr, I == 0 ? C166::S_SEG : C166::S_SOF, OutContext);
        else if (IsAbsoluteControl)
          Expr = MCSpecifierExpr::create(Expr, C166::S_COF, OutContext);
        Out.addOperand(MCOperand::createExpr(Expr));
      } else if (MO.isGlobal() || MO.isSymbol() || MO.isJTI()) {
        MCSymbol *Symbol = MO.isGlobal() ? getSymbol(MO.getGlobal())
                           : MO.isSymbol()
                               ? GetExternalSymbolSymbol(MO.getSymbolName())
                               : getC166JumpTableSymbol(MO.getIndex());
        const MCExpr *Expr = MCSymbolRefExpr::create(Symbol, OutContext);
        if (MO.isGlobal() && MO.getOffset())
          Expr = MCBinaryExpr::createAdd(
              Expr, MCConstantExpr::create(MO.getOffset(), OutContext),
              OutContext);
        C166::Specifier Specifier = GetSpecifier(MO, I);
        if (Specifier != C166::S_None)
          Expr = MCSpecifierExpr::create(Expr, Specifier, OutContext);
        Out.addOperand(MCOperand::createExpr(Expr));
      } else {
        MI->print(errs());
        llvm_unreachable("unsupported C166 machine operand");
      }
    }
    EmitToStreamer(*OutStreamer, Out);
  }
};

} // namespace

char C166AsmPrinter::ID;

INITIALIZE_PASS(C166AsmPrinter, DEBUG_TYPE, "C166 Assembly Printer", false,
                false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeC166AsmPrinter() {
  RegisterAsmPrinter<C166AsmPrinter> X(getTheC166Target());
}
