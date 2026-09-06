//===-- C166PostISel.cpp - C166 post-selection optimizations -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166InstrInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "c166-post-isel"

namespace {

static bool callsExternalSymbol(const MachineInstr &MI, StringRef Name) {
  return any_of(MI.operands(), [&](const MachineOperand &Operand) {
    return Operand.isSymbol() && Operand.getSymbolName() == Name;
  });
}

static MachineInstr *previousNonDebugInstruction(MachineInstr &MI) {
  MachineInstr *Previous = MI.getPrevNode();
  while (Previous && Previous->isDebugInstr())
    Previous = Previous->getPrevNode();
  return Previous;
}

static MachineInstr *nextNonDebugInstruction(MachineInstr &MI) {
  MachineInstr *Next = MI.getNextNode();
  while (Next && Next->isDebugInstr())
    Next = Next->getNextNode();
  return Next;
}

static Register getCopiedSource(const MachineInstr *Copy,
                                Register Destination) {
  if (!Copy || !Copy->isCopy() || Copy->getNumOperands() < 2 ||
      !Copy->getOperand(0).isReg() || !Copy->getOperand(1).isReg() ||
      Copy->getOperand(0).getReg() != Destination ||
      Copy->getOperand(0).getSubReg() || Copy->getOperand(1).getSubReg())
    return Register();
  return Copy->getOperand(1).getReg();
}

struct ExtendedWord {
  Register Source;
  unsigned MultiplyOpcode;
};

static ExtendedWord getExtendedWord(Register Reg, MachineRegisterInfo &MRI) {
  if (!Reg.isVirtual())
    return {};
  MachineInstr *Def = MRI.getUniqueVRegDef(Reg);
  if (!Def || Def->getNumOperands() < 2 || !Def->getOperand(1).isReg() ||
      Def->getOperand(1).getSubReg())
    return {};
  if (Def->getOpcode() == C166::ZEXT16_32)
    return {Def->getOperand(1).getReg(), C166::UMUL16_32};
  if (Def->getOpcode() == C166::SEXT16_32)
    return {Def->getOperand(1).getReg(), C166::SMUL16_32};
  return {};
}

static bool foldWideningMultiplyAdds(MachineFunction &MF,
                                     const C166InstrInfo &TII) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &Add : make_early_inc_range(MBB)) {
      if (Add.getOpcode() != C166::ADD32rr)
        continue;

      unsigned ProductOperand = 0;
      MachineInstr *Multiply = nullptr;
      for (unsigned Operand : {2u, 1u}) {
        if (!Add.getOperand(Operand).isReg())
          continue;
        Register Product = Add.getOperand(Operand).getReg();
        if (!Product.isVirtual() || !MRI.hasOneNonDBGUse(Product))
          continue;
        MachineInstr *Def = MRI.getUniqueVRegDef(Product);
        if (Def && Def->getParent() == &MBB &&
            (Def->getOpcode() == C166::SMUL16_32 ||
             Def->getOpcode() == C166::UMUL16_32)) {
          ProductOperand = Operand;
          Multiply = Def;
          break;
        }
      }
      if (!Multiply)
        continue;

      if (nextNonDebugInstruction(*Multiply) != &Add) {
        unsigned Distance = 0;
        MachineInstr *Scan = nextNonDebugInstruction(*Multiply);
        for (; Scan && Scan != &Add && Distance <= 3;
             Scan = nextNonDebugInstruction(*Scan)) {
          if (Scan->isCall() || Scan->isInlineAsm() || Scan->isTerminator() ||
              Scan->hasUnmodeledSideEffects() ||
              Scan->readsRegister(C166::MDL, &TRI) ||
              Scan->readsRegister(C166::MDH, &TRI) ||
              Scan->readsRegister(C166::MDC, &TRI))
            break;
          ++Distance;
        }
        if (Scan != &Add || Distance > 3)
          continue;

        bool OperandAlreadyLive = false;
        for (unsigned Operand : {1u, 2u}) {
          Register Reg = Multiply->getOperand(Operand).getReg();
          for (auto Use = std::next(Add.getIterator()); Use != MBB.end(); ++Use)
            OperandAlreadyLive |=
                !Use->isDebugInstr() && Use->readsRegister(Reg, &TRI);
        }
        if (!OperandAlreadyLive)
          continue;
      }

      unsigned AddendOperand = ProductOperand == 1 ? 2 : 1;
      unsigned FusedOpcode = Multiply->getOpcode() == C166::UMUL16_32
                                 ? C166::UMUL16_ADD32
                                 : C166::SMUL16_ADD32;
      MachineInstrBuilder Fused =
          BuildMI(MBB, Add, MIMetadata(Add), TII.get(FusedOpcode),
                  Add.getOperand(0).getReg())
              .add(Add.getOperand(AddendOperand))
              .add(Multiply->getOperand(1))
              .add(Multiply->getOperand(2));
      Fused->getOperand(0).setIsDead(Add.getOperand(0).isDead());
      Fused->setFlags(Add.getFlags() | Multiply->getFlags());

      Multiply->eraseFromParent();
      Add.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

struct ExtendedByte {
  Register Source;
  unsigned ExtensionOpcode;
};

static ExtendedByte getExtendedByte(Register Reg, MachineRegisterInfo &MRI) {
  while (Reg.isVirtual()) {
    MachineInstr *Def = MRI.getUniqueVRegDef(Reg);
    if (!Def)
      return {};
    if (Def->isCopy() && Def->getNumOperands() >= 2 &&
        Def->getOperand(1).isReg() && !Def->getOperand(1).getSubReg()) {
      Reg = Def->getOperand(1).getReg();
      continue;
    }

    switch (Def->getOpcode()) {
    case C166::SEXT8rr:
    case C166::ZEXT8rr:
      if (!Def->getOperand(1).isReg() || Def->getOperand(1).getSubReg())
        return {};
      return {Def->getOperand(1).getReg(), Def->getOpcode()};
    case C166::FARLOAD8S:
    case C166::FARLOAD8S_POSTINC:
    case C166::FRAMELOAD8S:
    case C166::NEARLOAD8S:
    case C166::NEARLOAD8S_POSTINC:
    case C166::SEGLOAD8S:
      return {Reg, C166::SEXT8rr};
    case C166::FARLOAD8Z:
    case C166::FARLOAD8Z_POSTINC:
    case C166::FRAMELOAD8Z:
    case C166::NEARLOAD8Z:
    case C166::NEARLOAD8Z_POSTINC:
    case C166::SEGLOAD8Z:
      return {Reg, C166::ZEXT8rr};
    default:
      return {};
    }
  }
  return {};
}

static bool isByteLoad(Register Reg, MachineRegisterInfo &MRI) {
  ExtendedByte Value = getExtendedByte(Reg, MRI);
  return Value.Source == Reg;
}

static bool foldExtendedByteBranches(MachineFunction &MF,
                                     const C166InstrInfo &TII,
                                     MachineDominatorTree &MDT) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  bool Changed = false;

  auto RemovesExtension = [&](Register Reg) {
    bool FoundExtension = false;
    while (Reg.isVirtual() && MRI.hasOneUse(Reg)) {
      MachineInstr *Def = MRI.getUniqueVRegDef(Reg);
      if (!Def || Def->getNumOperands() < 2 || !Def->getOperand(1).isReg() ||
          Def->getOperand(1).getSubReg())
        return false;
      if (Def->isCopy()) {
        Reg = Def->getOperand(1).getReg();
        continue;
      }
      if (Def->getOpcode() != C166::SEXT8rr &&
          Def->getOpcode() != C166::ZEXT8rr)
        return false;
      FoundExtension = true;
      Reg = Def->getOperand(1).getReg();
    }
    return FoundExtension;
  };

  auto CrossesCall = [&](Register Reg, MachineInstr &Branch) {
    if (!Reg.isVirtual())
      return true;
    MachineInstr *Def = MRI.getUniqueVRegDef(Reg);
    if (!Def)
      return true;
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : MBB)
        if (MI.isCall() && MDT.dominates(Def, &MI) &&
            MDT.dominates(&MI, &Branch))
          return true;
    return false;
  };

  auto RemoveDeadExtensionChain = [&](Register Reg) {
    while (Reg.isVirtual() && MRI.use_empty(Reg)) {
      MachineInstr *Def = MRI.getUniqueVRegDef(Reg);
      if (!Def ||
          (!Def->isCopy() && Def->getOpcode() != C166::SEXT8rr &&
           Def->getOpcode() != C166::ZEXT8rr) ||
          Def->getNumOperands() < 2 || !Def->getOperand(1).isReg() ||
          Def->getOperand(1).getSubReg())
        break;
      Register Source = Def->getOperand(1).getReg();
      Def->eraseFromParent();
      Reg = Source;
    }
  };

  auto HasOnlyByteUsesAfterRewrite = [&](Register Source, Register Root,
                                         MachineInstr &Branch) {
    SmallVector<MachineInstr *, 4> Chain;
    Register Reg = Root;
    while (Reg != Source) {
      if (!Reg.isVirtual() || !MRI.hasOneUse(Reg))
        return false;
      MachineInstr *Def = MRI.getUniqueVRegDef(Reg);
      if (!Def || !Def->isCopy() || Def->getNumOperands() < 2 ||
          !Def->getOperand(1).isReg() || Def->getOperand(1).getSubReg())
        return false;
      Chain.push_back(Def);
      Reg = Def->getOperand(1).getReg();
    }

    for (MachineInstr &Use : MRI.use_nodbg_instructions(Source)) {
      if (is_contained(Chain, &Use))
        continue;
      if (Use.getOpcode() == C166::CMPBBR &&
          ((Use.getOperand(0).isReg() &&
            Use.getOperand(0).getReg() == Source) ||
           (Use.getOperand(1).isReg() && Use.getOperand(1).getReg() == Source)))
        continue;
      if (&Use == &Branch && Root == Source)
        continue;
      return false;
    }
    return true;
  };

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() != C166::CMPBR ||
          (MI.getOperand(2).getImm() != C166::CC_EQ &&
           MI.getOperand(2).getImm() != C166::CC_NE))
        continue;

      ExtendedByte LHS = getExtendedByte(MI.getOperand(0).getReg(), MRI);
      ExtendedByte RHS = getExtendedByte(MI.getOperand(1).getReg(), MRI);
      bool NarrowsLHS =
          LHS.Source && (RemovesExtension(MI.getOperand(0).getReg()) ||
                         (isByteLoad(LHS.Source, MRI) &&
                          HasOnlyByteUsesAfterRewrite(
                              LHS.Source, MI.getOperand(0).getReg(), MI)));
      bool NarrowsRHS =
          RHS.Source && (RemovesExtension(MI.getOperand(1).getReg()) ||
                         (isByteLoad(RHS.Source, MRI) &&
                          HasOnlyByteUsesAfterRewrite(
                              RHS.Source, MI.getOperand(1).getReg(), MI)));
      if (!LHS.Source || !RHS.Source ||
          LHS.ExtensionOpcode != RHS.ExtensionOpcode ||
          (!NarrowsLHS && !NarrowsRHS) || CrossesCall(LHS.Source, MI) ||
          CrossesCall(RHS.Source, MI))
        continue;

      Register OldLHS = MI.getOperand(0).getReg();
      Register OldRHS = MI.getOperand(1).getReg();
      MRI.clearKillFlags(LHS.Source);
      MRI.clearKillFlags(RHS.Source);
      MI.setDesc(TII.get(C166::CMPBBR));
      MI.getOperand(0).setReg(LHS.Source);
      MI.getOperand(0).setIsKill(false);
      MI.getOperand(1).setReg(RHS.Source);
      MI.getOperand(1).setIsKill(false);
      RemoveDeadExtensionChain(OldLHS);
      RemoveDeadExtensionChain(OldRHS);
      Changed = true;
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() != C166::CMPBRi ||
          (MI.getOperand(2).getImm() != C166::CC_EQ &&
           MI.getOperand(2).getImm() != C166::CC_NE))
        continue;

      Register OldValue = MI.getOperand(0).getReg();
      ExtendedByte Value = getExtendedByte(OldValue, MRI);
      if (!Value.Source || CrossesCall(Value.Source, MI))
        continue;

      uint16_t Immediate = static_cast<uint16_t>(MI.getOperand(1).getImm());
      bool FitsExtension =
          Value.ExtensionOpcode == C166::ZEXT8rr
              ? isUInt<8>(Immediate)
              : static_cast<uint16_t>(static_cast<int16_t>(
                    static_cast<int8_t>(Immediate))) == Immediate;
      bool IsProfitable =
          RemovesExtension(OldValue) ||
          (isByteLoad(Value.Source, MRI) &&
           HasOnlyByteUsesAfterRewrite(Value.Source, OldValue, MI));
      if (!FitsExtension || !IsProfitable)
        continue;

      MRI.clearKillFlags(Value.Source);
      MI.setDesc(TII.get(C166::CMPBBRi));
      MI.getOperand(0).setReg(Value.Source);
      MI.getOperand(0).setIsKill(false);
      MI.getOperand(1).setImm(static_cast<uint8_t>(Immediate));
      RemoveDeadExtensionChain(OldValue);
      Changed = true;
    }
  }

  return Changed;
}

