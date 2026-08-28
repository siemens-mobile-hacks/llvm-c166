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
      static_cast<C166TargetStreamer *>(TS)->emitMemoryModel(TM.getCodeModel());
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

    const bool IsMedium = TM.getCodeModel() == CodeModel::Medium;
    const C166DataClass DataClass = TM.getCodeModel() == CodeModel::Small
                                        ? C166DataClass::Near
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
        if (!IsMedium)
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
    Out.setOpcode(MI->getOpcode());
    const bool IsSegmentedControl =
        MI->getOpcode() == C166::CALLS || MI->getOpcode() == C166::JMPS;
    const bool IsAbsoluteControl =
        MI->getOpcode() == C166::CALLA || MI->getOpcode() == C166::JMPA;
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
    unsigned NumMCOperands =
        IsSegmentedControl ? 2 : MI->getDesc().getNumOperands();
    for (unsigned I = 0; I != NumMCOperands; ++I) {
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