// A widening multiply can reach instruction selection with one extension in a
// predecessor block.  SelectionDAG then sees that operand only as an i32
// live-in and emits __mulsi3.  Recover the extensions from Machine SSA and use
// the native 16x16-to-32 instruction when both operands have the same
// signedness.
static bool foldWideningMultiplyLibcalls(MachineFunction &MF,
                                         const C166InstrInfo &TII) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallVector<MachineInstr *, 4> Calls;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if ((MI.getOpcode() == C166::CALLA || MI.getOpcode() == C166::CALLS) &&
          callsExternalSymbol(MI, "__mulsi3"))
        Calls.push_back(&MI);

  bool Changed = false;
  for (MachineInstr *Call : Calls) {
    MachineInstr *SecondCopy = previousNonDebugInstruction(*Call);
    if (!SecondCopy)
      continue;
    MachineInstr *FirstCopy = previousNonDebugInstruction(*SecondCopy);
    MachineInstr *ResultCopy = nextNonDebugInstruction(*Call);
    if (!FirstCopy || !ResultCopy || !ResultCopy->isCopy() ||
        ResultCopy->getNumOperands() < 2 ||
        !ResultCopy->getOperand(0).isReg() ||
        !ResultCopy->getOperand(1).isReg() ||
        !ResultCopy->getOperand(0).getReg().isVirtual() ||
        ResultCopy->getOperand(0).getSubReg() ||
        ResultCopy->getOperand(1).getReg() != C166::R5R4 ||
        ResultCopy->getOperand(1).getSubReg())
      continue;

    Register FirstArgument = getCopiedSource(FirstCopy, C166::R13R12);
    Register SecondArgument = getCopiedSource(SecondCopy, C166::R15R14);
    if (!FirstArgument || !SecondArgument) {
      FirstArgument = getCopiedSource(SecondCopy, C166::R13R12);
      SecondArgument = getCopiedSource(FirstCopy, C166::R15R14);
    }
    if (!FirstArgument || !SecondArgument)
      continue;

    ExtendedWord Left = getExtendedWord(FirstArgument, MRI);
    ExtendedWord Right = getExtendedWord(SecondArgument, MRI);
    if (!Left.Source || !Right.Source ||
        Left.MultiplyOpcode != Right.MultiplyOpcode)
      continue;

    MRI.clearKillFlags(Left.Source);
    MRI.clearKillFlags(Right.Source);
    BuildMI(*Call->getParent(), *Call, MIMetadata(*Call),
            TII.get(Left.MultiplyOpcode), ResultCopy->getOperand(0).getReg())
        .addReg(Left.Source)
        .addReg(Right.Source);
    FirstCopy->eraseFromParent();
    SecondCopy->eraseFromParent();
    ResultCopy->eraseFromParent();
    Call->eraseFromParent();
    Changed = true;
  }
  return Changed;
}

static bool rematerializeAddCarryBranches(MachineFunction &MF,
                                          const C166InstrInfo &TII) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  SmallVector<MachineInstr *, 4> Branches;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.getOpcode() == C166::FLAGSBR)
        Branches.push_back(&MI);

  bool Changed = false;
  for (MachineInstr *Branch : Branches) {
    Register Carry = Branch->getOperand(0).getReg();
    unsigned CC = Branch->getOperand(1).getImm();
    if (!Carry.isVirtual() || !MRI.hasOneNonDBGUse(Carry) ||
        (CC != C166::CC_ULT && CC != C166::CC_UGE))
      continue;

    MachineInstr *Add = MRI.getUniqueVRegDef(Carry);
    if (!Add || Add->getOpcode() != C166::ADDCarryri3 ||
        Add->getParent() != Branch->getParent() ||
        Add->getOperand(1).getReg() != Carry || !Add->getOperand(0).isReg() ||
        !Add->getOperand(0).getReg().isVirtual() || !Add->getOperand(3).isImm())
      continue;

    bool ClobbersCarry = false;
    for (auto I = std::next(Add->getIterator()); I != Branch->getIterator();
         ++I)
      ClobbersCarry |= I->modifiesRegister(C166::C, &TRI);
    if (!ClobbersCarry)
      continue;

    MachineBasicBlock &MBB = *Branch->getParent();
    if (llvm::any_of(MBB.successors(), [](const MachineBasicBlock *Successor) {
          return Successor->isLiveIn(C166::PSW) || Successor->isLiveIn(C166::C);
        }))
      continue;

    Register Result = Add->getOperand(0).getReg();
    MRI.clearKillFlags(Result);
    MachineInstrBuilder Compare =
        BuildMI(MBB, *Branch, MIMetadata(*Branch), TII.get(C166::CMPBRi))
            .addReg(Result)
            .addImm(Add->getOperand(3).getImm())
            .addImm(CC)
            .addMBB(Branch->getOperand(2).getMBB());
    for (Register Flag : {Register(C166::PSW), Register(C166::C)})
      if (MachineOperand *Def = Compare->findRegisterDefOperand(Flag, &TRI))
        Def->setIsDead(true);
    Compare->setFlags(Branch->getFlags());

    Branch->eraseFromParent();
    Add->getOperand(1).setIsDead(true);
    Changed = true;
  }
  return Changed;
}

static MachineInstr *getAddImmediateDef(Register Reg,
                                        MachineRegisterInfo &MRI) {
  if (!Reg.isVirtual())
    return nullptr;
  MachineInstr *Def = MRI.getUniqueVRegDef(Reg);
  if (!Def ||
      (Def->getOpcode() != C166::ADDri3 && Def->getOpcode() != C166::ADDri16))
    return nullptr;
  return Def;
}

static MachineInstr *getCompareBranch(Register Reg, MachineRegisterInfo &MRI) {
  for (MachineInstr &Use : MRI.use_nodbg_instructions(Reg)) {
    if ((Use.getOpcode() == C166::CMPBR || Use.getOpcode() == C166::CMPBRi) &&
        Use.getOperand(0).getReg() == Reg)
      return &Use;
  }
  return nullptr;
}

static bool foldShortImmediatePhis(MachineFunction &MF,
                                   const C166InstrInfo &TII) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &Phi : MBB.phis()) {
      if (Phi.getNumOperands() != 5)
        continue;

      for (unsigned BaselineOperand : {1u, 3u}) {
        unsigned OtherOperand = BaselineOperand == 1 ? 3 : 1;
        Register BaselineReg = Phi.getOperand(BaselineOperand).getReg();
        Register OtherReg = Phi.getOperand(OtherOperand).getReg();
        MachineInstr *Baseline = getAddImmediateDef(BaselineReg, MRI);
        MachineInstr *Other = getAddImmediateDef(OtherReg, MRI);
        MachineInstr *Branch = getCompareBranch(BaselineReg, MRI);
        if (!Baseline || !Other || !Branch ||
            Baseline->getParent() != Other->getParent() ||
            Baseline->getParent() != Branch->getParent() ||
            Baseline->getOperand(1).getReg() != Other->getOperand(1).getReg() ||
            !MRI.hasOneNonDBGUse(OtherReg))
          continue;

        uint16_t BaselineImmediate =
            static_cast<uint16_t>(Baseline->getOperand(2).getImm());
        uint16_t OtherImmediate =
            static_cast<uint16_t>(Other->getOperand(2).getImm());
        uint16_t Adjustment = OtherImmediate - BaselineImmediate;
        if (Adjustment == 0 || Adjustment > 7)
          continue;

        // C166 has a two-byte ADD immediate for 0..7.  Express the alternate
        // PHI value relative to the value already needed by the comparison;
        // MachineSink can then place this short adjustment on its CFG edge.
        Other->setDesc(TII.get(C166::ADDri3));
        Other->getOperand(1).setReg(BaselineReg);
        Other->getOperand(2).setImm(Adjustment);
        Other->clearFlag(MachineInstr::NoSWrap);
        Other->clearFlag(MachineInstr::NoUWrap);
        Other->moveBefore(Branch);
        Changed = true;
        break;
      }
    }
  }
  return Changed;
}

// Reuse an already required no-overflow offset when a signed comparison asks
// whether the offset result is nonnegative. This replaces a full immediate
// comparison with a sign-bit branch and can express the adjacent offset as a
// short increment.
static bool foldOffsetSignBranches(MachineFunction &MF,
                                   const C166InstrInfo &TII) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &Branch : make_early_inc_range(MBB)) {
      if (Branch.getOpcode() != C166::CMPBRi ||
          Branch.getOperand(2).getImm() != C166::CC_SGT ||
          !Branch.getOperand(0).isReg() ||
          !Branch.getOperand(0).getReg().isVirtual() ||
          !Branch.getOperand(1).isImm() ||
          !Branch.getOperand(3).isMBB())
        continue;

      bool DeadPhysicalDefs = true;
      for (const MachineOperand &Def : Branch.all_defs())
        if (Def.getReg().isPhysical() && !Def.isDead())
          DeadPhysicalDefs = false;
      if (!DeadPhysicalDefs)
        continue;

      Register Base = Branch.getOperand(0).getReg();
      MachineInstr *Offset = nullptr;
      int64_t Amount = 0;
      for (MachineInstr &MI : MBB) {
        if (&MI == &Branch)
          break;
        if ((MI.getOpcode() != C166::ADDri3 &&
             MI.getOpcode() != C166::ADDri16) ||
            !MI.getFlag(MachineInstr::NoSWrap) ||
            !MI.getOperand(0).isReg() ||
            !MI.getOperand(0).getReg().isVirtual() ||
            !MI.getOperand(1).isReg() || MI.getOperand(1).getReg() != Base ||
            !MI.getOperand(2).isImm())
          continue;
        int64_t CandidateAmount = MI.getOperand(2).getImm();
        if (CandidateAmount <= 0 || CandidateAmount > INT16_MAX ||
            Branch.getOperand(1).getImm() != -CandidateAmount - 1)
          continue;
        Offset = &MI;
        Amount = CandidateAmount;
      }
      if (!Offset)
        continue;

      Register Adjusted = Offset->getOperand(0).getReg();
      MRI.clearKillFlags(Adjusted);
      MachineInstrBuilder BitBranch =
          BuildMI(MBB, Branch, MIMetadata(Branch), TII.get(C166::BITBR))
              .addReg(Adjusted)
              .addImm(15)
              .addImm(C166::CC_EQ)
              .add(Branch.getOperand(3));
      BitBranch->setFlags(Branch.getFlags());

      if (MBB.succ_size() == 2) {
        MachineBasicBlock *Target = Branch.getOperand(3).getMBB();
        MachineBasicBlock *Other = nullptr;
        for (MachineBasicBlock *Successor : MBB.successors())
          if (Successor != Target)
            Other = Successor;

        if (Other && Other->pred_size() == 1)
          for (MachineInstr &MI : *Other) {
            if (MI.isDebugInstr() || MI.isMetaInstruction() || MI.isPHI())
              continue;
            if ((MI.getOpcode() == C166::ADDri3 ||
                 MI.getOpcode() == C166::ADDri16) &&
                MI.getFlag(MachineInstr::NoSWrap) &&
                MI.getOperand(1).isReg() &&
                MI.getOperand(1).getReg() == Base &&
                MI.getOperand(2).isImm() &&
                MI.getOperand(2).getImm() == Amount + 1) {
              MI.setDesc(TII.get(C166::ADDri3));
              MI.getOperand(1).setReg(Adjusted);
              MI.getOperand(2).setImm(1);
              MI.clearFlag(MachineInstr::NoSWrap);
              MI.clearFlag(MachineInstr::NoUWrap);
            }
            break;
          }
      }

      Branch.eraseFromParent();
      Changed = true;
    }
  }
  return Changed;
}

struct FrameAddress {
  int FrameIndex;
  uint16_t Offset;
};

static bool getFrameAddress(Register Address, MachineRegisterInfo &MRI,
                            FrameAddress &Result) {
  uint32_t Offset = 0;
  for (unsigned Depth = 0; Depth != 8; ++Depth) {
    if (!Address.isVirtual())
      return false;
    MachineInstr *Def = MRI.getUniqueVRegDef(Address);
    if (!Def)
      return false;

    if (Def->getOpcode() == C166::FRAMEADDR32) {
      if (!Def->getOperand(1).isFI() || !Def->getOperand(2).isImm())
        return false;
      int64_t Displacement = Def->getOperand(2).getImm();
      if (Displacement < 0 || Displacement >= 0x4000 ||
          Offset + Displacement >= 0x4000)
        return false;
      Result = {Def->getOperand(1).getIndex(),
                static_cast<uint16_t>(Offset + Displacement)};
      return true;
    }

    if (Def->isCopy()) {
      const MachineOperand &Source = Def->getOperand(1);
      if (!Source.isReg() || Source.getSubReg())
        return false;
      Address = Source.getReg();
      continue;
    }

    int64_t Increment;
    if (Def->getOpcode() == C166::FARADD32i ||
        Def->getOpcode() == C166::ADD32ri) {
      if (!Def->getOperand(1).isReg() || !Def->getOperand(2).isImm())
        return false;
      Address = Def->getOperand(1).getReg();
      Increment = Def->getOperand(2).getImm();
    } else if (Def->getOpcode() == C166::FARADD32) {
      if (!Def->getOperand(1).isReg() || !Def->getOperand(2).isReg())
        return false;
      MachineInstr *IncrementDef =
          MRI.getUniqueVRegDef(Def->getOperand(2).getReg());
      if (!IncrementDef ||
          (IncrementDef->getOpcode() != C166::MOVri4 &&
           IncrementDef->getOpcode() != C166::MOVri16) ||
          !IncrementDef->getOperand(1).isImm())
        return false;
      Address = Def->getOperand(1).getReg();
      Increment = IncrementDef->getOperand(1).getImm();
    } else {
      return false;
    }

    if (Increment < 0 || Increment >= 0x4000 || Offset + Increment >= 0x4000)
      return false;
    Offset += Increment;
  }
  return false;
}

static bool foldFrameMemoryAddresses(MachineFunction &MF,
                                     const C166InstrInfo &TII) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallVector<MachineInstr *, 16> MemoryInstructions;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      switch (MI.getOpcode()) {
      case C166::FARLOAD8Z:
      case C166::FARLOAD8S:
      case C166::FARSTORE8:
      case C166::FARLOAD16:
      case C166::FARSTORE16:
      case C166::FARLOAD32:
      case C166::FARSTORE32:
        MemoryInstructions.push_back(&MI);
        break;
      default:
        break;
      }

  bool Changed = false;
  for (MachineInstr *MI : MemoryInstructions) {
    bool IsLoad = MI->mayLoad();
    unsigned AddressOperand = IsLoad ? 1 : 0;
    FrameAddress Address;
    if (!getFrameAddress(MI->getOperand(AddressOperand).getReg(), MRI, Address))
      continue;
    int64_t Displacement = MI->getOperand(IsLoad ? 2 : 1).getImm();
    if (Displacement < 0 || Displacement >= 0x4000 ||
        Address.Offset + Displacement >= 0x4000)
      continue;
    Address.Offset += Displacement;

    unsigned Opcode;
    switch (MI->getOpcode()) {
    case C166::FARLOAD8Z:
      Opcode = C166::FRAMELOAD8Z;
      break;
    case C166::FARLOAD8S:
      Opcode = C166::FRAMELOAD8S;
      break;
    case C166::FARSTORE8:
      Opcode = C166::FRAMESTORE8;
      break;
    case C166::FARLOAD16:
      Opcode = C166::MOVfi;
      break;
    case C166::FARSTORE16:
      Opcode = C166::MOVfiStore;
      break;
    case C166::FARLOAD32:
      Opcode = C166::FRAMELOAD32;
      break;
    case C166::FARSTORE32:
      Opcode = C166::FRAMESTORE32;
      break;
    default:
      llvm_unreachable("unexpected C166 far-memory instruction");
    }

    MachineBasicBlock &MBB = *MI->getParent();
    MachineInstrBuilder MIB =
        IsLoad ? BuildMI(MBB, *MI, MIMetadata(*MI), TII.get(Opcode),
                         MI->getOperand(0).getReg())
               : BuildMI(MBB, *MI, MIMetadata(*MI), TII.get(Opcode));
    if (IsLoad) {
      MachineOperand &Destination = MIB->getOperand(0);
      Destination.setSubReg(MI->getOperand(0).getSubReg());
      Destination.setIsDead(MI->getOperand(0).isDead());
    }
    MIB.addFrameIndex(Address.FrameIndex).addImm(Address.Offset);
    if (!IsLoad)
      MIB.add(MI->getOperand(2));
    MIB.cloneMemRefs(*MI).setMIFlags(MI->getFlags());
    MI->eraseFromParent();
    Changed = true;
  }
  return Changed;
}

static bool canMoveIndirectLoadTo(const MachineInstr &Load,
                                  const MachineInstr &Use,
                                  const TargetRegisterInfo &TRI) {
  if (Load.getParent() != Use.getParent() || Load.hasOrderedMemoryRef() ||
      llvm::any_of(Load.memoperands(), [](const MachineMemOperand *MMO) {
        return MMO->isVolatile() || MMO->isAtomic();
      }))
    return false;

  bool PostIncrement = Load.getOpcode() == C166::MOVrmPostInc;
  if (PostIncrement) {
    const MachineInstr *Next = Load.getNextNode();
    while (Next && Next->isDebugInstr())
      Next = Next->getNextNode();
    if (Next != &Use)
      return false;
  }

  Register Base = Load.getOperand(PostIncrement ? 2 : 1).getReg();
  for (const MachineInstr *I = Load.getNextNode(); I; I = I->getNextNode()) {
    if (I == &Use)
      return true;
    if (I->isCall() || I->isInlineAsm() || I->hasUnmodeledSideEffects() ||
        I->mayStore() || I->hasOrderedMemoryRef() ||
        I->modifiesRegister(Base, &TRI) ||
        llvm::any_of(I->memoperands(), [](const MachineMemOperand *MMO) {
          return MMO->isVolatile() || MMO->isAtomic();
        }))
      return false;
  }
  return false;
}

static bool foldIndirectALULoads(MachineFunction &MF,
                                 const C166InstrInfo &TII) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  SmallVector<MachineInstr *, 16> Loads;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if ((MI.getOpcode() == C166::MOVrm ||
           MI.getOpcode() == C166::MOVrmPostInc) &&
          MI.getOperand(0).isReg() && MI.getOperand(0).getReg().isVirtual())
        Loads.push_back(&MI);

  bool Changed = false;
  for (MachineInstr *Load : Loads) {
    if (!Load->getParent())
      continue;
    Register Value = Load->getOperand(0).getReg();
    MachineOperand *PSWDef = Load->findRegisterDefOperand(C166::PSW, &TRI);
    if (!PSWDef || !PSWDef->isDead() || !MRI.hasOneNonDBGUse(Value))
      continue;

    MachineInstr &Use = *MRI.use_nodbg_instructions(Value).begin();
    if (!canMoveIndirectLoadTo(*Load, Use, TRI))
      continue;

    unsigned Operand = 0;
    while (Operand != Use.getNumExplicitOperands() &&
           (!Use.getOperand(Operand).isReg() ||
            Use.getOperand(Operand).getReg() != Value ||
            Use.getOperand(Operand).isDef()))
      ++Operand;
    if (Operand == Use.getNumExplicitOperands())
      continue;

    MachineInstr *Copy = nullptr;
    MachineInstr *Folded = TII.foldMemoryOperandImpl(MF, Use, {Operand}, *Load,
                                                     Copy, nullptr, nullptr);
    if (!Folded)
      continue;
    assert(!Copy && "C166 indirect ALU folding created an unexpected copy");
    Folded->setMemRefs(MF, Load->memoperands());
    MRI.markUsesInDebugValueAsUndef(Value);
    Use.eraseFromParent();
    Load->eraseFromParent();
    Changed = true;
  }
  return Changed;
}

static bool isDeadWithDiscardedFlags(const MachineInstr &MI,
                                     const MachineRegisterInfo &MRI) {
  bool HasDiscardedFlagDef = false;
  for (const MachineOperand &Def : MI.all_defs()) {
    Register Reg = Def.getReg();
    if (Reg.isPhysical()) {
      if ((Reg != C166::PSW && Reg != C166::C) || !Def.isDead())
        return false;
      HasDiscardedFlagDef = true;
      continue;
    }

    for (const MachineInstr &Use : MRI.use_nodbg_instructions(Reg))
      if (&Use != &MI)
        return false;
  }
  return HasDiscardedFlagDef && MI.wouldBeTriviallyDead();
}

static bool eliminateDeadInstructionsWithDiscardedFlags(MachineFunction &MF) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  bool Changed = false;
  bool IterationChanged;
  do {
    IterationChanged = false;
    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : make_early_inc_range(reverse(MBB))) {
        // Generic Machine DCE deliberately preserves every definition of a
        // reserved physical register.  C166 pseudos model their eventual flag
        // clobbers, so an otherwise dead address or value computation survives
        // when those clobbers are explicitly marked dead.  In that case none
        // of the instruction's results are observable.
        if (!isDeadWithDiscardedFlags(MI, MRI))
          continue;
        MI.eraseFromParent();
        Changed = IterationChanged = true;
      }
    }
  } while (IterationChanged);
  return Changed;
}

static bool isReusableImmediate(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case C166::MOVri4:
  case C166::MOVri16:
  case C166::CONST32:
    break;
  default:
    return false;
  }

  if (!MI.getOperand(0).isReg() || !MI.getOperand(0).getReg().isVirtual() ||
      !MI.getOperand(1).isImm())
    return false;

  for (const MachineOperand &Def : MI.all_defs()) {
    if (!Def.getReg().isPhysical())
      continue;
    if ((Def.getReg() != C166::PSW && Def.getReg() != C166::C) || !Def.isDead())
      return false;
  }
  return true;
}

static bool reuseDominatingImmediates(MachineFunction &MF,
                                      MachineDominatorTree &MDT) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  // Extending a rematerializable value through a large CFG can cost more in
  // spills than the removed instruction saves.  Keep this local to small
  // control-flow regions where the added live range is bounded.
  if (MF.size() > 4)
    return false;

  SmallVector<MachineInstr *, 16> Available;
  bool Changed = false;

  auto HasOnePhiUse = [&](Register Reg) {
    MachineInstr *Use = nullptr;
    for (MachineInstr &MI : MRI.use_nodbg_instructions(Reg)) {
      if (Use || !MI.isPHI())
        return false;
      Use = &MI;
    }
    return Use != nullptr;
  };

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : make_early_inc_range(MBB)) {
      if (!isReusableImmediate(MI))
        continue;

      Register CurrentReg = MI.getOperand(0).getReg();
      // Merging arbitrary constants can extend their live ranges across a
      // large CFG.  Restrict this to the SSA join pattern where equivalent
      // constants initialize separate PHIs; sharing the value then removes a
      // materialization without keeping an unrelated temporary live.
      if (!HasOnePhiUse(CurrentReg))
        continue;

      MachineInstr *Dominating = nullptr;
      for (MachineInstr *Candidate : reverse(Available)) {
        Register CandidateReg = Candidate->getOperand(0).getReg();
        if (Candidate->getOpcode() == MI.getOpcode() &&
            Candidate->getOperand(1).getImm() == MI.getOperand(1).getImm() &&
            MRI.getRegClass(CurrentReg) == MRI.getRegClass(CandidateReg) &&
            MDT.dominates(Candidate, &MI)) {
          Dominating = Candidate;
          break;
        }
      }
      if (!Dominating) {
        Available.push_back(&MI);
        continue;
      }

      Register Old = MI.getOperand(0).getReg();
      Register Replacement = Dominating->getOperand(0).getReg();
      MRI.replaceRegWith(Old, Replacement);
      MI.eraseFromParent();
      Changed = true;
    }
  }
  return Changed;
}

static bool narrowSingleWordTestMasks(MachineFunction &MF,
                                      const C166InstrInfo &TII) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallVector<MachineInstr *, 8> Masks;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.getOpcode() == C166::AND32ri)
        Masks.push_back(&MI);

  bool Changed = false;

  for (MachineInstr *MaskPtr : Masks) {
    MachineInstr &Mask = *MaskPtr;
    MachineBasicBlock &MBB = *Mask.getParent();
    if (Mask.getOpcode() != C166::AND32ri ||
        !Mask.getOperand(0).getReg().isVirtual() ||
        !Mask.getOperand(1).isReg() || !Mask.getOperand(2).isImm())
      continue;

    Register WideResult = Mask.getOperand(0).getReg();
    uint32_t Immediate = static_cast<uint32_t>(Mask.getOperand(2).getImm());
    uint16_t LowMask = static_cast<uint16_t>(Immediate);
    uint16_t HighMask = static_cast<uint16_t>(Immediate >> 16);
    if ((LowMask == 0) == (HighMask == 0))
      continue;

    SmallVector<MachineInstr *, 4> Tests;
    bool Safe = true;
    for (MachineOperand &Use : MRI.use_operands(WideResult)) {
      MachineInstr *User = Use.getParent();
      if (User->isDebugInstr() || User->getOpcode() != C166::TEST32BR ||
          User->getOperand(0).getReg() != WideResult ||
          User->getOperand(0).getSubReg() != sub_lo16 ||
          User->getOperand(1).getReg() != WideResult ||
          User->getOperand(1).getSubReg() != sub_hi16) {
        Safe = false;
        break;
      }
      if (!is_contained(Tests, User))
        Tests.push_back(User);
    }
    if (!Safe || Tests.empty())
      continue;

    unsigned WordSubReg = LowMask != 0 ? sub_lo16 : sub_hi16;
    uint16_t WordMask = LowMask != 0 ? LowMask : HighMask;
    Register NarrowResult = MRI.createVirtualRegister(&C166::GR16RegClass);
    if (WordMask == UINT16_MAX) {
      BuildMI(MBB, Mask, MIMetadata(Mask), TII.get(TargetOpcode::COPY),
              NarrowResult)
          .addReg(Mask.getOperand(1).getReg(), {}, WordSubReg);
    } else {
      BuildMI(MBB, Mask, MIMetadata(Mask),
              TII.get(isUInt<3>(WordMask) ? C166::ANDri3 : C166::ANDri16),
              NarrowResult)
          .addReg(Mask.getOperand(1).getReg(), {}, WordSubReg)
          .addImm(WordMask);
    }

    for (MachineInstr *Test : Tests) {
      BuildMI(*Test->getParent(), *Test, MIMetadata(*Test),
              TII.get(C166::CMPBRi))
          .addReg(NarrowResult)
          .addImm(0)
          .add(Test->getOperand(2))
          .add(Test->getOperand(3));
      Test->eraseFromParent();
    }
    MRI.clearKillFlags(NarrowResult);
    Mask.eraseFromParent();
    Changed = true;
  }
  return Changed;
}

class C166PostISel : public MachineFunctionPass {
public:
  static char ID;

  C166PostISel() : MachineFunctionPass(ID) {
    initializeC166PostISelPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "C166 post-selection optimizations";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineDominatorTreeWrapperPass>();
    AU.addPreserved<MachineDominatorTreeWrapperPass>();
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char C166PostISel::ID = 0;

INITIALIZE_PASS_BEGIN(C166PostISel, DEBUG_TYPE,
                      "C166 post-selection optimizations", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTreeWrapperPass)
INITIALIZE_PASS_END(C166PostISel, DEBUG_TYPE,
                    "C166 post-selection optimizations", false, false)

FunctionPass *llvm::createC166PostISelPass() { return new C166PostISel(); }

bool C166PostISel::runOnMachineFunction(MachineFunction &MF) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto &TII =
      *static_cast<const C166InstrInfo *>(MF.getSubtarget().getInstrInfo());
  MachineDominatorTree &MDT =
      getAnalysis<MachineDominatorTreeWrapperPass>().getDomTree();
  SmallVector<MachineInstr *, 8> DeadConstants;
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() != C166::FARADD32)
        continue;

      Register Offset = MI.getOperand(2).getReg();
      if (!Offset.isVirtual())
        continue;
      MachineInstr *Def = MRI.getUniqueVRegDef(Offset);
      if (!Def || (Def->getOpcode() != C166::MOVri4 &&
                   Def->getOpcode() != C166::MOVri16))
        continue;

      MI.setDesc(TII.get(C166::FARADD32i));
      MI.getOperand(2).ChangeToImmediate(Def->getOperand(1).getImm());
      if (MRI.use_empty(Offset))
        DeadConstants.push_back(Def);
      Changed = true;
    }
  }

  for (MachineInstr *MI : DeadConstants)
    if (MI->getParent())
      MI->eraseFromParent();

  Changed |= foldWideningMultiplyLibcalls(MF, TII);
  Changed |= foldWideningMultiplyAdds(MF, TII);
  Changed |= foldExtendedByteBranches(MF, TII, MDT);
  Changed |= rematerializeAddCarryBranches(MF, TII);
  Changed |= foldFrameMemoryAddresses(MF, TII);
  Changed |= foldIndirectALULoads(MF, TII);
  Changed |= foldShortImmediatePhis(MF, TII);
  Changed |= foldOffsetSignBranches(MF, TII);
  Changed |= narrowSingleWordTestMasks(MF, TII);
  Changed |= reuseDominatingImmediates(MF, MDT);
  Changed |= eliminateDeadInstructionsWithDiscardedFlags(MF);
  return Changed;
}
