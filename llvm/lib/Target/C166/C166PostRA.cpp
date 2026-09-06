//===-- C166PostRA.cpp - C166 post-register-allocation optimizations -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166InstrInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/IR/Instructions.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "c166-post-ra"

namespace {

static bool hasExplicitOrderedMemoryAccess(const MachineInstr &MI) {
  // This transform preserves every access and its order. Missing memory
  // operands are therefore safe; reject only accesses explicitly marked as
  // ordered.
  return !MI.memoperands_empty() && MI.hasOrderedMemoryRef();
}

static void copyPSWDefLiveness(MachineInstr &Destination,
                               const MachineInstr &Source,
                               const TargetRegisterInfo &TRI) {
  MachineOperand *NewDef = Destination.findRegisterDefOperand(C166::PSW, &TRI);
  const MachineOperand *OldDef = Source.findRegisterDefOperand(C166::PSW, &TRI);
  if (NewDef && OldDef)
    NewDef->setIsDead(OldDef->isDead());
}

static void markRegisterDefDead(MachineInstr &MI, Register Reg,
                                const TargetRegisterInfo &TRI) {
  if (MachineOperand *Def = MI.findRegisterDefOperand(Reg, &TRI))
    Def->setIsDead(true);
}

static MachineBasicBlock::iterator nextNonDebug(MachineBasicBlock &MBB,
                                                MachineBasicBlock::iterator I) {
  do {
    ++I;
  } while (I != MBB.end() && I->isDebugInstr());
  return I;
}

static MachineBasicBlock::iterator
previousNonDebug(MachineBasicBlock &MBB, MachineBasicBlock::iterator I) {
  while (I != MBB.begin()) {
    --I;
    if (!I->isDebugInstr())
      return I;
  }
  return MBB.end();
}

static bool isExtensionOpcode(unsigned Opcode) {
  return Opcode == C166::EXTPp || Opcode == C166::EXTPr ||
         Opcode == C166::EXTSr;
}

static bool isInsideExtensionWindow(const MachineBasicBlock &MBB,
                                    MachineBasicBlock::const_iterator I) {
  unsigned InstructionsSinceExtension = 0;
  while (I != MBB.begin() && InstructionsSinceExtension < 4) {
    --I;
    if (I->isDebugInstr() || I->isMetaInstruction())
      continue;
    if (isExtensionOpcode(I->getOpcode())) {
      const MachineOperand &Count =
          I->getOperand(I->getNumExplicitOperands() - 1);
      return Count.isImm() && InstructionsSinceExtension < Count.getImm();
    }
    ++InstructionsSinceExtension;
  }
  return false;
}

static constexpr MCPhysReg PostRAScratchCandidates[] = {
    C166::R1,  C166::R2,  C166::R3,  C166::R4,  C166::R5, C166::R10,
    C166::R11, C166::R12, C166::R13, C166::R14, C166::R15};

static Register findPostRAScratch(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator First,
                                  MachineBasicBlock::iterator Last,
                                  const TargetRegisterInfo &TRI) {
  for (MCRegister Candidate : PostRAScratchCandidates) {
    if (MBB.computeRegisterLiveness(&TRI, Candidate,
                                    MachineBasicBlock::const_iterator(First)) !=
        MachineBasicBlock::LQR_Dead)
      continue;

    bool Used = false;
    for (auto I = First;; ++I) {
      if (!I->isDebugInstr())
        Used |= llvm::any_of(I->operands(), [&](const MachineOperand &MO) {
          return MO.isReg() && MO.getReg() &&
                 TRI.regsOverlap(Candidate, MO.getReg());
        });
      if (I == Last)
        break;
    }
    if (!Used)
      return Candidate;
  }
  return Register();
}

static bool foldMaskedCarryShifts(MachineFunction &MF,
                                  const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Mask = *I++;
      if (Mask.getOpcode() != C166::ANDri3 || !Mask.getOperand(0).isReg() ||
          !Mask.getOperand(0).getReg().isPhysical() ||
          Mask.getOperand(0).getSubReg() || !Mask.getOperand(1).isReg() ||
          Mask.getOperand(1).getReg() != Mask.getOperand(0).getReg() ||
          Mask.getOperand(1).getSubReg() || !Mask.getOperand(2).isImm() ||
          Mask.getOperand(2).getImm() != 1)
        continue;

      auto Shift = nextNonDebug(MBB, Mask.getIterator());
      if (Shift == MBB.end() || Shift->getOpcode() != C166::SHRri4 ||
          !Shift->getOperand(0).isReg() || !Shift->getOperand(0).isDead() ||
          Shift->getOperand(0).getReg() != Mask.getOperand(0).getReg() ||
          Shift->getOperand(0).getSubReg() || !Shift->getOperand(1).isReg() ||
          Shift->getOperand(1).getReg() != Mask.getOperand(0).getReg() ||
          Shift->getOperand(1).getSubReg() || !Shift->getOperand(2).isImm() ||
          Shift->getOperand(2).getImm() != 1)
        continue;

      if (isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Mask.getIterator())) ||
          MBB.computeRegisterLiveness(
              &TRI, C166::PSW,
              MachineBasicBlock::const_iterator(std::next(Shift))) !=
              MachineBasicBlock::LQR_Dead)
        continue;

      Register Value = Mask.getOperand(0).getReg();
      MachineOperand Source = Mask.getOperand(1);
      MachineInstrBuilder Replacement =
          BuildMI(MBB, Mask, MIMetadata(Mask), TII.get(C166::SHRri4), Value)
              .add(Source)
              .addImm(1);
      Replacement->getOperand(0).setIsDead(true);
      copyPSWDefLiveness(*Replacement, *Shift, TRI);
      if (MachineOperand *NewCarry =
              Replacement->findRegisterDefOperand(C166::C, &TRI)) {
        if (const MachineOperand *OldCarry =
                Shift->findRegisterDefOperand(C166::C, &TRI))
          NewCarry->setIsDead(OldCarry->isDead());
      }
      Replacement->setFlags(Mask.getFlags() | Shift->getFlags());
      auto Resume = std::next(Shift);
      Mask.eraseFromParent();
      Shift->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static bool readsPhysicalRegister(const MachineInstr &MI, Register Reg,
                                  const TargetRegisterInfo &TRI) {
  return llvm::any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isUse() && MO.getReg() &&
           TRI.regsOverlap(MO.getReg(), Reg);
  });
}

static bool isWholePhysicalRegister(const MachineOperand &MO) {
  return MO.isReg() && MO.getReg().isPhysical() && !MO.getSubReg();
}

static bool formsGR32Pair(Register Low, Register High,
                          const TargetRegisterInfo &TRI) {
  MCRegister Pair = TRI.getMatchingSuperReg(Low, sub_lo16, &C166::GR32RegClass);
  return Pair && Register(TRI.getSubReg(Pair, sub_hi16)) == High;
}

static bool foldInvertedBitExtractions(MachineFunction &MF,
                                       const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Copy = *I++;
      if (Copy.getOpcode() != C166::MOVrr ||
          !isWholePhysicalRegister(Copy.getOperand(0)) ||
          !isWholePhysicalRegister(Copy.getOperand(1)) ||
          TRI.regsOverlap(Copy.getOperand(0).getReg(),
                          Copy.getOperand(1).getReg()) ||
          Copy.isBundledWithPred() || Copy.isBundledWithSucc() ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Copy.getIterator())))
        continue;

      Register Value = Copy.getOperand(0).getReg();
      Register Source = Copy.getOperand(1).getReg();
      auto Complement = nextNonDebug(MBB, Copy.getIterator());
      if (Complement == MBB.end() || Complement->getOpcode() != C166::CPL ||
          !isWholePhysicalRegister(Complement->getOperand(0)) ||
          Complement->getOperand(0).getReg() != Value ||
          !isWholePhysicalRegister(Complement->getOperand(1)) ||
          Complement->getOperand(1).getReg() != Value ||
          Complement->isBundledWithPred() || Complement->isBundledWithSucc())
        continue;

      auto Shift = nextNonDebug(MBB, Complement);
      if (Shift == MBB.end() || Shift->getOpcode() != C166::SHRri4 ||
          !isWholePhysicalRegister(Shift->getOperand(0)) ||
          Shift->getOperand(0).getReg() != Value ||
          !isWholePhysicalRegister(Shift->getOperand(1)) ||
          Shift->getOperand(1).getReg() != Value ||
          !Shift->getOperand(2).isImm() ||
          !isUInt<4>(Shift->getOperand(2).getImm()) ||
          Shift->isBundledWithPred() || Shift->isBundledWithSucc())
        continue;

      auto Mask = nextNonDebug(MBB, Shift);
      if (Mask == MBB.end() || Mask->getOpcode() != C166::ANDri3 ||
          !isWholePhysicalRegister(Mask->getOperand(0)) ||
          Mask->getOperand(0).getReg() != Value ||
          !isWholePhysicalRegister(Mask->getOperand(1)) ||
          Mask->getOperand(1).getReg() != Value ||
          !Mask->getOperand(2).isImm() || Mask->getOperand(2).getImm() != 1 ||
          Mask->isBundledWithPred() || Mask->isBundledWithSucc())
        continue;

      bool HasRelevantDebugUse = false;
      for (auto Debug = std::next(Copy.getIterator()); Debug != Mask; ++Debug)
        if (Debug->isDebugInstr() &&
            (readsPhysicalRegister(*Debug, Value, TRI) ||
             readsPhysicalRegister(*Debug, Source, TRI)))
          HasRelevantDebugUse = true;
      if (HasRelevantDebugUse)
        continue;

      auto AfterMask = std::next(Mask);
      auto IsDeadAfterMask = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterMask)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterMask(C166::PSW) || !IsDeadAfterMask(C166::C))
        continue;

      unsigned OldSize =
          TII.getInstSizeInBytes(Copy) + TII.getInstSizeInBytes(*Complement) +
          TII.getInstSizeInBytes(*Shift) + TII.getInstSizeInBytes(*Mask);
      unsigned NewSize =
          TII.get(C166::MOVri4).getSize() + TII.get(C166::BMOVNreg).getSize();
      if (NewSize >= OldSize)
        continue;

      MachineInstrBuilder Clear =
          BuildMI(MBB, Copy, MIMetadata(Copy), TII.get(C166::MOVri4), Value)
              .addImm(0);
      markRegisterDefDead(*Clear, C166::PSW, TRI);

      MachineOperand SourceOperand = Copy.getOperand(1);
      MachineInstrBuilder Move =
          BuildMI(MBB, Copy, MIMetadata(*Mask), TII.get(C166::BMOVNreg), Value)
              .addReg(Value, RegState::Kill)
              .add(SourceOperand)
              .addImm(0)
              .addImm(Shift->getOperand(2).getImm());
      Move->getOperand(0).setIsDead(Mask->getOperand(0).isDead());
      markRegisterDefDead(*Move, C166::PSW, TRI);
      markRegisterDefDead(*Move, C166::C, TRI);
      Move->setFlags(Copy.getFlags() | Complement->getFlags() |
                     Shift->getFlags() | Mask->getFlags());

      auto Resume = std::next(Mask);
      Copy.eraseFromParent();
      Complement->eraseFromParent();
      Shift->eraseFromParent();
      Mask->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static void copyFlagDefLiveness(MachineInstr &Destination,
                                const MachineInstr &Source,
                                const TargetRegisterInfo &TRI) {
  for (Register Flag : {Register(C166::PSW), Register(C166::C)}) {
    MachineOperand *NewDef = Destination.findRegisterDefOperand(Flag, &TRI);
    const MachineOperand *OldDef = Source.findRegisterDefOperand(Flag, &TRI);
    if (NewDef && OldDef)
      NewDef->setIsDead(OldDef->isDead());
  }
}

static bool foldSFRShuttles(MachineFunction &MF, const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Load = *I++;
      if (Load.getOpcode() != C166::MOVgsfr ||
          !isWholePhysicalRegister(Load.getOperand(0)) ||
          !isWholePhysicalRegister(Load.getOperand(1)) ||
          Load.isBundledWithPred() || Load.isBundledWithSucc())
        continue;

      Register Temporary = Load.getOperand(0).getReg();
      Register Source = Load.getOperand(1).getReg();
      auto AfterLoad = nextNonDebug(MBB, Load.getIterator());
      if (MBB.computeRegisterLiveness(
              &TRI, C166::PSW, MachineBasicBlock::const_iterator(AfterLoad)) !=
          MachineBasicBlock::LQR_Dead)
        continue;

      MachineBasicBlock::iterator Store = MBB.end();
      unsigned Distance = 0;
      for (auto Scan = AfterLoad; Scan != MBB.end() && Distance != 16; ++Scan) {
        if (Scan->isDebugInstr()) {
          if (readsPhysicalRegister(*Scan, Temporary, TRI))
            break;
          continue;
        }
        if (Scan->isMetaInstruction())
          continue;
        ++Distance;
        if (Scan->isCall() || Scan->isInlineAsm() || Scan->isTerminator() ||
            Scan->hasUnmodeledSideEffects() ||
            Scan->modifiesRegister(Source, &TRI) ||
            Scan->modifiesRegister(Temporary, &TRI))
          break;
        if (!readsPhysicalRegister(*Scan, Temporary, TRI))
          continue;
        if (Scan->getOpcode() == C166::MOVsfrg &&
            isWholePhysicalRegister(Scan->getOperand(0)) &&
            isWholePhysicalRegister(Scan->getOperand(1)) &&
            Scan->getOperand(1).getReg() == Temporary &&
            Scan->getOperand(0).getReg() != Source &&
            !Scan->isBundledWithPred() && !Scan->isBundledWithSucc())
          Store = Scan;
        break;
      }
      if (Store == MBB.end())
        continue;

      auto Resume = nextNonDebug(MBB, Store);
      if (MBB.computeRegisterLiveness(
              &TRI, Temporary, MachineBasicBlock::const_iterator(Resume)) !=
          MachineBasicBlock::LQR_Dead)
        continue;

      Register Destination = Store->getOperand(0).getReg();
      MachineInstrBuilder Direct =
          BuildMI(MBB, *Store, MIMetadata(*Store), TII.get(C166::MOVsfrsfr),
                  Destination)
              .addReg(Source);
      copyFlagDefLiveness(*Direct, *Store, TRI);
      Direct->setFlags(Load.getFlags() | Store->getFlags());
      Load.eraseFromParent();
      Store->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static std::optional<unsigned> getSFROperandOpcode(unsigned Opcode) {
  switch (Opcode) {
  case C166::ADDrr:
    return C166::ADDgsfr;
  case C166::SUBrr:
    return C166::SUBgsfr;
  case C166::CMPrr:
    return C166::CMPgsfr;
  case C166::XORrr:
    return C166::XORgsfr;
  case C166::ANDrr:
    return C166::ANDgsfr;
  case C166::ORrr:
    return C166::ORgsfr;
  default:
    return std::nullopt;
  }
}

static bool foldSFROperands(MachineFunction &MF, const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Load = *I++;
      if (Load.getOpcode() != C166::MOVgsfr ||
          !isWholePhysicalRegister(Load.getOperand(0)) ||
          !isWholePhysicalRegister(Load.getOperand(1)) ||
          Load.isBundledWithPred() || Load.isBundledWithSucc())
        continue;

      Register Temporary = Load.getOperand(0).getReg();
      auto Consumer = nextNonDebug(MBB, Load.getIterator());
      if (Consumer == MBB.end() || Consumer->isBundledWithPred() ||
          Consumer->isBundledWithSucc())
        continue;

      std::optional<unsigned> DirectOpcode =
          getSFROperandOpcode(Consumer->getOpcode());
      if (!DirectOpcode)
        continue;

      unsigned SourceOperand = Consumer->getOpcode() == C166::CMPrr ? 1 : 2;
      if (!isWholePhysicalRegister(Consumer->getOperand(SourceOperand)) ||
          Consumer->getOperand(SourceOperand).getReg() != Temporary)
        continue;

      bool HasDebugUse = false;
      for (auto Debug = std::next(Load.getIterator()); Debug != Consumer;
           ++Debug)
        HasDebugUse |= Debug->isDebugInstr() &&
                       readsPhysicalRegister(*Debug, Temporary, TRI);
      if (HasDebugUse)
        continue;

      auto Resume = std::next(Consumer);
      if (MBB.computeRegisterLiveness(
              &TRI, Temporary, MachineBasicBlock::const_iterator(Resume)) !=
          MachineBasicBlock::LQR_Dead)
        continue;

      MachineInstrBuilder Direct = BuildMI(
          MBB, *Consumer, MIMetadata(*Consumer), TII.get(*DirectOpcode));
      for (unsigned Operand = 0; Operand != SourceOperand; ++Operand)
        Direct.add(Consumer->getOperand(Operand));
      Direct.add(Load.getOperand(1));
      copyFlagDefLiveness(*Direct, *Consumer, TRI);
      Direct->setFlags(Load.getFlags() | Consumer->getFlags());

      Load.eraseFromParent();
      Consumer->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static bool foldZeroExtendedWideAdds(MachineFunction &MF,
                                     const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Add = *I++;
      if (Add.getOpcode() != C166::ADDrr || !Add.getOperand(0).isReg() ||
          !Add.getOperand(1).isReg() || !Add.getOperand(2).isReg())
        continue;

      auto AddCarry = nextNonDebug(MBB, Add.getIterator());
      if (AddCarry == MBB.end() || AddCarry->getOpcode() != C166::ADDCrr ||
          !AddCarry->getOperand(0).isReg() ||
          !AddCarry->getOperand(1).isReg() || !AddCarry->getOperand(2).isReg())
        continue;

      Register DstLow = Add.getOperand(0).getReg();
      Register DstHigh = AddCarry->getOperand(0).getReg();
      Register WordLow = Add.getOperand(2).getReg();
      Register WordHigh = AddCarry->getOperand(2).getReg();
      if (!DstLow.isPhysical() || !DstHigh.isPhysical() ||
          !WordLow.isPhysical() || !WordHigh.isPhysical() ||
          Add.getOperand(1).getReg() != DstLow ||
          AddCarry->getOperand(1).getReg() != DstHigh ||
          !formsGR32Pair(DstLow, DstHigh, TRI) ||
          !formsGR32Pair(WordLow, WordHigh, TRI) ||
          TRI.regsOverlap(DstLow, WordLow) ||
          TRI.regsOverlap(DstHigh, WordHigh))
        continue;

      MachineInstr *LowCopy = nullptr;
      MachineInstr *HighZero = nullptr;
      for (auto Scan = Add.getIterator();
           Scan != MBB.begin() && (!LowCopy || !HighZero);) {
        --Scan;
        if (Scan->isDebugInstr() || Scan->isMetaInstruction())
          continue;

        bool ModifiesLow = Scan->modifiesRegister(WordLow, &TRI);
        bool ModifiesHigh = Scan->modifiesRegister(WordHigh, &TRI);
        bool ReadsLow = Scan->readsRegister(WordLow, &TRI);
        bool ReadsHigh = Scan->readsRegister(WordHigh, &TRI);

        if (ModifiesLow && !LowCopy) {
          if (Scan->getOpcode() != C166::MOVrr ||
              Scan->getOperand(0).getReg() != WordLow ||
              !Scan->getOperand(1).isReg() ||
              !Scan->getOperand(1).getReg().isPhysical())
            break;
          LowCopy = &*Scan;
          ReadsHigh = false;
        } else if (ModifiesLow) {
          break;
        }

        if (ModifiesHigh && !HighZero) {
          bool IsZero = (Scan->getOpcode() == C166::MOVri4 ||
                         Scan->getOpcode() == C166::MOVri16) &&
                        Scan->getOperand(0).getReg() == WordHigh &&
                        Scan->getOperand(1).isImm() &&
                        Scan->getOperand(1).getImm() == 0;
          if (!IsZero)
            break;
          HighZero = &*Scan;
        } else if (ModifiesHigh) {
          break;
        }

        if ((ReadsLow && &*Scan != LowCopy) || (ReadsHigh && &*Scan != LowCopy))
          break;
      }
      if (!LowCopy || !HighZero)
        continue;

      Register Source = LowCopy->getOperand(1).getReg();
      if (TRI.regsOverlap(Source, DstLow) || TRI.regsOverlap(Source, DstHigh))
        continue;

      bool InvalidInterveningInstruction = false;
      for (auto Scan = std::next(LowCopy->getIterator());
           Scan != Add.getIterator(); ++Scan) {
        if (Scan->isDebugInstr() || Scan->isMetaInstruction() ||
            &*Scan == HighZero)
          continue;
        InvalidInterveningInstruction |= Scan->modifiesRegister(Source, &TRI) ||
                                         Scan->readsRegister(C166::PSW, &TRI) ||
                                         Scan->readsRegister(C166::C, &TRI);
      }
      auto AfterAddCarry = nextNonDebug(MBB, AddCarry);
      auto IsDeadAfter = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg,
                   MachineBasicBlock::const_iterator(AfterAddCarry)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (InvalidInterveningInstruction || !IsDeadAfter(WordLow) ||
          !IsDeadAfter(WordHigh))
        continue;

      MachineInstrBuilder Low =
          BuildMI(MBB, Add, MIMetadata(Add), TII.get(C166::ADDrr), DstLow)
              .addReg(DstLow, RegState::Kill)
              .addReg(Source, RegState::Kill);
      Low->getOperand(0).setIsDead(Add.getOperand(0).isDead());
      copyFlagDefLiveness(*Low, Add, TRI);
      Low->setFlags(Add.getFlags());

      MachineInstrBuilder High = BuildMI(MBB, Add, MIMetadata(*AddCarry),
                                         TII.get(C166::ADDCri3), DstHigh)
                                     .addReg(DstHigh, RegState::Kill)
                                     .addImm(0);
      High->getOperand(0).setIsDead(AddCarry->getOperand(0).isDead());
      copyFlagDefLiveness(*High, *AddCarry, TRI);
      High->setFlags(AddCarry->getFlags());

      LowCopy->eraseFromParent();
      HighZero->eraseFromParent();
      AddCarry->eraseFromParent();
      Add.eraseFromParent();
      Changed = true;
      I = AfterAddCarry;
    }
  }

  return Changed;
}

static bool hasDebugUseBeforeOverwrite(const MachineInstr &MI, Register Reg,
                                       const TargetRegisterInfo &TRI) {
  enum class ScanResult { DebugUse, Overwritten, ReachesEnd };
  auto Scan = [&](MachineBasicBlock::const_iterator Begin,
                  MachineBasicBlock::const_iterator End) {
    for (auto I = Begin; I != End; ++I) {
      if (I->isDebugInstr()) {
        if (readsPhysicalRegister(*I, Reg, TRI))
          return ScanResult::DebugUse;
        continue;
      }
      if (I->modifiesRegister(Reg, &TRI))
        return ScanResult::Overwritten;
    }
    return ScanResult::ReachesEnd;
  };

  const MachineBasicBlock &MBB = *MI.getParent();
  ScanResult Initial = Scan(std::next(MI.getIterator()), MBB.end());
  if (Initial == ScanResult::DebugUse)
    return true;
  if (Initial == ScanResult::Overwritten)
    return false;

  SmallPtrSet<const MachineBasicBlock *, 8> Visited;
  SmallVector<const MachineBasicBlock *, 8> Worklist(MBB.successors());
  while (!Worklist.empty()) {
    const MachineBasicBlock &Successor = *Worklist.pop_back_val();
    if (!Visited.insert(&Successor).second)
      continue;

    ScanResult Result = Scan(Successor.begin(), Successor.end());
    if (Result == ScanResult::DebugUse)
      return true;
    if (Result == ScanResult::ReachesEnd)
      llvm::append_range(Worklist, Successor.successors());
  }
  return false;
}

static bool isUnusedBeforeOverwrite(const MachineInstr &MI, Register Reg,
                                    const TargetRegisterInfo &TRI,
                                    Register ExplicitHighHalf = Register()) {
  MCRegister Pair = 0;
  if (ExplicitHighHalf) {
    Pair = TRI.getMatchingSuperReg(Reg, sub_lo16, &C166::GR32RegClass);
    if (!Pair || Register(TRI.getSubReg(Pair, sub_hi16)) != ExplicitHighHalf)
      return false;
  }

  auto ReadsValue = [&](const MachineInstr &Instruction) {
    // Subregister uses can retain an implicit superregister kill after
    // rewriting.  An explicit high-half use does not read the low half.
    bool HasExplicitHighUse =
        ExplicitHighHalf &&
        llvm::any_of(Instruction.operands(), [&](const auto &MO) {
          return MO.isReg() && MO.isUse() && !MO.isImplicit() &&
                 MO.getReg() == ExplicitHighHalf && !MO.getSubReg();
        });
    return llvm::any_of(Instruction.operands(), [&](const MachineOperand &MO) {
      if (!MO.isReg() || !MO.isUse() || !MO.getReg() ||
          !TRI.regsOverlap(MO.getReg(), Reg))
        return false;
      return !HasExplicitHighUse || !MO.isImplicit() || MO.getReg() != Pair;
    });
  };

  enum class ScanResult { Used, Overwritten, ReachesEnd };
  auto Scan = [&](MachineBasicBlock::const_iterator Begin,
                  MachineBasicBlock::const_iterator End) {
    for (auto I = Begin; I != End; ++I) {
      if (ReadsValue(*I))
        return ScanResult::Used;
      if (!I->isDebugInstr() && !I->isMetaInstruction() &&
          I->modifiesRegister(Reg, &TRI))
        return ScanResult::Overwritten;
    }
    return ScanResult::ReachesEnd;
  };

  const MachineBasicBlock &MBB = *MI.getParent();
  ScanResult Initial = Scan(std::next(MI.getIterator()), MBB.end());
  if (Initial == ScanResult::Used)
    return false;
  if (Initial == ScanResult::Overwritten)
    return true;

  SmallPtrSet<const MachineBasicBlock *, 8> Visited;
  SmallVector<const MachineBasicBlock *, 8> Worklist(MBB.successors());
  while (!Worklist.empty()) {
    const MachineBasicBlock &Successor = *Worklist.pop_back_val();
    if (!Visited.insert(&Successor).second)
      continue;

    ScanResult Result = Scan(Successor.begin(), Successor.end());
    if (Result == ScanResult::Used)
      return false;
    if (Result == ScanResult::ReachesEnd)
      llvm::append_range(Worklist, Successor.successors());
  }
  return true;
}

static bool removeDeadPhysicalLoads(MachineFunction &MF,
                                    const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Load = *I++;
      if ((Load.getOpcode() != C166::MOVrm &&
           Load.getOpcode() != C166::MOVrm16) ||
          hasExplicitOrderedMemoryAccess(Load) ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Load.getIterator())) ||
          !Load.getOperand(0).isReg() ||
          !Load.getOperand(0).getReg().isPhysical() ||
          Load.getOperand(0).getSubReg())
        continue;

      Register Value = Load.getOperand(0).getReg();
      auto AfterLoad = nextNonDebug(MBB, Load.getIterator());
      auto IsDeadOrOverwritten = [&](Register Reg) {
        if (TII.isRegisterOverwrittenBeforeUse(Load, Reg))
          return true;
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterLoad)) ==
                   MachineBasicBlock::LQR_Dead &&
               !hasDebugUseBeforeOverwrite(Load, Reg, TRI);
      };
      if (!IsDeadOrOverwritten(Value) || !IsDeadOrOverwritten(C166::PSW))
        continue;

      Load.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

static bool foldIndirectMemoryCopies(MachineFunction &MF,
                                     const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Load = *I++;
      bool IsByte = Load.getOpcode() == C166::MOVBrm ||
                    Load.getOpcode() == C166::MOVBrmPostInc;
      bool SourcePostIncrement = Load.getOpcode() == C166::MOVrmPostInc ||
                                 Load.getOpcode() == C166::MOVBrmPostInc;
      if ((!SourcePostIncrement && Load.getOpcode() != C166::MOVrm &&
           !IsByte) ||
          hasExplicitOrderedMemoryAccess(Load) ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Load.getIterator())))
        continue;

      unsigned SourceBaseOperand = SourcePostIncrement ? 2 : 1;
      if (!Load.getOperand(0).isReg() ||
          !Load.getOperand(0).getReg().isPhysical() ||
          Load.getOperand(0).getSubReg() ||
          !Load.getOperand(SourceBaseOperand).isReg() ||
          !Load.getOperand(SourceBaseOperand).getReg().isPhysical() ||
          Load.getOperand(SourceBaseOperand).getSubReg())
        continue;

      auto Store = nextNonDebug(MBB, Load.getIterator());
      MachineInstr *Extension = nullptr;
      Register ExtendedWord;
      if (IsByte && Store != MBB.end() &&
          (Store->getOpcode() == C166::MOVBZrr ||
           Store->getOpcode() == C166::MOVBSrr) &&
          Store->getOperand(0).isReg() &&
          Store->getOperand(0).getReg().isPhysical() &&
          !Store->getOperand(0).getSubReg() && Store->getOperand(1).isReg() &&
          Store->getOperand(1).getReg() == Load.getOperand(0).getReg() &&
          !Store->getOperand(1).getSubReg()) {
        Extension = &*Store;
        ExtendedWord = Store->getOperand(0).getReg();
        Store = nextNonDebug(MBB, Store);
      }
      unsigned StoreOpcode = IsByte ? C166::MOVBmr : C166::MOVmr;
      if (Store == MBB.end() || Store->getOpcode() != StoreOpcode ||
          hasExplicitOrderedMemoryAccess(*Store) ||
          !Store->getOperand(0).isReg() ||
          !Store->getOperand(0).getReg().isPhysical() ||
          Store->getOperand(0).getSubReg() || !Store->getOperand(1).isReg() ||
          Store->getOperand(1).getReg() != Load.getOperand(0).getReg() ||
          Store->getOperand(1).getSubReg())
        continue;

      Register Value = Load.getOperand(0).getReg();
      Register SourceBase = Load.getOperand(SourceBaseOperand).getReg();
      Register DestinationBase = Store->getOperand(0).getReg();
      if (TRI.regsOverlap(Value, DestinationBase) ||
          (SourcePostIncrement && TRI.regsOverlap(Value, SourceBase)) ||
          (Extension && TRI.regsOverlap(ExtendedWord, DestinationBase)) ||
          (SourcePostIncrement && TRI.regsOverlap(SourceBase, DestinationBase)))
        continue;

      bool DebugUse = false;
      for (auto Debug = std::next(Load.getIterator()); Debug != Store; ++Debug)
        DebugUse |=
            Debug->isDebugInstr() &&
            (readsPhysicalRegister(*Debug, Value, TRI) ||
             readsPhysicalRegister(*Debug, SourceBase, TRI) ||
             readsPhysicalRegister(*Debug, DestinationBase, TRI) ||
             (Extension && readsPhysicalRegister(*Debug, ExtendedWord, TRI)));
      if (DebugUse)
        continue;

      auto AfterStore = nextNonDebug(MBB, Store);
      if (MBB.computeRegisterLiveness(
              &TRI, Value, MachineBasicBlock::const_iterator(AfterStore)) !=
              MachineBasicBlock::LQR_Dead ||
          (Extension && MBB.computeRegisterLiveness(
                            &TRI, ExtendedWord,
                            MachineBasicBlock::const_iterator(AfterStore)) !=
                            MachineBasicBlock::LQR_Dead))
        continue;

      MachineInstr *Increment = nullptr;
      Register IncrementedBase;
      if (!SourcePostIncrement && std::next(Store) == AfterStore &&
          AfterStore != MBB.end() &&
          (AfterStore->getOpcode() == C166::ADDri3 ||
           AfterStore->getOpcode() == C166::ADDri16) &&
          AfterStore->getNumExplicitOperands() == 3 &&
          AfterStore->getOperand(0).isReg() &&
          AfterStore->getOperand(0).getReg().isPhysical() &&
          !AfterStore->getOperand(0).getSubReg() &&
          AfterStore->getOperand(1).isReg() &&
          AfterStore->getOperand(1).getReg() ==
              AfterStore->getOperand(0).getReg() &&
          !AfterStore->getOperand(1).getSubReg() &&
          AfterStore->getOperand(2).isImm() &&
          AfterStore->getOperand(2).getImm() == (IsByte ? 1 : 2) &&
          !isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(AfterStore))) {
        IncrementedBase = AfterStore->getOperand(0).getReg();
        auto AfterIncrement = nextNonDebug(MBB, AfterStore);
        auto IsDeadOrOverwritten = [&](Register Reg) {
          return MBB.computeRegisterLiveness(
                     &TRI, Reg,
                     MachineBasicBlock::const_iterator(AfterIncrement)) ==
                     MachineBasicBlock::LQR_Dead ||
                 TII.isRegisterOverwrittenBeforeUse(*AfterStore, Reg);
        };
        if ((IncrementedBase == SourceBase ||
             IncrementedBase == DestinationBase) &&
            IsDeadOrOverwritten(C166::PSW) && IsDeadOrOverwritten(C166::C))
          Increment = &*AfterStore;
      }

      MachineInstrBuilder Copy;
      if (SourcePostIncrement) {
        Copy = BuildMI(MBB, Load, MIMetadata(Load),
                       TII.get(IsByte ? C166::MOVBmmSrcPostInc
                                      : C166::MOVmmSrcPostInc),
                       SourceBase)
                   .add(Store->getOperand(0))
                   .add(Load.getOperand(SourceBaseOperand));
      } else if (Increment && IncrementedBase == SourceBase) {
        Copy = BuildMI(MBB, Load, MIMetadata(Load),
                       TII.get(IsByte ? C166::MOVBmmSrcPostInc
                                      : C166::MOVmmSrcPostInc),
                       SourceBase)
                   .add(Store->getOperand(0))
                   .addReg(SourceBase, RegState::Kill);
        Copy->getOperand(0).setIsDead(Increment->getOperand(0).isDead());
      } else if (Increment) {
        Copy = BuildMI(MBB, Load, MIMetadata(Load),
                       TII.get(IsByte ? C166::MOVBmmDstPostInc
                                      : C166::MOVmmDstPostInc),
                       DestinationBase)
                   .addReg(DestinationBase, RegState::Kill)
                   .add(Load.getOperand(SourceBaseOperand));
        Copy->getOperand(0).setIsDead(Increment->getOperand(0).isDead());
      } else {
        Copy = BuildMI(MBB, Load, MIMetadata(Load),
                       TII.get(IsByte ? C166::MOVBmm : C166::MOVmm))
                   .add(Store->getOperand(0))
                   .add(Load.getOperand(SourceBaseOperand));
      }
      SmallVector<MachineMemOperand *, 2> MemoryOperands;
      llvm::append_range(MemoryOperands, Load.memoperands());
      llvm::append_range(MemoryOperands, Store->memoperands());
      Copy->setMemRefs(MF, MemoryOperands);
      copyPSWDefLiveness(*Copy, Increment ? *Increment : *Store, TRI);
      Copy->setFlags(Load.getFlags() | Store->getFlags() |
                     (Extension ? Extension->getFlags() : 0) |
                     (Increment ? Increment->getFlags() : 0));

      auto Resume =
          Increment ? std::next(Increment->getIterator()) : std::next(Store);
      Load.eraseFromParent();
      if (Extension)
        Extension->eraseFromParent();
      Store->eraseFromParent();
      if (Increment)
        Increment->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static bool hasDebugUseBeforeFullDef(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator Start,
                                     Register Word,
                                     const TargetRegisterInfo &TRI) {
  for (auto I = Start; I != MBB.end(); ++I) {
    if (I->isDebugInstr()) {
      if (readsPhysicalRegister(*I, Word, TRI))
        return true;
      continue;
    }
    if (I->isMetaInstruction())
      continue;
    if (llvm::any_of(I->operands(), [&](const MachineOperand &MO) {
          return MO.isReg() && MO.isDef() && MO.getReg() == Word &&
                 !MO.getSubReg();
        }))
      return false;
  }
  return false;
}

static bool foldZeroExtendedByteComparisons(MachineFunction &MF,
                                            const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Compare = *I++;
      if (Compare.getOpcode() != C166::CMPrr ||
          !Compare.getOperand(0).isReg() ||
          !Compare.getOperand(0).getReg().isPhysical() ||
          Compare.getOperand(0).getSubReg() ||
          !Compare.getOperand(0).isKill() || !Compare.getOperand(1).isReg() ||
          !Compare.getOperand(1).getReg().isPhysical() ||
          Compare.getOperand(1).getSubReg() || !Compare.getOperand(1).isKill())
        continue;

      auto SecondExtension = previousNonDebug(MBB, Compare.getIterator());
      if (SecondExtension == MBB.end())
        continue;
      auto FirstExtension = previousNonDebug(MBB, SecondExtension);
      if (FirstExtension == MBB.end() ||
          FirstExtension->getOpcode() != C166::MOVBZrr ||
          SecondExtension->getOpcode() != C166::MOVBZrr ||
          isInsideExtensionWindow(MBB, FirstExtension))
        continue;

      Register LeftWord = Compare.getOperand(0).getReg();
      Register RightWord = Compare.getOperand(1).getReg();
      if (TRI.regsOverlap(LeftWord, RightWord))
        continue;

      MachineInstr *LeftExtension = nullptr;
      MachineInstr *RightExtension = nullptr;
      for (MachineInstr *Extension : {&*FirstExtension, &*SecondExtension}) {
        if (!Extension->getOperand(0).isReg() ||
            !Extension->getOperand(0).getReg().isPhysical() ||
            Extension->getOperand(0).getSubReg() ||
            !Extension->getOperand(1).isReg() ||
            !Extension->getOperand(1).getReg().isPhysical() ||
            Extension->getOperand(1).getSubReg()) {
          LeftExtension = nullptr;
          RightExtension = nullptr;
          break;
        }
        Register Destination = Extension->getOperand(0).getReg();
        Register Source = Extension->getOperand(1).getReg();
        if (Register(TRI.getSubReg(Destination, sub_lo8)) != Source) {
          LeftExtension = nullptr;
          RightExtension = nullptr;
          break;
        }
        if (Destination == LeftWord)
          LeftExtension = Extension;
        else if (Destination == RightWord)
          RightExtension = Extension;
      }
      if (!LeftExtension || !RightExtension)
        continue;

      bool DebugUse = false;
      for (auto Scan = FirstExtension; Scan != Compare.getIterator(); ++Scan)
        DebugUse |= Scan->isDebugInstr() &&
                    (readsPhysicalRegister(*Scan, LeftWord, TRI) ||
                     readsPhysicalRegister(*Scan, RightWord, TRI));
      auto AfterCompare = nextNonDebug(MBB, Compare.getIterator());
      bool PSWIsDead = MBB.computeRegisterLiveness(
                           &TRI, C166::PSW,
                           MachineBasicBlock::const_iterator(AfterCompare)) ==
                       MachineBasicBlock::LQR_Dead;
      if (DebugUse || !PSWIsDead ||
          hasDebugUseBeforeFullDef(MBB, AfterCompare, LeftWord, TRI) ||
          hasDebugUseBeforeFullDef(MBB, AfterCompare, RightWord, TRI))
        continue;

      Register LeftByte = LeftExtension->getOperand(1).getReg();
      Register RightByte = RightExtension->getOperand(1).getReg();
      MachineInstrBuilder Replacement =
          BuildMI(MBB, Compare, MIMetadata(Compare), TII.get(C166::CMPBrr))
              .addReg(LeftByte, RegState::Kill)
              .addReg(RightByte, RegState::Kill);
      markRegisterDefDead(*Replacement, C166::PSW, TRI);
      if (MBB.computeRegisterLiveness(
              &TRI, C166::C, MachineBasicBlock::const_iterator(AfterCompare)) ==
          MachineBasicBlock::LQR_Dead)
        markRegisterDefDead(*Replacement, C166::C, TRI);
      Replacement->setFlags(FirstExtension->getFlags() |
                            SecondExtension->getFlags() | Compare.getFlags());

      FirstExtension->eraseFromParent();
      SecondExtension->eraseFromParent();
      Compare.eraseFromParent();
      Changed = true;
      I = AfterCompare;
    }
  }

  return Changed;
}

static bool foldZeroExtendedByteImmediateComparisons(MachineFunction &MF,
                                                     const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Compare = *I++;
      if ((Compare.getOpcode() != C166::CMPri3 &&
           Compare.getOpcode() != C166::CMPri16) ||
          !Compare.getOperand(0).isReg() ||
          !Compare.getOperand(0).getReg().isPhysical() ||
          Compare.getOperand(0).getSubReg() ||
          !Compare.getOperand(0).isKill() || !Compare.getOperand(1).isImm() ||
          !isUInt<8>(Compare.getOperand(1).getImm()))
        continue;

      auto Extension = previousNonDebug(MBB, Compare.getIterator());
      if (Extension == MBB.end() || Extension->getOpcode() != C166::MOVBZrr ||
          !Extension->getOperand(0).isReg() ||
          !Extension->getOperand(0).getReg().isPhysical() ||
          Extension->getOperand(0).getSubReg() ||
          Extension->getOperand(0).getReg() != Compare.getOperand(0).getReg() ||
          !Extension->getOperand(1).isReg() ||
          !Extension->getOperand(1).getReg().isPhysical() ||
          Extension->getOperand(1).getSubReg() ||
          isInsideExtensionWindow(MBB, Extension))
        continue;

      Register Word = Extension->getOperand(0).getReg();
      Register Byte = Extension->getOperand(1).getReg();
      if (Register(TRI.getSubReg(Word, sub_lo8)) != Byte)
        continue;

      bool DebugUse = false;
      for (auto Scan = Extension; Scan != Compare.getIterator(); ++Scan)
        DebugUse |=
            Scan->isDebugInstr() && readsPhysicalRegister(*Scan, Word, TRI);
      auto AfterCompare = nextNonDebug(MBB, Compare.getIterator());
      auto IsDeadAfter = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg,
                   MachineBasicBlock::const_iterator(AfterCompare)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (DebugUse || !IsDeadAfter(C166::PSW) ||
          hasDebugUseBeforeFullDef(MBB, AfterCompare, Word, TRI))
        continue;

      unsigned ByteOpcode = isUInt<3>(Compare.getOperand(1).getImm())
                                ? C166::CMPBri3
                                : C166::CMPBri8;
      MachineInstrBuilder Replacement =
          BuildMI(MBB, Compare, MIMetadata(Compare), TII.get(ByteOpcode))
              .addReg(Byte, RegState::Kill)
              .addImm(Compare.getOperand(1).getImm());
      markRegisterDefDead(*Replacement, C166::PSW, TRI);
      if (IsDeadAfter(C166::C))
        markRegisterDefDead(*Replacement, C166::C, TRI);
      Replacement->setFlags(Extension->getFlags() | Compare.getFlags());

      Extension->eraseFromParent();
      Compare.eraseFromParent();
      Changed = true;
      I = AfterCompare;
    }
  }

  return Changed;
}

static bool onlyLowByteIsUsedUntilFullDef(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator Start,
                                          Register Word, Register LowByte,
                                          const TargetRegisterInfo &TRI) {
  for (auto I = Start; I != MBB.end(); ++I) {
    for (const MachineOperand &MO : I->operands()) {
      if (!MO.isReg() || !MO.isUse() || !MO.getReg() ||
          !TRI.regsOverlap(MO.getReg(), Word))
        continue;
      if (I->isDebugInstr() || MO.getReg() != LowByte || MO.getSubReg())
        return false;
    }
    if (I->isMetaInstruction())
      continue;
    if (llvm::any_of(I->operands(), [&](const MachineOperand &MO) {
          return MO.isReg() && MO.isDef() && MO.getReg() == Word &&
                 !MO.getSubReg();
        }))
      return true;
  }

  return MBB.computeRegisterLiveness(
             &TRI, Word, MachineBasicBlock::const_iterator(MBB.end())) ==
         MachineBasicBlock::LQR_Dead;
}

static bool removeDeadByteExtensions(MachineFunction &MF,
                                     const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Extension = *I++;
      if ((Extension.getOpcode() != C166::MOVBZrr &&
           Extension.getOpcode() != C166::MOVBSrr) ||
          !Extension.getOperand(0).isReg() ||
          !Extension.getOperand(0).getReg().isPhysical() ||
          Extension.getOperand(0).getSubReg() ||
          !Extension.getOperand(1).isReg() ||
          !Extension.getOperand(1).getReg().isPhysical() ||
          Extension.getOperand(1).getSubReg() ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Extension.getIterator())))
        continue;

      Register Word = Extension.getOperand(0).getReg();
      Register LowByte = Extension.getOperand(1).getReg();
      if (Register(TRI.getSubReg(Word, sub_lo8)) != LowByte)
        continue;

      auto Next = nextNonDebug(MBB, Extension.getIterator());
      bool FlagsAreDead =
          MBB.computeRegisterLiveness(
              &TRI, C166::PSW, MachineBasicBlock::const_iterator(Next)) ==
              MachineBasicBlock::LQR_Dead ||
          TII.isRegisterOverwrittenBeforeUse(Extension, C166::PSW);
      if (!FlagsAreDead ||
          !onlyLowByteIsUsedUntilFullDef(MBB, Next, Word, LowByte, TRI))
        continue;

      Extension.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

static MachineInstr *
findRedundantByteExtension(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator Before, Register Word,
                           Register LowByte, const TargetRegisterInfo &TRI) {
  for (auto I = Before; I != MBB.begin();) {
    --I;
    if (I->isDebugInstr()) {
      if (readsPhysicalRegister(*I, Word, TRI))
        return nullptr;
      continue;
    }
    if (I->isMetaInstruction())
      continue;
    if (I->readsRegister(C166::PSW, &TRI) || I->readsRegister(C166::C, &TRI))
      return nullptr;
    if (!I->modifiesRegister(Word, &TRI)) {
      if (I->readsRegister(Word, &TRI))
        return nullptr;
      continue;
    }
    if (I->getOpcode() != C166::MOVBZrr || !I->getOperand(0).isReg() ||
        I->getOperand(0).getReg() != Word || I->getOperand(0).getSubReg() ||
        !I->getOperand(1).isReg() || I->getOperand(1).getReg() != LowByte ||
        I->getOperand(1).getSubReg() || isInsideExtensionWindow(MBB, I))
      return nullptr;
    return &*I;
  }
  return nullptr;
}

static bool narrowDeadByteALU(MachineFunction &MF, const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Extension = *I++;
      if (Extension.getOpcode() != C166::MOVBZrr ||
          !Extension.getOperand(0).isReg() ||
          !Extension.getOperand(0).getReg().isPhysical() ||
          Extension.getOperand(0).getSubReg() ||
          !Extension.getOperand(1).isReg() ||
          !Extension.getOperand(1).getReg().isPhysical() ||
          Extension.getOperand(1).getSubReg() ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Extension.getIterator())))
        continue;

      Register Word = Extension.getOperand(0).getReg();
      Register LowByte = Extension.getOperand(1).getReg();
      if (Register(TRI.getSubReg(Word, sub_lo8)) != LowByte)
        continue;

      auto ALU = nextNonDebug(MBB, Extension.getIterator());
      if (ALU == MBB.end() || ALU->getNumExplicitOperands() < 2 ||
          !ALU->getOperand(0).isReg() || ALU->getOperand(0).getReg() != Word ||
          ALU->getOperand(0).getSubReg() || !ALU->getOperand(1).isReg() ||
          ALU->getOperand(1).getReg() != Word || ALU->getOperand(1).getSubReg())
        continue;

      unsigned ByteOpcode;
      unsigned SmallImmediateOpcode = 0;
      unsigned FullImmediateOpcode = 0;
      uint8_t ByteImmediate = 0;
      bool HasImmediate = false;
      bool HasRegister = false;
      switch (ALU->getOpcode()) {
      case C166::ADDrr:
        ByteOpcode = C166::ADDBrr;
        HasRegister = true;
        break;
      case C166::ADDri3:
      case C166::ADDri16:
        SmallImmediateOpcode = C166::ADDBri3;
        FullImmediateOpcode = C166::ADDBri8;
        HasImmediate = true;
        break;
      case C166::SUBrr:
        ByteOpcode = C166::SUBBrr;
        HasRegister = true;
        break;
      case C166::SUBri3:
      case C166::SUBri16:
        SmallImmediateOpcode = C166::SUBBri3;
        FullImmediateOpcode = C166::SUBBri8;
        HasImmediate = true;
        break;
      case C166::XORrr:
        ByteOpcode = C166::XORBrr;
        HasRegister = true;
        break;
      case C166::XORri3:
      case C166::XORri16:
        SmallImmediateOpcode = C166::XORBri3;
        FullImmediateOpcode = C166::XORBri8;
        HasImmediate = true;
        break;
      case C166::ANDrr:
        ByteOpcode = C166::ANDBrr;
        HasRegister = true;
        break;
      case C166::ANDri3:
      case C166::ANDri16:
        SmallImmediateOpcode = C166::ANDBri3;
        FullImmediateOpcode = C166::ANDBri8;
        HasImmediate = true;
        break;
      case C166::ORrr:
        ByteOpcode = C166::ORBrr;
        HasRegister = true;
        break;
      case C166::ORri3:
      case C166::ORri16:
        SmallImmediateOpcode = C166::ORBri3;
        FullImmediateOpcode = C166::ORBri8;
        HasImmediate = true;
        break;
      case C166::NEG:
        ByteOpcode = C166::NEGB;
        break;
      case C166::CPL:
        ByteOpcode = C166::CPLB;
        break;
      default:
        continue;
      }
      if (HasImmediate &&
          (ALU->getNumExplicitOperands() != 3 || !ALU->getOperand(2).isImm()))
        continue;
      if (HasImmediate) {
        ByteImmediate = static_cast<uint8_t>(ALU->getOperand(2).getImm());
        ByteOpcode = isUInt<3>(ByteImmediate) ? SmallImmediateOpcode
                                              : FullImmediateOpcode;
      }

      Register RightWord;
      Register RightByte;
      if (HasRegister) {
        if (ALU->getNumExplicitOperands() != 3 || !ALU->getOperand(2).isReg() ||
            !ALU->getOperand(2).getReg().isPhysical() ||
            ALU->getOperand(2).getSubReg())
          continue;
        RightWord = ALU->getOperand(2).getReg();
        RightByte = TRI.getSubReg(RightWord, sub_lo8);
        if (!RightByte)
          continue;
      }

      auto AfterALU = nextNonDebug(MBB, ALU);
      auto IsDeadAfter = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterALU)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfter(C166::PSW) || !IsDeadAfter(C166::C) ||
          !onlyLowByteIsUsedUntilFullDef(MBB, AfterALU, Word, LowByte, TRI))
        continue;

      MachineInstr *RightExtension =
          HasRegister && RightWord != Word &&
                  onlyLowByteIsUsedUntilFullDef(MBB, AfterALU, RightWord,
                                                RightByte, TRI)
              ? findRedundantByteExtension(MBB, Extension.getIterator(),
                                           RightWord, RightByte, TRI)
              : nullptr;

      MachineInstrBuilder Replacement =
          BuildMI(MBB, Extension, MIMetadata(*ALU), TII.get(ByteOpcode),
                  LowByte)
              .addReg(LowByte, RegState::Kill);
      if (HasImmediate)
        Replacement.addImm(ByteImmediate);
      else if (HasRegister)
        Replacement.addReg(RightByte,
                           getKillRegState(ALU->getOperand(2).isKill()));
      Replacement->getOperand(0).setIsDead(ALU->getOperand(0).isDead());
      markRegisterDefDead(*Replacement, C166::PSW, TRI);
      markRegisterDefDead(*Replacement, C166::C, TRI);
      Replacement->setFlags(Extension.getFlags() | ALU->getFlags());
      if (RightExtension)
        Replacement->setFlags(Replacement->getFlags() |
                              RightExtension->getFlags());

      auto Resume = std::next(ALU);
      if (RightExtension)
        RightExtension->eraseFromParent();
      Extension.eraseFromParent();
      ALU->eraseFromParent();
      Changed = true;
      I = Resume;
    }
  }

  return Changed;
}

static bool getPhysicalExtensionMove(const MachineInstr &MI, unsigned &Opcode,
                                     Register &Destination, Register &Source) {
  if ((MI.getOpcode() != C166::MOVBZrr && MI.getOpcode() != C166::MOVBSrr) ||
      !MI.getOperand(0).isReg() || !MI.getOperand(0).getReg().isPhysical() ||
      MI.getOperand(0).getSubReg() || !MI.getOperand(1).isReg() ||
      !MI.getOperand(1).getReg().isPhysical() || MI.getOperand(1).getSubReg())
    return false;

  Opcode = MI.getOpcode();
  Destination = MI.getOperand(0).getReg();
  Source = MI.getOperand(1).getReg();
  return true;
}

static MachineInstr *findPredecessorExtension(MachineInstr &Move,
                                              unsigned Opcode,
                                              Register Destination,
                                              Register Source,
                                              const TargetRegisterInfo &TRI) {
  MachineBasicBlock &MBB = *Move.getParent();
  if (MBB.pred_size() != 1)
    return nullptr;
  auto First = MBB.begin();
  while (First != MBB.end() &&
         (First->isDebugInstr() || First->isMetaInstruction()))
    ++First;
  if (First != Move.getIterator())
    return nullptr;

  MachineBasicBlock &Predecessor = **MBB.pred_begin();
  if (&Predecessor == &MBB)
    return nullptr;
  for (MachineInstr &MI : llvm::reverse(Predecessor)) {
    if (MI.isDebugInstr() || MI.isMetaInstruction())
      continue;
    if (MI.isTerminator()) {
      if (MI.modifiesRegister(Destination, &TRI) ||
          MI.modifiesRegister(Source, &TRI))
        return nullptr;
      continue;
    }

    unsigned PreviousOpcode = 0;
    Register PreviousDestination;
    Register PreviousSource;
    if (!getPhysicalExtensionMove(MI, PreviousOpcode, PreviousDestination,
                                  PreviousSource) ||
        PreviousOpcode != Opcode || PreviousDestination != Destination ||
        PreviousSource != Source)
      return nullptr;
    return &MI;
  }
  return nullptr;
}

static bool removeRedundantSuccessorExtensions(MachineFunction &MF,
                                               const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Move = *I++;
      unsigned Opcode;
      Register Destination;
      Register Source;
      if (!getPhysicalExtensionMove(Move, Opcode, Destination, Source) ||
          Register(TRI.getSubReg(Destination, sub_lo8)) != Source ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Move.getIterator())))
        continue;

      auto Next = nextNonDebug(MBB, Move.getIterator());
      bool FlagsAreDead =
          MBB.computeRegisterLiveness(
              &TRI, C166::PSW, MachineBasicBlock::const_iterator(Next)) ==
              MachineBasicBlock::LQR_Dead ||
          TII.isRegisterOverwrittenBeforeUse(Move, C166::PSW);
      if (!FlagsAreDead)
        continue;

      MachineInstr *Previous =
          findPredecessorExtension(Move, Opcode, Destination, Source, TRI);
      if (!Previous)
        continue;

      Previous->getOperand(0).setIsDead(false);
      if (!MBB.isLiveIn(Destination))
        MBB.addLiveIn(Destination);
      for (auto Scan = std::next(Previous->getIterator());
           Scan != Previous->getParent()->end(); ++Scan)
        Scan->clearRegisterKills(Destination, &TRI);

      Move.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

static bool hasSamePhysicalCompare(const MachineInstr &Left,
                                   const MachineInstr &Right) {
  if (Left.getOpcode() == C166::CMPrr && Right.getOpcode() == C166::CMPrr) {
    for (unsigned Index = 0; Index != 2; ++Index) {
      const MachineOperand &LeftOperand = Left.getOperand(Index);
      const MachineOperand &RightOperand = Right.getOperand(Index);
      if (!LeftOperand.isReg() || !RightOperand.isReg() ||
          !LeftOperand.getReg().isPhysical() ||
          LeftOperand.getReg() != RightOperand.getReg() ||
          LeftOperand.getSubReg() != RightOperand.getSubReg())
        return false;
    }
    return true;
  }

  auto IsImmediateCompare = [](const MachineInstr &MI) {
    return MI.getOpcode() == C166::CMPri3 || MI.getOpcode() == C166::CMPri16;
  };
  if (!IsImmediateCompare(Left) || !IsImmediateCompare(Right))
    return false;

  const MachineOperand &LeftRegister = Left.getOperand(0);
  const MachineOperand &RightRegister = Right.getOperand(0);
  const MachineOperand &LeftImmediate = Left.getOperand(1);
  const MachineOperand &RightImmediate = Right.getOperand(1);
  return LeftRegister.isReg() && RightRegister.isReg() &&
         LeftRegister.getReg().isPhysical() &&
         LeftRegister.getReg() == RightRegister.getReg() &&
         LeftRegister.getSubReg() == RightRegister.getSubReg() &&
         LeftImmediate.isImm() && RightImmediate.isImm() &&
         LeftImmediate.getImm() == RightImmediate.getImm();
}

static MachineInstr *
findTrailingPredecessorCompare(MachineBasicBlock &MBB, MachineInstr &Compare,
                               const TargetRegisterInfo &TRI) {
  if (MBB.pred_size() != 1)
    return nullptr;

  MachineBasicBlock &Predecessor = **MBB.pred_begin();
  if (&Predecessor == &MBB)
    return nullptr;

  for (MachineInstr &MI : llvm::reverse(Predecessor)) {
    if (MI.isDebugInstr() || MI.isMetaInstruction())
      continue;
    if (MI.isTerminator()) {
      if (MI.modifiesRegister(C166::PSW, &TRI) ||
          MI.modifiesRegister(C166::C, &TRI))
        return nullptr;
      continue;
    }
    if (!hasSamePhysicalCompare(MI, Compare) ||
        isInsideExtensionWindow(
            Predecessor, MachineBasicBlock::const_iterator(MI.getIterator())))
      return nullptr;
    return &MI;
  }
  return nullptr;
}

static MachineInstr *findPredecessorCompare(MachineInstr &Compare,
                                            const TargetRegisterInfo &TRI) {
  MachineBasicBlock &MBB = *Compare.getParent();
  auto First = MBB.begin();
  while (First != MBB.end() &&
         (First->isDebugInstr() || First->isMetaInstruction()))
    ++First;
  if (First != Compare.getIterator())
    return nullptr;

  return findTrailingPredecessorCompare(MBB, Compare, TRI);
}

static bool isLiveInWithAliases(const MachineBasicBlock &MBB, Register Reg,
                                const TargetRegisterInfo &TRI) {
  return llvm::any_of(MBB.liveins(), [&](const auto &LiveIn) {
    return TRI.regsOverlap(LiveIn.PhysReg, Reg);
  });
}

static bool
hoistMovesBeforeRepeatedSuccessorCompares(MachineFunction &MF,
                                          const C166InstrInfo &TII) {
  // MOV defines the condition flags on C166.  A short sequence of register
  // copies at the start of a successor can therefore force an otherwise
  // redundant comparison to be repeated.  When the copied values are dead
  // on every other edge, execute the copies before the predecessor compare;
  // that compare restores the flags and the normal redundant-compare fold
  // can reuse them in the successor.
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    if (MBB.pred_size() != 1 || MBB.isEHPad() || MBB.hasAddressTaken())
      continue;

    SmallVector<MachineInstr *, 4> Moves;
    auto I = MBB.begin();
    while (I != MBB.end()) {
      if (I->isDebugInstr() || I->isMetaInstruction()) {
        ++I;
        continue;
      }
      if (I->getOpcode() != C166::MOVrr)
        break;
      if (!I->getOperand(0).isReg() || !I->getOperand(1).isReg() ||
          !I->getOperand(0).getReg().isPhysical() ||
          !I->getOperand(1).getReg().isPhysical() ||
          I->getOperand(0).getSubReg() || I->getOperand(1).getSubReg()) {
        Moves.clear();
        break;
      }
      Moves.push_back(&*I++);
    }

    if (Moves.empty() || I == MBB.end() ||
        (I->getOpcode() != C166::CMPrr && I->getOpcode() != C166::CMPri3 &&
         I->getOpcode() != C166::CMPri16) ||
        isInsideExtensionWindow(MBB, MachineBasicBlock::const_iterator(I)))
      continue;

    MachineInstr &Compare = *I;
    MachineInstr *Previous = findTrailingPredecessorCompare(MBB, Compare, TRI);
    if (!Previous)
      continue;
    MachineBasicBlock &Predecessor = *Previous->getParent();

    bool Safe = true;
    for (MachineInstr *Move : Moves) {
      Register Destination = Move->getOperand(0).getReg();
      if (MRI.isReserved(Destination) ||
          Compare.readsRegister(Destination, &TRI)) {
        Safe = false;
        break;
      }
      for (MachineBasicBlock *Successor : Predecessor.successors()) {
        if (Successor != &MBB &&
            isLiveInWithAliases(*Successor, Destination, TRI)) {
          Safe = false;
          break;
        }
      }
      if (!Safe)
        break;
    }
    if (!Safe)
      continue;

    for (MachineInstr *Move : Moves) {
      Register Destination = Move->getOperand(0).getReg();
      Register Source = Move->getOperand(1).getReg();
      Move->clearRegisterKills(Source, &TRI);
      if (!MBB.isLiveIn(Destination))
        MBB.addLiveIn(Destination);
      Predecessor.splice(Previous->getIterator(), &MBB, Move->getIterator());
    }
    Changed = true;
  }

  return Changed;
}

static bool removeRedundantSuccessorCompares(MachineFunction &MF,
                                             const C166InstrInfo &TII) {
  // Branch pseudo expansion can leave the same physical comparison at the
  // start of a single-predecessor successor.  C166 branches preserve the
  // comparison flags, so keep the predecessor definition live across the
  // edge instead of recomputing it.
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Compare = *I++;
      if (Compare.getOpcode() != C166::CMPrr &&
          Compare.getOpcode() != C166::CMPri3 &&
          Compare.getOpcode() != C166::CMPri16)
        continue;
      if (isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Compare.getIterator())))
        continue;

      MachineInstr *Previous = findPredecessorCompare(Compare, TRI);
      if (!Previous)
        continue;

      for (Register Flag : {Register(C166::PSW), Register(C166::C)}) {
        const MachineOperand *RemovedDef =
            Compare.findRegisterDefOperand(Flag, &TRI);
        if (!RemovedDef || RemovedDef->isDead())
          continue;
        if (MachineOperand *PreviousDef =
                Previous->findRegisterDefOperand(Flag, &TRI))
          PreviousDef->setIsDead(false);
        if (!MBB.isLiveIn(Flag))
          MBB.addLiveIn(Flag);
        for (auto Scan = std::next(Previous->getIterator());
             Scan != Previous->getParent()->end(); ++Scan)
          Scan->clearRegisterKills(Flag, &TRI);
      }

      Compare.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

static bool removeEqualityProvenImmediateMoves(MachineFunction &MF,
                                               const C166InstrInfo &TII) {
  // On an equality edge, the compared register already contains the immediate
  // being materialized at the start of the successor.
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    if (MBB.pred_size() != 1 || MBB.isEHPad() || MBB.hasAddressTaken())
      continue;

    auto Move = MBB.begin();
    while (Move != MBB.end() &&
           (Move->isDebugInstr() || Move->isMetaInstruction()))
      ++Move;
    if (Move == MBB.end() ||
        (Move->getOpcode() != C166::MOVri4 &&
         Move->getOpcode() != C166::MOVri16) ||
        !isWholePhysicalRegister(Move->getOperand(0)) ||
        !Move->getOperand(1).isImm() || Move->isBundledWithPred() ||
        Move->isBundledWithSucc() || isInsideExtensionWindow(MBB, Move))
      continue;

    const MachineOperand *FlagDef =
        Move->findRegisterDefOperand(C166::PSW, &TRI);
    if (!FlagDef || !FlagDef->isDead())
      continue;

    MachineBasicBlock &Predecessor = **MBB.pred_begin();
    auto Branch = Predecessor.getLastNonDebugInstr();
    if (Branch == Predecessor.end() ||
        (Branch->getOpcode() != C166::JMPR_EQ &&
         Branch->getOpcode() != C166::JMPR_NE) ||
        Branch->isBundledWithPred() || Branch->isBundledWithSucc() ||
        Branch->getNumExplicitOperands() != 1 ||
        !Branch->getOperand(0).isMBB() ||
        isInsideExtensionWindow(Predecessor, Branch))
      continue;

    bool IsEqualityEdge = Branch->getOpcode() == C166::JMPR_EQ
                              ? Branch->getOperand(0).getMBB() == &MBB
                              : Predecessor.getNextNode() == &MBB &&
                                    Branch->getOperand(0).getMBB() != &MBB;
    if (!IsEqualityEdge || !Predecessor.isSuccessor(&MBB))
      continue;

    auto Compare = previousNonDebug(Predecessor, Branch);
    if (Compare == Predecessor.end() ||
        (Compare->getOpcode() != C166::CMPri3 &&
         Compare->getOpcode() != C166::CMPri16) ||
        Compare->isBundledWithPred() || Compare->isBundledWithSucc() ||
        !isWholePhysicalRegister(Compare->getOperand(0)) ||
        !Compare->getOperand(1).isImm() ||
        isInsideExtensionWindow(Predecessor, Compare))
      continue;

    Register Value = Move->getOperand(0).getReg();
    if (!C166::GR16RegClass.contains(Value) ||
        Compare->getOperand(0).getReg() != Value ||
        static_cast<uint16_t>(Compare->getOperand(1).getImm()) !=
            static_cast<uint16_t>(Move->getOperand(1).getImm()))
      continue;

    Compare->clearRegisterKills(Value, &TRI);
    if (!MBB.isLiveIn(Value))
      MBB.addLiveIn(Value);
    Move->eraseFromParent();
    Changed = true;
  }

  return Changed;
}

static bool forwardExplicitJumpOnlyBlocks(MachineFunction &MF) {
  // Earlier post-RA folds can empty blocks after generic branch folding has
  // run.  Forward only explicit branches: deleting a fallthrough block would
  // also change the layout path.
  SmallVector<MachineBasicBlock *, 4> ToErase;

  for (MachineBasicBlock &MBB : MF) {
    if (MBB.pred_size() != 1 || MBB.succ_size() != 1 || MBB.hasAddressTaken() ||
        MBB.isEHPad() || MBB.isEHScopeEntry() || MBB.isEHContTarget() ||
        MBB.isEHFuncletEntry() || MBB.isCleanupFuncletEntry() ||
        MBB.isBeginSection() || MBB.isEndSection() || MBB.size() != 1)
      continue;

    MachineBasicBlock &Predecessor = **MBB.pred_begin();
    MachineBasicBlock &Successor = **MBB.succ_begin();
    if (&Predecessor == &MBB || &Successor == &MBB ||
        &Predecessor == &Successor ||
        Predecessor.getSectionID() != MBB.getSectionID() ||
        MBB.getSectionID() != Successor.getSectionID() ||
        !Successor.phis().empty())
      continue;

    MachineInstr &Jump = MBB.front();
    if (Jump.getOpcode() != C166::JMPR_UC || Jump.isBundledWithPred() ||
        Jump.isBundledWithSucc() || Jump.getNumExplicitOperands() != 1 ||
        Jump.getDebugLoc() || Jump.getFlags() || !Jump.getOperand(0).isMBB() ||
        Jump.getOperand(0).getMBB() != &Successor)
      continue;

    bool HasExplicitPredecessorBranch = false;
    bool CrossesExtensionWindow = false;
    for (MachineInstr &Terminator : Predecessor.terminators()) {
      for (const MachineOperand &Operand : Terminator.operands()) {
        if (Operand.isMBB() && Operand.getMBB() == &MBB) {
          HasExplicitPredecessorBranch = true;
          CrossesExtensionWindow |= isInsideExtensionWindow(
              Predecessor,
              MachineBasicBlock::const_iterator(Terminator.getIterator()));
          break;
        }
      }
    }
    if (!HasExplicitPredecessorBranch || CrossesExtensionWindow)
      continue;

    Predecessor.ReplaceUsesOfBlockWith(&MBB, &Successor);
    MBB.removeSuccessor(&Successor);
    ToErase.push_back(&MBB);
  }

  for (MachineBasicBlock *MBB : ToErase)
    MBB->eraseFromParent();
  return !ToErase.empty();
}

static bool getPhysicalImmediateMove(const MachineInstr &MI,
                                     Register &Destination,
                                     uint16_t &Immediate) {
  if ((MI.getOpcode() != C166::MOVri4 && MI.getOpcode() != C166::MOVri16) ||
      !MI.getOperand(0).isReg() || !MI.getOperand(0).getReg().isPhysical() ||
      MI.getOperand(0).getSubReg() || !MI.getOperand(1).isImm())
    return false;

  Destination = MI.getOperand(0).getReg();
  Immediate = static_cast<uint16_t>(MI.getOperand(1).getImm());
  return true;
}

static bool getPhysicalTwoAddressRegister(const MachineInstr &MI,
                                          unsigned Opcode,
                                          Register &Destination,
                                          Register &Source) {
  if (MI.getOpcode() != Opcode || !isWholePhysicalRegister(MI.getOperand(0)) ||
      !isWholePhysicalRegister(MI.getOperand(1)) ||
      !isWholePhysicalRegister(MI.getOperand(2)) ||
      MI.getOperand(0).getReg() != MI.getOperand(1).getReg())
    return false;

  Destination = MI.getOperand(0).getReg();
  Source = MI.getOperand(2).getReg();
  return true;
}

static bool getPhysicalTwoAddressImmediate(const MachineInstr &MI,
                                           unsigned Opcode,
                                           Register &Destination,
                                           int64_t &Immediate) {
  if (MI.getOpcode() != Opcode || !isWholePhysicalRegister(MI.getOperand(0)) ||
      !isWholePhysicalRegister(MI.getOperand(1)) || !MI.getOperand(2).isImm() ||
      MI.getOperand(0).getReg() != MI.getOperand(1).getReg())
    return false;

  Destination = MI.getOperand(0).getReg();
  Immediate = MI.getOperand(2).getImm();
  return true;
}

static MachineInstr *findReachingImmediateDef(MachineInstr &Move,
                                              Register RegisterValue,
                                              uint16_t Immediate,
                                              const TargetRegisterInfo &TRI) {
  MachineInstr *Definition = nullptr;
  for (auto I = Move.getIterator(); I != Move.getParent()->begin();) {
    --I;
    if (I->isDebugInstr() || I->isMetaInstruction())
      continue;
    if (I->modifiesRegister(RegisterValue, &TRI)) {
      Definition = &*I;
      break;
    }
  }

  Register DefinedRegister;
  uint16_t DefinedImmediate;
  if (!Definition ||
      !getPhysicalImmediateMove(*Definition, DefinedRegister,
                                DefinedImmediate) ||
      DefinedRegister != RegisterValue || DefinedImmediate != Immediate)
    return nullptr;
  return Definition;
}

static bool collectUsesBeforeRedef(
    MachineInstr &Definition, Register Value, const TargetRegisterInfo &TRI,
    SmallVectorImpl<std::pair<MachineInstr *, unsigned>> &Uses,
    MachineInstr *&LastUse) {
  MachineBasicBlock &MBB = *Definition.getParent();
  for (auto I = std::next(Definition.getIterator()); I != MBB.end(); ++I) {
    if (I->isDebugInstr()) {
      if (readsPhysicalRegister(*I, Value, TRI))
        return false;
      continue;
    }
    if (I->isMetaInstruction())
      continue;

    bool Reads = readsPhysicalRegister(*I, Value, TRI);
    if (I->modifiesRegister(Value, &TRI)) {
      if (Reads)
        return false;
      break;
    }
    if (!Reads)
      continue;

    unsigned OldUseCount = Uses.size();
    for (unsigned Operand = 0; Operand != I->getNumOperands(); ++Operand) {
      MachineOperand &MO = I->getOperand(Operand);
      if (!MO.isReg() || !MO.isUse() || !MO.getReg() ||
          !TRI.regsOverlap(MO.getReg(), Value))
        continue;
      if (MO.isImplicit() || MO.getReg() != Value || MO.getSubReg()) {
        Uses.clear();
        return false;
      }
      Uses.emplace_back(&*I, Operand);
    }
    if (Uses.size() == OldUseCount) {
      Uses.clear();
      return false;
    }
    LastUse = &*I;
  }

  if (Uses.empty())
    return false;
  auto AfterUse = nextNonDebug(MBB, LastUse->getIterator());
  return MBB.computeRegisterLiveness(
             &TRI, Value, MachineBasicBlock::const_iterator(AfterUse)) ==
         MachineBasicBlock::LQR_Dead;
}

static bool getImmediateALUOpcodes(const MachineInstr &MI, Register Value,
                                   unsigned &ShortOpcode, unsigned &LongOpcode,
                                   unsigned &LhsOperand, unsigned &RhsOperand) {
  switch (MI.getOpcode()) {
  case C166::ADDrr:
  case C166::ADDCarryrr:
    ShortOpcode = C166::ADDri3;
    LongOpcode = C166::ADDri16;
    break;
  case C166::ADDCrr:
  case C166::ADDCCarryrr:
  case C166::ADDCCarryInrr:
    ShortOpcode = C166::ADDCri3;
    LongOpcode = C166::ADDCri16;
    break;
  case C166::SUBrr:
  case C166::SUBCarryrr:
    ShortOpcode = C166::SUBri3;
    LongOpcode = C166::SUBri16;
    break;
  case C166::SUBCrr:
  case C166::SUBCCarryrr:
  case C166::SUBCCarryInrr:
    ShortOpcode = C166::SUBCri3;
    LongOpcode = C166::SUBCri16;
    break;
  case C166::XORrr:
    ShortOpcode = C166::XORri3;
    LongOpcode = C166::XORri16;
    break;
  case C166::ANDrr:
    ShortOpcode = C166::ANDri3;
    LongOpcode = C166::ANDri16;
    break;
  case C166::ORrr:
    ShortOpcode = C166::ORri3;
    LongOpcode = C166::ORri16;
    break;
  default:
    return false;
  }

  bool HasExplicitCarryOut = MI.getOpcode() == C166::ADDCarryrr ||
                             MI.getOpcode() == C166::ADDCCarryrr ||
                             MI.getOpcode() == C166::SUBCarryrr ||
                             MI.getOpcode() == C166::SUBCCarryrr;
  LhsOperand = HasExplicitCarryOut ? 2 : 1;
  RhsOperand = HasExplicitCarryOut ? 3 : 2;

  return MI.getOperand(0).isReg() && !MI.getOperand(0).getSubReg() &&
         MI.getOperand(LhsOperand).isReg() &&
         MI.getOperand(LhsOperand).getReg() == MI.getOperand(0).getReg() &&
         !MI.getOperand(LhsOperand).getSubReg() &&
         MI.getOperand(RhsOperand).isReg() &&
         MI.getOperand(RhsOperand).getReg() == Value &&
         !MI.getOperand(RhsOperand).getSubReg();
}

static bool foldImmediateIntoALU(MachineInstr &Move, Register Value,
                                 uint16_t Immediate, const C166InstrInfo &TII,
                                 MachineBasicBlock::iterator &Resume) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  SmallVector<std::pair<MachineInstr *, unsigned>, 2> Uses;
  MachineInstr *LastUse = nullptr;
  bool Collected = collectUsesBeforeRedef(Move, Value, TRI, Uses, LastUse);
  if (!Collected || Uses.size() != 1)
    return false;

  MachineInstr &Use = *Uses.front().first;
  unsigned ShortOpcode;
  unsigned LongOpcode;
  unsigned LhsOperand;
  unsigned RhsOperand;
  if (!getImmediateALUOpcodes(Use, Value, ShortOpcode, LongOpcode, LhsOperand,
                              RhsOperand) ||
      Uses.front().second != RhsOperand)
    return false;

  bool HasShortImmediate = isUInt<3>(Immediate);
  if (!HasShortImmediate && Move.getOpcode() != C166::MOVri16)
    return false;

  MachineBasicBlock &MBB = *Use.getParent();
  auto AfterMove = std::next(Move.getIterator());
  MachineInstrBuilder Replacement =
      BuildMI(MBB, Use, MIMetadata(Use),
              TII.get(HasShortImmediate ? ShortOpcode : LongOpcode),
              Use.getOperand(0).getReg())
          .add(Use.getOperand(LhsOperand))
          .addImm(Immediate);
  Replacement->getOperand(0).setIsDead(Use.getOperand(0).isDead());
  copyFlagDefLiveness(*Replacement, Use, TRI);
  Replacement->setFlags(Use.getFlags());

  bool UseImmediatelyFollowsMove = &*AfterMove == &Use;
  Use.eraseFromParent();
  Resume = UseImmediatelyFollowsMove ? Replacement->getIterator() : AfterMove;
  Move.eraseFromParent();
  return true;
}

static bool reuseImmediateForUses(MachineInstr &Move, Register Value,
                                  uint16_t Immediate,
                                  const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  SmallVector<std::pair<MachineInstr *, unsigned>, 4> Uses;
  MachineInstr *LastUse = nullptr;
  if (!collectUsesBeforeRedef(Move, Value, TRI, Uses, LastUse))
    return false;

  for (MCPhysReg Candidate : C166::GR16RegClass) {
    if (Candidate == Value || Candidate == C166::R0)
      continue;

    MachineInstr *Definition =
        findReachingImmediateDef(Move, Candidate, Immediate, TRI);
    if (!Definition)
      continue;

    bool Clobbered = false;
    for (auto I = std::next(Move.getIterator());; ++I) {
      Clobbered |= I->modifiesRegister(Candidate, &TRI);
      if (&*I == LastUse)
        break;
    }
    if (Clobbered)
      continue;

    bool AcceptsCandidate = true;
    for (auto [Use, Operand] : Uses) {
      const TargetRegisterClass *RC = TII.getRegClass(Use->getDesc(), Operand);
      AcceptsCandidate &= !RC || RC->contains(Candidate);
    }
    if (!AcceptsCandidate)
      continue;

    Definition->getOperand(0).setIsDead(false);
    for (auto I = std::next(Definition->getIterator());; ++I) {
      for (MachineOperand &MO : I->operands())
        if (MO.isReg() && MO.isUse() && MO.isKill() && MO.getReg() &&
            TRI.regsOverlap(MO.getReg(), Candidate))
          MO.setIsKill(false);
      if (&*I == LastUse)
        break;
    }
    for (auto [Use, Operand] : Uses) {
      MachineOperand &MO = Use->getOperand(Operand);
      MO.setReg(Candidate);
      MO.setIsKill(false);
    }
    Move.eraseFromParent();
    return true;
  }
  return false;
}

static bool replaceImmediateWithCopy(MachineInstr &Move, Register Value,
                                     uint16_t Immediate,
                                     const C166InstrInfo &TII) {
  if (Move.getOpcode() != C166::MOVri16)
    return false;

  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  for (MCPhysReg Candidate : C166::GR16RegClass) {
    if (Candidate == C166::R0 || TRI.regsOverlap(Candidate, Value))
      continue;

    MachineInstr *Definition =
        findReachingImmediateDef(Move, Candidate, Immediate, TRI);
    if (!Definition)
      continue;

    Definition->getOperand(0).setIsDead(false);
    for (auto I = std::next(Definition->getIterator()); I != Move.getIterator();
         ++I)
      for (MachineOperand &MO : I->operands())
        if (MO.isReg() && MO.isUse() && MO.isKill() && MO.getReg() &&
            TRI.regsOverlap(MO.getReg(), Candidate))
          MO.setIsKill(false);

    MachineBasicBlock &MBB = *Move.getParent();
    auto Next = nextNonDebug(MBB, Move.getIterator());
    bool CandidateIsDead =
        MBB.computeRegisterLiveness(&TRI, Candidate,
                                    MachineBasicBlock::const_iterator(Next)) ==
        MachineBasicBlock::LQR_Dead;
    MachineInstrBuilder Copy =
        BuildMI(MBB, Move, MIMetadata(Move), TII.get(C166::MOVrr), Value)
            .addReg(Candidate, getKillRegState(CandidateIsDead));
    Copy->getOperand(0).setIsDead(Move.getOperand(0).isDead());
    copyPSWDefLiveness(*Copy, Move, TRI);
    Copy->setFlags(Move.getFlags());
    Move.eraseFromParent();
    return true;
  }
  return false;
}

static bool removeRedundantPhysicalImmediates(MachineFunction &MF,
                                              const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Move = *I++;
      Register Value;
      uint16_t Immediate;
      if (!getPhysicalImmediateMove(Move, Value, Immediate) ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Move.getIterator())))
        continue;

      auto Next = nextNonDebug(MBB, Move.getIterator());
      const MachineOperand *PSWDef =
          Move.findRegisterDefOperand(C166::PSW, &TRI);
      bool FlagsAreDead =
          (PSWDef && PSWDef->isDead()) ||
          MBB.computeRegisterLiveness(
              &TRI, C166::PSW, MachineBasicBlock::const_iterator(Next)) ==
              MachineBasicBlock::LQR_Dead ||
          TII.isRegisterOverwrittenBeforeUse(Move, C166::PSW);
      if (!FlagsAreDead)
        continue;

      auto Resume = I;
      if (foldImmediateIntoALU(Move, Value, Immediate, TII, Resume)) {
        I = Resume;
        Changed = true;
        continue;
      }

      if (reuseImmediateForUses(Move, Value, Immediate, TII)) {
        Changed = true;
        continue;
      }

      if (replaceImmediateWithCopy(Move, Value, Immediate, TII)) {
        Changed = true;
        continue;
      }

      MachineInstr *PreviousDef =
          findReachingImmediateDef(Move, Value, Immediate, TRI);
      if (!PreviousDef)
        continue;

      PreviousDef->getOperand(0).setIsDead(false);
      for (auto Scan = std::next(PreviousDef->getIterator());
           Scan != Move.getIterator(); ++Scan)
        for (MachineOperand &MO : Scan->operands())
          if (MO.isReg() && MO.isUse() && MO.isKill() && MO.getReg() &&
              TRI.regsOverlap(MO.getReg(), Value))
            MO.setIsKill(false);
      Move.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

static bool replaceZeroAnds(MachineFunction &MF, const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &And = *I++;
      if ((And.getOpcode() != C166::ANDri3 &&
           And.getOpcode() != C166::ANDri16) ||
          !And.getOperand(0).isReg() ||
          !And.getOperand(0).getReg().isPhysical() ||
          And.getOperand(0).getSubReg() || !And.getOperand(1).isReg() ||
          And.getOperand(1).getReg() != And.getOperand(0).getReg() ||
          And.getOperand(1).getSubReg() || !And.getOperand(2).isImm() ||
          And.getOperand(2).getImm() != 0)
        continue;

      auto Next = nextNonDebug(MBB, And.getIterator());
      auto IsDeadAfterAnd = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(Next)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterAnd(C166::PSW) || !IsDeadAfterAnd(C166::C))
        continue;

      Register Value = And.getOperand(0).getReg();
      MachineInstr *DiscardedDef = nullptr;
      for (auto Scan = std::prev(I); Scan != MBB.begin();) {
        --Scan;
        if (Scan->isDebugInstr() || Scan->isMetaInstruction())
          continue;
        bool ReadsValue = Scan->readsRegister(Value, &TRI);
        if (!Scan->modifiesRegister(Value, &TRI)) {
          if (ReadsValue)
            break;
          continue;
        }

        unsigned Opcode = Scan->getOpcode();
        bool IsDiscardableDef = Opcode == C166::MOVri4 ||
                                Opcode == C166::MOVri16 ||
                                Opcode == C166::MOVrr;
        if (IsDiscardableDef &&
            TII.isRegisterOverwrittenBeforeUse(*Scan, C166::PSW) &&
            !isInsideExtensionWindow(MBB, Scan))
          DiscardedDef = &*Scan;
        break;
      }

      MachineInstrBuilder Replacement =
          BuildMI(MBB, And, MIMetadata(And), TII.get(C166::MOVri4), Value)
              .addImm(0);
      Replacement->getOperand(0).setIsDead(And.getOperand(0).isDead());
      markRegisterDefDead(*Replacement, C166::PSW, TRI);
      Replacement->setFlags(And.getFlags());
      And.eraseFromParent();
      if (DiscardedDef)
        DiscardedDef->eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

static bool foldPhysicalIndirectALULoads(MachineFunction &MF,
                                         const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  SmallVector<MachineInstr *, 16> Loads;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.getOpcode() == C166::MOVrm || MI.getOpcode() == C166::MOVrmPostInc)
        Loads.push_back(&MI);

  bool Changed = false;
  for (MachineInstr *Load : Loads) {
    if (!Load->getParent() || hasExplicitOrderedMemoryAccess(*Load) ||
        isInsideExtensionWindow(
            *Load->getParent(),
            MachineBasicBlock::const_iterator(Load->getIterator())) ||
        !Load->getOperand(0).isReg() ||
        !Load->getOperand(0).getReg().isPhysical() ||
        Load->getOperand(0).getSubReg())
      continue;

    bool PostIncrement = Load->getOpcode() == C166::MOVrmPostInc;
    unsigned BaseOperand = PostIncrement ? 2 : 1;
    if (!Load->getOperand(BaseOperand).isReg() ||
        !Load->getOperand(BaseOperand).getReg().isPhysical() ||
        Load->getOperand(BaseOperand).getSubReg())
      continue;

    const MachineOperand *PSWDef =
        Load->findRegisterDefOperand(C166::PSW, &TRI);
    if (!PSWDef || (!PSWDef->isDead() &&
                    !TII.isRegisterOverwrittenBeforeUse(*Load, C166::PSW)))
      continue;

    MachineBasicBlock &MBB = *Load->getParent();
    Register Value = Load->getOperand(0).getReg();
    Register Base = Load->getOperand(BaseOperand).getReg();
    MachineInstr *Use = nullptr;
    unsigned FoldedOperand = 0;
    for (auto Scan = std::next(Load->getIterator()); Scan != MBB.instr_end();
         ++Scan) {
      if (Scan->isDebugInstr()) {
        if (readsPhysicalRegister(*Scan, Value, TRI) ||
            (PostIncrement && readsPhysicalRegister(*Scan, Base, TRI)))
          break;
        continue;
      }
      if (Scan->isCall() || Scan->isInlineAsm() || Scan->isTerminator() ||
          Scan->hasUnmodeledSideEffects() || Scan->mayStore() ||
          hasExplicitOrderedMemoryAccess(*Scan) ||
          isExtensionOpcode(Scan->getOpcode()))
        break;
      if (Scan->modifiesRegister(Base, &TRI) ||
          (PostIncrement && readsPhysicalRegister(*Scan, Base, TRI)))
        break;

      bool ReadsValue = readsPhysicalRegister(*Scan, Value, TRI);
      bool ModifiesValue = Scan->modifiesRegister(Value, &TRI);
      if (!ReadsValue && !ModifiesValue)
        continue;
      if (!ReadsValue || ModifiesValue)
        break;

      unsigned MatchingOperands = 0;
      for (unsigned Operand = Scan->getNumExplicitDefs();
           Operand != Scan->getNumExplicitOperands(); ++Operand) {
        const MachineOperand &MO = Scan->getOperand(Operand);
        if (!MO.isReg() || MO.getReg() != Value || MO.getSubReg())
          continue;
        FoldedOperand = Operand;
        ++MatchingOperands;
      }
      // The load is erased after folding, so its value must die at the
      // folded use.  A later use would otherwise read an undefined physical
      // register.
      if (MatchingOperands == 1 && Scan->getOperand(FoldedOperand).isKill())
        Use = &*Scan;
      break;
    }
    if (!Use)
      continue;

    MachineInstr *Copy = nullptr;
    MachineInstr *Folded = TII.foldMemoryOperandImpl(
        MF, *Use, {FoldedOperand}, *Load, Copy, nullptr, nullptr);
    if (!Folded)
      continue;
    assert(!Copy && "C166 indirect ALU folding created an unexpected copy");
    Folded->setMemRefs(MF, Load->memoperands());
    Folded->setFlags(Load->getFlags() | Use->getFlags());
    Use->eraseFromParent();
    Load->eraseFromParent();
    Changed = true;
  }

  return Changed;
}

static bool formPostIncrementLoadChains(MachineFunction &MF,
                                        const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      if (I->getOpcode() != C166::MOVrm || hasExplicitOrderedMemoryAccess(*I) ||
          !I->getOperand(0).getReg().isPhysical() ||
          !I->getOperand(1).getReg().isPhysical()) {
        ++I;
        continue;
      }

      Register Base = I->getOperand(1).getReg();
      if (Base == C166::R0) {
        ++I;
        continue;
      }

      SmallVector<MachineInstr *, 4> Loads{&*I};
      auto Scan = nextNonDebug(MBB, I);
      int64_t ExpectedOffset = 2;
      while (Scan != MBB.end() && Scan->getOpcode() == C166::MOVrm16 &&
             !hasExplicitOrderedMemoryAccess(*Scan) &&
             Scan->getOperand(0).getReg().isPhysical() &&
             Scan->getOperand(1).isReg() &&
             Scan->getOperand(1).getReg() == Base &&
             Scan->getOperand(2).isImm() &&
             Scan->getOperand(2).getImm() == ExpectedOffset) {
        Loads.push_back(&*Scan);
        ExpectedOffset += 2;
        Scan = nextNonDebug(MBB, Scan);
      }

      if (Loads.size() < 2) {
        ++I;
        continue;
      }

      bool ClobbersBaseEarly = false;
      for (MachineInstr *Load : ArrayRef(Loads).drop_back())
        ClobbersBaseEarly |=
            TRI.regsOverlap(Load->getOperand(0).getReg(), Base);
      MachineInstr &Last = *Loads.back();
      bool LastDefinesBase = TRI.regsOverlap(Last.getOperand(0).getReg(), Base);
      bool BaseIsDead =
          MBB.computeRegisterLiveness(
              &TRI, Base, MachineBasicBlock::const_iterator(Scan)) ==
          MachineBasicBlock::LQR_Dead;
      if (ClobbersBaseEarly || (!LastDefinesBase && !BaseIsDead)) {
        I = Scan;
        continue;
      }

      for (MachineInstr *Load : ArrayRef(Loads).drop_back()) {
        MachineInstrBuilder Replacement =
            BuildMI(MBB, *Load, MIMetadata(*Load), TII.get(C166::MOVrmPostInc),
                    Load->getOperand(0).getReg())
                .addDef(Base)
                .addReg(Base, RegState::Kill);
        Replacement->getOperand(0).setIsDead(Load->getOperand(0).isDead());
        Replacement.cloneMemRefs(*Load).setMIFlags(Load->getFlags());
        MachineOperand *PSWDef =
            Replacement->findRegisterDefOperand(C166::PSW, &TRI);
        if (PSWDef)
          PSWDef->setIsDead(true);
      }

      MachineInstrBuilder Replacement =
          BuildMI(MBB, Last, MIMetadata(Last), TII.get(C166::MOVrm),
                  Last.getOperand(0).getReg())
              .addReg(Base, RegState::Kill);
      Replacement->getOperand(0).setIsDead(Last.getOperand(0).isDead());
      Replacement.cloneMemRefs(Last).setMIFlags(Last.getFlags());
      copyPSWDefLiveness(*Replacement, Last, TRI);

      for (MachineInstr *Load : Loads)
        Load->eraseFromParent();
      Changed = true;
      I = Scan;
    }
  }

  return Changed;
}

static bool getPhysicalLoadAddress(const MachineInstr &MI, Register &Base,
                                   int64_t &Offset) {
  if ((MI.getOpcode() != C166::MOVrm && MI.getOpcode() != C166::MOVrm16) ||
      hasExplicitOrderedMemoryAccess(MI) || !MI.getOperand(0).isReg() ||
      !MI.getOperand(0).getReg().isPhysical() || !MI.getOperand(1).isReg() ||
      !MI.getOperand(1).getReg().isPhysical())
    return false;

  Base = MI.getOperand(1).getReg();
  Offset = 0;
  if (MI.getOpcode() == C166::MOVrm16) {
    if (!MI.getOperand(2).isImm())
      return false;
    Offset = MI.getOperand(2).getImm();
  }
  return Offset >= 0 && !(Offset & 1) && isUInt<16>(Offset);
}

static bool getPhysicalStoreAddress(const MachineInstr &MI, Register &Base,
                                    int64_t &Offset, Register &Source) {
  if ((MI.getOpcode() != C166::MOVmr && MI.getOpcode() != C166::MOVmr16) ||
      hasExplicitOrderedMemoryAccess(MI) || !MI.getOperand(0).isReg() ||
      !MI.getOperand(0).getReg().isPhysical() || MI.getOperand(0).getSubReg())
    return false;

  unsigned SourceOperand = MI.getOpcode() == C166::MOVmr ? 1 : 2;
  if (!MI.getOperand(SourceOperand).isReg() ||
      !MI.getOperand(SourceOperand).getReg().isPhysical() ||
      MI.getOperand(SourceOperand).getSubReg())
    return false;

  Base = MI.getOperand(0).getReg();
  Source = MI.getOperand(SourceOperand).getReg();
  Offset = 0;
  if (MI.getOpcode() == C166::MOVmr16) {
    if (!MI.getOperand(1).isImm())
      return false;
    Offset = MI.getOperand(1).getImm();
  }
  return Offset >= 0 && !(Offset & 1) && isUInt<16>(Offset);
}

static bool foldStoredWideAddChains(MachineFunction &MF,
                                    const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &CopyLow = *I++;
      if (CopyLow.getOpcode() != C166::MOVrr ||
          !isWholePhysicalRegister(CopyLow.getOperand(0)) ||
          !isWholePhysicalRegister(CopyLow.getOperand(1)) ||
          CopyLow.isBundledWithPred() || CopyLow.isBundledWithSucc() ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(CopyLow.getIterator())))
        continue;

      auto ZeroHigh = nextNonDebug(MBB, CopyLow.getIterator());
      auto AddWord =
          ZeroHigh == MBB.end() ? MBB.end() : nextNonDebug(MBB, ZeroHigh);
      auto AddWordCarry =
          AddWord == MBB.end() ? MBB.end() : nextNonDebug(MBB, AddWord);
      auto AddPair = AddWordCarry == MBB.end()
                         ? MBB.end()
                         : nextNonDebug(MBB, AddWordCarry);
      auto AddPairCarry =
          AddPair == MBB.end() ? MBB.end() : nextNonDebug(MBB, AddPair);
      auto Store = AddPairCarry == MBB.end() ? MBB.end()
                                             : nextNonDebug(MBB, AddPairCarry);
      if (ZeroHigh == MBB.end() || AddWord == MBB.end() ||
          AddWordCarry == MBB.end() || AddPair == MBB.end() ||
          AddPairCarry == MBB.end() || Store == MBB.end())
        continue;

      Register DstLow = CopyLow.getOperand(0).getReg();
      Register Carry = CopyLow.getOperand(1).getReg();
      Register DstHigh;
      Register AddWordDst;
      Register Word;
      Register AddWordCarryDst;
      Register AddPairDst;
      Register AddendLow;
      Register AddPairCarryDst;
      Register AddendHigh;
      uint16_t Zero;
      int64_t CarryImmediate;
      if (!getPhysicalImmediateMove(*ZeroHigh, DstHigh, Zero) || Zero != 0 ||
          !getPhysicalTwoAddressRegister(*AddWord, C166::ADDrr, AddWordDst,
                                         Word) ||
          AddWordDst != DstLow ||
          !getPhysicalTwoAddressImmediate(*AddWordCarry, C166::ADDCri3,
                                          AddWordCarryDst, CarryImmediate) ||
          AddWordCarryDst != DstHigh || CarryImmediate != 0 ||
          !getPhysicalTwoAddressRegister(*AddPair, C166::ADDrr, AddPairDst,
                                         AddendLow) ||
          AddPairDst != DstLow ||
          !getPhysicalTwoAddressRegister(*AddPairCarry, C166::ADDCrr,
                                         AddPairCarryDst, AddendHigh) ||
          AddPairCarryDst != DstHigh)
        continue;

      if (!formsGR32Pair(DstLow, DstHigh, TRI) ||
          !formsGR32Pair(AddendLow, AddendHigh, TRI) ||
          TRI.regsOverlap(DstLow, DstHigh) ||
          TRI.regsOverlap(DstLow, AddendLow) ||
          TRI.regsOverlap(DstLow, AddendHigh) ||
          TRI.regsOverlap(DstHigh, AddendLow) ||
          TRI.regsOverlap(DstHigh, AddendHigh) ||
          TRI.regsOverlap(Word, DstLow) || TRI.regsOverlap(Word, DstHigh) ||
          TRI.regsOverlap(Word, AddendLow) ||
          TRI.regsOverlap(Word, AddendHigh) || TRI.regsOverlap(Word, Carry) ||
          TRI.regsOverlap(Carry, DstLow) || TRI.regsOverlap(Carry, AddendLow) ||
          TRI.regsOverlap(Carry, AddendHigh))
        continue;
      Register StoreBase;
      Register StoreSource;
      int64_t StoreOffset;
      if (!getPhysicalStoreAddress(*Store, StoreBase, StoreOffset,
                                   StoreSource) ||
          StoreSource != DstLow || TRI.regsOverlap(StoreBase, DstLow) ||
          TRI.regsOverlap(StoreBase, DstHigh) ||
          TRI.regsOverlap(StoreBase, AddendLow) ||
          TRI.regsOverlap(StoreBase, AddendHigh))
        continue;
      bool HasDebugInstruction = false;
      for (auto Scan = std::next(CopyLow.getIterator()); Scan != Store; ++Scan)
        HasDebugInstruction |= Scan->isDebugInstr();
      if (HasDebugInstruction ||
          !isUnusedBeforeOverwrite(*Store, DstLow, TRI, DstHigh) ||
          !isUnusedBeforeOverwrite(*AddPairCarry, AddendLow, TRI) ||
          !isUnusedBeforeOverwrite(*AddPairCarry, AddendHigh, TRI) ||
          !isUnusedBeforeOverwrite(*AddPairCarry, C166::PSW, TRI) ||
          !isUnusedBeforeOverwrite(*AddPairCarry, C166::C, TRI))
        continue;

      bool PreserveHigh = !isUnusedBeforeOverwrite(*Store, DstHigh, TRI);
      if (PreserveHigh && !isUnusedBeforeOverwrite(*Store, C166::PSW, TRI))
        continue;
      bool WordIsDead = isUnusedBeforeOverwrite(*AddWord, Word, TRI);

      MachineInstrBuilder FirstLow =
          BuildMI(MBB, CopyLow, MIMetadata(*AddWord), TII.get(C166::ADDrr),
                  AddendLow)
              .addReg(AddendLow)
              .addReg(Word, getKillRegState(WordIsDead));
      FirstLow->setFlags(AddWord->getFlags());

      MachineInstrBuilder FirstHigh =
          BuildMI(MBB, CopyLow, MIMetadata(*AddWordCarry),
                  TII.get(C166::ADDCri3), AddendHigh)
              .addReg(AddendHigh)
              .addImm(0);
      markRegisterDefDead(*FirstHigh, C166::PSW, TRI);
      markRegisterDefDead(*FirstHigh, C166::C, TRI);
      FirstHigh->setFlags(AddWordCarry->getFlags());

      MachineInstrBuilder SecondLow =
          BuildMI(MBB, CopyLow, MIMetadata(*AddPair), TII.get(C166::ADDrr),
                  AddendLow)
              .addReg(AddendLow)
              .addReg(Carry);
      SecondLow->setFlags(AddPair->getFlags());

      MachineInstrBuilder SecondHigh =
          BuildMI(MBB, CopyLow, MIMetadata(*AddPairCarry),
                  TII.get(C166::ADDCri3), AddendHigh)
              .addReg(AddendHigh)
              .addImm(0);
      markRegisterDefDead(*SecondHigh, C166::PSW, TRI);
      markRegisterDefDead(*SecondHigh, C166::C, TRI);
      SecondHigh->getOperand(0).setIsDead(!PreserveHigh);
      SecondHigh->setFlags(AddPairCarry->getFlags());

      unsigned StoreSourceOperand = Store->getOpcode() == C166::MOVmr ? 1 : 2;
      Store->getOperand(StoreSourceOperand).setReg(AddendLow);
      Store->getOperand(StoreSourceOperand).setIsKill(true);

      auto Resume = std::next(Store);
      if (PreserveHigh) {
        MachineInstrBuilder CopyHigh = BuildMI(MBB, Resume, MIMetadata(*Store),
                                               TII.get(C166::MOVrr), DstHigh)
                                           .addReg(AddendHigh, RegState::Kill);
        markRegisterDefDead(*CopyHigh, C166::PSW, TRI);
      }

      CopyLow.eraseFromParent();
      ZeroHigh->eraseFromParent();
      AddWord->eraseFromParent();
      AddWordCarry->eraseFromParent();
      AddPair->eraseFromParent();
      AddPairCarry->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static bool
canDelayPhysicalLoad(MachineInstr &Load, MachineBasicBlock::iterator Insertion,
                     const SmallPtrSetImpl<MachineInstr *> &DelayedLoads,
                     Register Base, const TargetRegisterInfo &TRI,
                     bool AllowUnmarkedDeadPSW = false) {
  const MachineOperand *PSWDef = Load.findRegisterDefOperand(C166::PSW, &TRI);
  if (!PSWDef || (!PSWDef->isDead() && !AllowUnmarkedDeadPSW))
    return false;

  Register Destination = Load.getOperand(0).getReg();
  for (auto Scan = std::next(Load.getIterator()); Scan != Insertion; ++Scan) {
    if (DelayedLoads.contains(&*Scan))
      continue;
    if (Scan->isDebugInstr()) {
      if (readsPhysicalRegister(*Scan, Destination, TRI))
        return false;
      continue;
    }
    if (Scan->isMetaInstruction())
      continue;
    if (Scan->isCall() || Scan->isInlineAsm() || Scan->isTerminator() ||
        Scan->hasUnmodeledSideEffects() || !Scan->mayLoad() ||
        Scan->mayStore() || hasExplicitOrderedMemoryAccess(*Scan) ||
        Scan->modifiesRegister(Base, &TRI) ||
        Scan->modifiesRegister(Destination, &TRI) ||
        readsPhysicalRegister(*Scan, Destination, TRI) ||
        readsPhysicalRegister(*Scan, C166::PSW, TRI) ||
        readsPhysicalRegister(*Scan, C166::C, TRI))
      return false;
  }
  return true;
}

// Put two ascending parts of one contiguous word stream into address order.
// Register allocation can rotate aggregate loads when the high words need to
// stay in a particular register pair, for example 4, 6, 0, 2.  Delaying the
// high part exposes one post-increment chain without extending any value's
// live range.  Keep this deliberately narrow: only unordered loads may occur
// between the two parts, and every destination must be independent.
static bool orderSplitWordLoadChains(MachineFunction &MF,
                                     const C166InstrInfo &TII) {
  static constexpr unsigned MaxSpanInstructions = 32;
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      Register Base;
      int64_t FirstOffset;
      if (!getPhysicalLoadAddress(*I, Base, FirstOffset) || FirstOffset <= 0 ||
          TRI.regsOverlap(I->getOperand(0).getReg(), Base) ||
          isInsideExtensionWindow(MBB, I)) {
        ++I;
        continue;
      }

      SmallVector<MachineInstr *, 4> HighLoads{&*I};
      int64_t ExpectedHighOffset = FirstOffset + 2;
      MachineInstr *LowStart = nullptr;
      int64_t LowOffset = 0;
      unsigned Span = 0;
      for (auto Scan = std::next(I);
           Scan != MBB.end() && Span != MaxSpanInstructions; ++Scan) {
        if (Scan->isMetaInstruction())
          continue;
        ++Span;

        Register LoadBase;
        int64_t Offset;
        if (!getPhysicalLoadAddress(*Scan, LoadBase, Offset) ||
            LoadBase != Base)
          continue;
        if (TRI.regsOverlap(Scan->getOperand(0).getReg(), Base) ||
            isInsideExtensionWindow(MBB, Scan))
          break;
        if (Offset == ExpectedHighOffset) {
          HighLoads.push_back(&*Scan);
          ExpectedHighOffset += 2;
          continue;
        }
        if (Offset >= 0 && Offset < FirstOffset) {
          LowStart = &*Scan;
          LowOffset = Offset;
        }
        break;
      }
      if (!LowStart) {
        ++I;
        continue;
      }

      SmallVector<MachineInstr *, 4> LowLoads{LowStart};
      int64_t ExpectedLowOffset = LowOffset + 2;
      auto AfterLow = std::next(LowStart->getIterator());
      Span = 0;
      while (ExpectedLowOffset != FirstOffset && AfterLow != MBB.end() &&
             Span != MaxSpanInstructions) {
        MachineInstr &Candidate = *AfterLow++;
        if (Candidate.isMetaInstruction())
          continue;
        ++Span;

        Register LoadBase;
        int64_t Offset;
        if (!getPhysicalLoadAddress(Candidate, LoadBase, Offset) ||
            LoadBase != Base || Offset != ExpectedLowOffset)
          continue;
        if (TRI.regsOverlap(Candidate.getOperand(0).getReg(), Base) ||
            isInsideExtensionWindow(MBB, MachineBasicBlock::const_iterator(
                                             Candidate.getIterator())))
          break;
        LowLoads.push_back(&Candidate);
        ExpectedLowOffset += 2;
      }
      if (ExpectedLowOffset != FirstOffset) {
        ++I;
        continue;
      }

      unsigned OriginalSize = 0;
      for (MachineInstr *Load :
           llvm::concat<MachineInstr *>(LowLoads, HighLoads))
        OriginalSize += Load->getOpcode() == C166::MOVrm ? 2 : 4;
      unsigned AddressSize = LowOffset == 0 ? 2 : (LowOffset <= 15 ? 4 : 6);
      if (AddressSize + 2 * (LowLoads.size() + HighLoads.size()) >=
          OriginalSize) {
        ++I;
        continue;
      }

      auto Insertion = std::next(LowLoads.back()->getIterator());
      bool FlagsAreDead =
          MBB.computeRegisterLiveness(
              &TRI, C166::PSW, MachineBasicBlock::const_iterator(Insertion)) ==
              MachineBasicBlock::LQR_Dead &&
          MBB.computeRegisterLiveness(
              &TRI, C166::C, MachineBasicBlock::const_iterator(Insertion)) ==
              MachineBasicBlock::LQR_Dead;
      if (!FlagsAreDead) {
        ++I;
        continue;
      }
      SmallPtrSet<MachineInstr *, 4> DelayedLoads(HighLoads.begin(),
                                                  HighLoads.end());
      if (!llvm::all_of(HighLoads, [&](MachineInstr *Load) {
            return canDelayPhysicalLoad(*Load, Insertion, DelayedLoads, Base,
                                        TRI, true);
          })) {
        ++I;
        continue;
      }

      for (MachineInstr *Load : HighLoads)
        MBB.splice(Insertion, &MBB, Load->getIterator());
      for (MachineInstr *Load :
           llvm::concat<MachineInstr *>(LowLoads, HighLoads)) {
        markRegisterDefDead(*Load, C166::PSW, TRI);
        markRegisterDefDead(*Load, C166::C, TRI);
      }
      Changed = true;
      I = MBB.begin();
    }
  }

  return Changed;
}

static bool getSpillSlotAccess(const MachineInstr &MI,
                               const MachineFrameInfo &MFI, int &FrameIndex,
                               int64_t &Offset) {
  if (!MI.hasOneMemOperand())
    return false;

  const MachineMemOperand &MMO = **MI.memoperands_begin();
  const auto *Stack =
      dyn_cast_or_null<FixedStackPseudoSourceValue>(MMO.getPseudoValue());
  if (!Stack || !MFI.isSpillSlotObjectIndex(Stack->getFrameIndex()))
    return false;

  FrameIndex = Stack->getFrameIndex();
  Offset = MMO.getOffset();
  return true;
}

static bool accessesSpillSlot(const MachineInstr &MI, int FrameIndex) {
  return llvm::any_of(MI.memoperands(), [&](const MachineMemOperand *MMO) {
    const auto *Stack =
        dyn_cast_or_null<FixedStackPseudoSourceValue>(MMO->getPseudoValue());
    return Stack && Stack->getFrameIndex() == FrameIndex;
  });
}

static bool removeUnreadSpillWordStores(MachineFunction &MF,
                                        const C166InstrInfo &TII) {
  if (!MF.getFunction().hasOptSize())
    return false;

  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  bool Changed = false;

  // Reads do not change when an unread store is removed. Summarize once
  // instead of scanning the function for every candidate store.
  DenseSet<std::pair<int, int64_t>> ReadWords;
  DenseSet<int> UnknownSlots;
  for (const MachineBasicBlock &MBB : MF) {
    for (const MachineInstr &Access : MBB) {
      int FrameIndex;
      int64_t Offset;
      Register Base;
      int64_t AddressOffset;
      Register Source;
      if (getSpillSlotAccess(Access, MFI, FrameIndex, Offset) &&
          (**Access.memoperands_begin()).getSize() == LocationSize::precise(2) &&
          Offset % 2 == 0) {
        if (getPhysicalLoadAddress(Access, Base, AddressOffset) &&
            Base == C166::R0) {
          ReadWords.insert({FrameIndex, Offset});
          continue;
        }
        if (getPhysicalStoreAddress(Access, Base, AddressOffset, Source) &&
            Base == C166::R0)
          continue;
      }
      for (const MachineMemOperand *MMO : Access.memoperands())
        if (const auto *Stack = dyn_cast_or_null<FixedStackPseudoSourceValue>(
                MMO->getPseudoValue()))
          UnknownSlots.insert(Stack->getFrameIndex());
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Store = *I++;
      Register StoreBase;
      Register Source;
      int64_t StoreOffset;
      int FrameIndex;
      int64_t MemoryOffset;
      if (!getPhysicalStoreAddress(Store, StoreBase, StoreOffset, Source) ||
          StoreBase != C166::R0 ||
          !getSpillSlotAccess(Store, MFI, FrameIndex, MemoryOffset) ||
          hasExplicitOrderedMemoryAccess(Store) ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Store.getIterator())))
        continue;

      const MachineOperand *PSWDef =
          Store.findRegisterDefOperand(C166::PSW, &TRI);
      const MachineOperand *CarryDef =
          Store.findRegisterDefOperand(C166::C, &TRI);
      if ((PSWDef && !PSWDef->isDead() &&
           !TII.isRegisterOverwrittenBeforeUse(Store, C166::PSW)) ||
          (CarryDef && !CarryDef->isDead() &&
           !TII.isRegisterOverwrittenBeforeUse(Store, C166::C)))
        continue;

      if (UnknownSlots.contains(FrameIndex) ||
          ReadWords.contains({FrameIndex, MemoryOffset}))
        continue;

      Store.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

static bool isPhysicalWordCopy(const MachineInstr &MI, Register Destination,
                               Register Source) {
  return MI.getOpcode() == C166::MOVrr && MI.getOperand(0).isReg() &&
         MI.getOperand(0).getReg() == Destination &&
         MI.getOperand(0).getReg().isPhysical() &&
         !MI.getOperand(0).getSubReg() && MI.getOperand(1).isReg() &&
         MI.getOperand(1).getReg() == Source &&
         MI.getOperand(1).getReg().isPhysical() &&
         !MI.getOperand(1).getSubReg();
}

static bool callPreservesPhysicalRegister(const MachineInstr &Call,
                                          Register Reg,
                                          const TargetRegisterInfo &TRI) {
  bool HasRegisterMask = false;
  for (const MachineOperand &MO : Call.operands())
    HasRegisterMask |= MO.isRegMask();
  return HasRegisterMask && !Call.modifiesRegister(Reg, &TRI) &&
         !readsPhysicalRegister(Call, Reg, TRI);
}

static bool foldPreservedRegisterShuttles(MachineFunction &MF,
                                          const C166InstrInfo &TII) {
  static constexpr unsigned MaxSpanInstructions = 32;
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Store = *I++;
      Register StoreBase;
      Register Preserved;
      int64_t StoreOffset;
      int FrameIndex;
      int64_t MemoryOffset;
      if (!getPhysicalStoreAddress(Store, StoreBase, StoreOffset, Preserved) ||
          !getSpillSlotAccess(Store, MFI, FrameIndex, MemoryOffset) ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Store.getIterator())))
        continue;

      auto CopyIn = nextNonDebug(MBB, Store.getIterator());
      if (CopyIn == MBB.end() || CopyIn->getOpcode() != C166::MOVrr ||
          !CopyIn->getOperand(0).isReg() ||
          CopyIn->getOperand(0).getReg() != Preserved ||
          CopyIn->getOperand(0).getSubReg() || !CopyIn->getOperand(1).isReg() ||
          !CopyIn->getOperand(1).getReg().isPhysical() ||
          CopyIn->getOperand(1).getSubReg() ||
          isInsideExtensionWindow(MBB, CopyIn))
        continue;

      Register Live = CopyIn->getOperand(1).getReg();
      if (TRI.regsOverlap(Preserved, Live))
        continue;

      MachineInstr *CopyOut = nullptr;
      MachineInstr *Load = nullptr;
      bool SawCall = false;
      bool Invalid = false;
      unsigned Span = 0;
      for (auto Scan = std::next(CopyIn); Scan != MBB.end(); ++Scan) {
        if (Scan->isDebugInstr()) {
          if (readsPhysicalRegister(*Scan, Preserved, TRI) ||
              readsPhysicalRegister(*Scan, Live, TRI)) {
            Invalid = true;
            break;
          }
          continue;
        }
        if (Scan->isMetaInstruction())
          continue;
        if (++Span > MaxSpanInstructions)
          break;

        if (isPhysicalWordCopy(*Scan, Live, Preserved)) {
          if (!SawCall) {
            Invalid = true;
            break;
          }

          auto CandidateLoad = nextNonDebug(MBB, Scan);
          Register LoadBase;
          int64_t LoadOffset;
          int LoadFrameIndex;
          int64_t LoadMemoryOffset;
          if (CandidateLoad == MBB.end() ||
              !getPhysicalLoadAddress(*CandidateLoad, LoadBase, LoadOffset) ||
              CandidateLoad->getOperand(0).getReg() != Preserved ||
              CandidateLoad->getOperand(0).getSubReg() ||
              StoreBase != LoadBase || StoreOffset != LoadOffset ||
              TRI.regsOverlap(LoadBase, Preserved) ||
              TRI.regsOverlap(LoadBase, Live) ||
              !getSpillSlotAccess(*CandidateLoad, MFI, LoadFrameIndex,
                                  LoadMemoryOffset) ||
              LoadFrameIndex != FrameIndex ||
              LoadMemoryOffset != MemoryOffset ||
              isInsideExtensionWindow(MBB, Scan) ||
              isInsideExtensionWindow(MBB, CandidateLoad)) {
            Invalid = true;
            break;
          }

          for (auto Debug = std::next(Scan); Debug != CandidateLoad; ++Debug)
            if (Debug->isDebugInstr() &&
                (readsPhysicalRegister(*Debug, Preserved, TRI) ||
                 readsPhysicalRegister(*Debug, Live, TRI)))
              Invalid = true;
          if (Invalid)
            break;

          auto AfterLoad = nextNonDebug(MBB, CandidateLoad);
          auto IsDeadAfterLoad = [&](Register Reg) {
            return MBB.computeRegisterLiveness(
                       &TRI, Reg,
                       MachineBasicBlock::const_iterator(AfterLoad)) ==
                   MachineBasicBlock::LQR_Dead;
          };
          if (!IsDeadAfterLoad(C166::PSW) || !IsDeadAfterLoad(C166::C)) {
            Invalid = true;
            break;
          }

          CopyOut = &*Scan;
          Load = &*CandidateLoad;
          break;
        }

        if (Scan->isCall()) {
          SawCall = true;
          if (!callPreservesPhysicalRegister(*Scan, Preserved, TRI) ||
              accessesSpillSlot(*Scan, FrameIndex)) {
            Invalid = true;
            break;
          }
          continue;
        }

        if (Scan->isInlineAsm() || Scan->isTerminator() ||
            Scan->hasUnmodeledSideEffects() ||
            isExtensionOpcode(Scan->getOpcode()) ||
            readsPhysicalRegister(*Scan, Preserved, TRI) ||
            Scan->modifiesRegister(Preserved, &TRI) ||
            accessesSpillSlot(*Scan, FrameIndex)) {
          Invalid = true;
          break;
        }
      }

      if (Invalid || !CopyOut || !Load)
        continue;

      unsigned StoreSource = Store.getOpcode() == C166::MOVmr ? 1 : 2;
      Store.getOperand(StoreSource).setReg(Live);
      Store.getOperand(StoreSource).setIsKill(CopyIn->getOperand(1).isKill());
      copyPSWDefLiveness(Store, *CopyIn, TRI);
      Store.setFlags(Store.getFlags() | CopyIn->getFlags());

      Load->getOperand(0).setReg(Live);
      Load->getOperand(0).setIsDead(CopyOut->getOperand(0).isDead());
      markRegisterDefDead(*Load, C166::PSW, TRI);
      Load->setFlags(Load->getFlags() | CopyOut->getFlags());

      CopyIn->eraseFromParent();
      CopyOut->eraseFromParent();
      Changed = true;
      I = std::next(Store.getIterator());
    }
  }

  return Changed;
}

// A loop-carried word can be copied out of one half of a register pair before
// a loop and copied back immediately afterwards when subregister liveness is
// not tracked. Update the pair half in place when the loop has no other use of
// its old value.
static bool foldLoopRegisterShuttles(MachineFunction &MF,
                                     const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  bool Changed = false;

  for (MachineBasicBlock &Preheader : MF) {
    if (Preheader.succ_size() != 1)
      continue;
    MachineBasicBlock &Header = **Preheader.succ_begin();
    if (&Header == &Preheader || Header.pred_size() != 2 ||
        !llvm::is_contained(Header.predecessors(), &Preheader) ||
        !llvm::is_contained(Header.predecessors(), &Header) ||
        Header.succ_size() != 2 ||
        !llvm::is_contained(Header.successors(), &Header))
      continue;

    MachineBasicBlock *Exit = nullptr;
    for (MachineBasicBlock *Successor : Header.successors())
      if (Successor != &Header)
        Exit = Successor;
    if (!Exit || Exit->pred_size() != 1 || *Exit->pred_begin() != &Header)
      continue;

    auto CopyIn = Preheader.getFirstTerminator();
    while (CopyIn != Preheader.begin()) {
      --CopyIn;
      if (!CopyIn->isDebugInstr() && !CopyIn->isMetaInstruction())
        break;
    }
    if (CopyIn == Preheader.end() || CopyIn->isTerminator() ||
        CopyIn->getOpcode() != C166::MOVrr)
      continue;

    auto CopyOut = Exit->begin();
    while (CopyOut != Exit->end() &&
           (CopyOut->isDebugInstr() || CopyOut->isMetaInstruction()))
      ++CopyOut;
    if (CopyOut == Exit->end() || CopyOut->getOpcode() != C166::MOVrr)
      continue;

    const MachineOperand &CopyInDestination = CopyIn->getOperand(0);
    const MachineOperand &CopyInSource = CopyIn->getOperand(1);
    const MachineOperand &CopyOutDestination = CopyOut->getOperand(0);
    const MachineOperand &CopyOutSource = CopyOut->getOperand(1);
    if (!isWholePhysicalRegister(CopyInDestination) ||
        !isWholePhysicalRegister(CopyInSource) ||
        !isWholePhysicalRegister(CopyOutDestination) ||
        !isWholePhysicalRegister(CopyOutSource))
      continue;

    Register Temporary = CopyInDestination.getReg();
    Register Value = CopyInSource.getReg();
    if (Temporary == Value || TRI.regsOverlap(Temporary, Value) ||
        CopyOutDestination.getReg() != Value ||
        CopyOutSource.getReg() != Temporary ||
        !TII.isRegisterOverwrittenBeforeUse(*CopyIn, C166::PSW) ||
        !TII.isRegisterOverwrittenBeforeUse(*CopyOut, C166::PSW) ||
        isInsideExtensionWindow(Preheader, CopyIn) ||
        isInsideExtensionWindow(*Exit, CopyOut))
      continue;

    bool Invalid = false;
    bool SawUse = false;
    bool SawDef = false;
    auto IsOtherHalfSuperregisterOperand = [&](const MachineInstr &MI,
                                               const MachineOperand &MO) {
      if (!MO.isImplicit() || !MO.getReg() ||
          !TRI.regsOverlap(MO.getReg(), Value))
        return false;
      return llvm::any_of(MI.operands(), [&](const MachineOperand &Other) {
        return Other.isReg() && !Other.isImplicit() && Other.getReg() &&
               TRI.regsOverlap(Other.getReg(), MO.getReg()) &&
               !TRI.regsOverlap(Other.getReg(), Value);
      });
    };

    for (MachineInstr &MI : Header) {
      if (MI.isDebugInstr()) {
        if (readsPhysicalRegister(MI, Temporary, TRI) ||
            readsPhysicalRegister(MI, Value, TRI))
          Invalid = true;
        continue;
      }
      if (MI.isMetaInstruction())
        continue;
      if (MI.isCall() || MI.isInlineAsm() || MI.hasUnmodeledSideEffects() ||
          isExtensionOpcode(MI.getOpcode())) {
        Invalid = true;
        break;
      }

      for (MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.getReg())
          continue;
        if (MO.getReg() == Temporary && !MO.getSubReg()) {
          SawUse |= MO.isUse();
          SawDef |= MO.isDef();
          continue;
        }
        if (TRI.regsOverlap(MO.getReg(), Temporary) ||
            (TRI.regsOverlap(MO.getReg(), Value) &&
             !IsOtherHalfSuperregisterOperand(MI, MO))) {
          Invalid = true;
          break;
        }
      }
      if (Invalid)
        break;
    }
    if (Invalid || !SawUse || !SawDef)
      continue;

    for (MachineBasicBlock &MBB : MF) {
      if (&MBB == &Header)
        continue;
      for (MachineInstr &MI : MBB) {
        if (&MI == &*CopyIn || &MI == &*CopyOut || MI.isMetaInstruction())
          continue;
        if (readsPhysicalRegister(MI, Temporary, TRI) ||
            MI.modifiesRegister(Temporary, &TRI)) {
          Invalid = true;
          break;
        }
      }
      if (Invalid)
        break;
    }
    if (Invalid)
      continue;

    for (MachineInstr &MI : Header)
      for (MachineOperand &MO : MI.operands())
        if (MO.isReg() && MO.getReg() == Temporary && !MO.getSubReg())
          MO.setReg(Value);

    Header.removeLiveIn(Temporary);
    Exit->removeLiveIn(Temporary);
    MRI.clearKillFlags(Value);
    CopyIn->eraseFromParent();
    CopyOut->eraseFromParent();
    Changed = true;
  }

  return Changed;
}

static bool forwardLiveSpillReloads(MachineFunction &MF,
                                    const C166InstrInfo &TII) {
  if (!MF.getFunction().hasOptSize())
    return false;

  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineDominatorTree MDT;
  MDT.recalculate(MF);
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Store = *I++;
      Register StoreBase;
      Register Source;
      int64_t StoreOffset;
      int FrameIndex;
      int64_t MemoryOffset;
      if ((Store.getOpcode() != C166::MOVmr &&
           Store.getOpcode() != C166::MOVmr16) ||
          !getPhysicalStoreAddress(Store, StoreBase, StoreOffset, Source) ||
          StoreBase != C166::R0 || Source == C166::R0 ||
          !getSpillSlotAccess(Store, MFI, FrameIndex, MemoryOffset) ||
          MFI.getObjectSize(FrameIndex) != 2 ||
          hasExplicitOrderedMemoryAccess(Store) ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Store.getIterator())))
        continue;

      const MachineOperand *StorePSW =
          Store.findRegisterDefOperand(C166::PSW, &TRI);
      if (StorePSW && !StorePSW->isDead() &&
          !TII.isRegisterOverwrittenBeforeUse(Store, C166::PSW))
        continue;

      SmallVector<MachineInstr *, 4> Loads;
      SmallPtrSet<MachineInstr *, 4> UnavailableLoads;
      SmallPtrSet<MachineBasicBlock *, 8> VisitedAvailable;
      SmallPtrSet<MachineBasicBlock *, 8> VisitedUnavailable;
      SmallVector<std::pair<MachineBasicBlock *, bool>, 8> Worklist;
      DenseMap<MachineBasicBlock *, SmallVector<MachineBasicBlock *, 2>>
          AvailablePredecessors;
      bool Invalid = false;

      auto Scan = [&](MachineBasicBlock &Block,
                      MachineBasicBlock::iterator Begin, bool Available) {
        for (auto Current = Begin; Current != Block.end(); ++Current) {
          if (Current->isDebugInstr() || Current->isMetaInstruction())
            continue;

          if (accessesSpillSlot(*Current, FrameIndex)) {
            if ((Current->getOpcode() == C166::MOVmr ||
                 Current->getOpcode() == C166::MOVmr16) &&
                !hasExplicitOrderedMemoryAccess(*Current)) {
              Available = false;
              continue;
            }

            Register LoadBase;
            int64_t LoadOffset;
            if ((Current->getOpcode() != C166::MOVrm &&
                 Current->getOpcode() != C166::MOVrm16) ||
                !getPhysicalLoadAddress(*Current, LoadBase, LoadOffset) ||
                LoadBase != StoreBase || LoadOffset != StoreOffset ||
                !Current->getOperand(0).isReg() ||
                !Current->getOperand(0).getReg().isPhysical() ||
                Current->getOperand(0).getSubReg() ||
                hasExplicitOrderedMemoryAccess(*Current) ||
                isInsideExtensionWindow(
                    Block, MachineBasicBlock::const_iterator(Current))) {
              Invalid = true;
              return false;
            }
            if (!Available)
              UnavailableLoads.insert(&*Current);
            else if (!llvm::is_contained(Loads, &*Current))
              Loads.push_back(&*Current);
          }

          if (Current->isInlineAsm() || Current->hasUnmodeledSideEffects()) {
            Invalid = true;
            return false;
          }
          if (Current->isCall())
            Available &= callPreservesPhysicalRegister(*Current, Source, TRI);
          else if (Current->modifiesRegister(Source, &TRI))
            Available = false;
        }

        for (MachineBasicBlock *Successor : Block.successors()) {
          if (Available)
            AvailablePredecessors[Successor].push_back(&Block);
          Worklist.emplace_back(Successor, Available);
        }
        return true;
      };

      if (!Scan(MBB, I, true) || Invalid)
        continue;
      while (!Worklist.empty() && !Invalid) {
        auto [Block, Available] = Worklist.pop_back_val();
        auto &Visited = Available ? VisitedAvailable : VisitedUnavailable;
        if (!Visited.insert(Block).second)
          continue;
        Scan(*Block, Block->begin(), Available);
      }
      llvm::erase_if(Loads, [&](MachineInstr *Load) {
        return UnavailableLoads.contains(Load) ||
               !MDT.dominates(&MBB, Load->getParent());
      });
      if (Invalid || Loads.empty())
        continue;

      MRI.clearKillFlags(Source);
      SmallPtrSet<MachineBasicBlock *, 8> NeededBlocks;
      SmallVector<MachineBasicBlock *, 8> NeededWorklist;
      for (MachineInstr *Load : Loads)
        NeededWorklist.push_back(Load->getParent());
      while (!NeededWorklist.empty()) {
        MachineBasicBlock *Block = NeededWorklist.pop_back_val();
        if (Block == &MBB || !NeededBlocks.insert(Block).second)
          continue;
        llvm::append_range(NeededWorklist, AvailablePredecessors[Block]);
      }
      for (MachineBasicBlock *Block : NeededBlocks)
        if (!isLiveInWithAliases(*Block, Source, TRI))
          Block->addLiveIn(Source);

      for (MachineInstr *Load : Loads) {
        Register Destination = Load->getOperand(0).getReg();
        MachineInstrBuilder Copy =
            BuildMI(*Load->getParent(), *Load, MIMetadata(*Load),
                    TII.get(C166::MOVrr), Destination)
                .addReg(Source);
        Copy->getOperand(0).setIsDead(Load->getOperand(0).isDead());
        copyPSWDefLiveness(*Copy, *Load, TRI);
        Copy->setFlags(Load->getFlags());
        Load->eraseFromParent();
      }
      // Forwarding a subset of the loads does not make the store dead.
      // The spill-store cleanup can remove it once no reads remain.
      I = std::next(Store.getIterator());
      Changed = true;
    }
  }

  return Changed;
}

// A LIFO spill sequence around a register-only call can use the user stack
// directly.  This is smaller than displaced frame accesses, but it changes R0
// temporarily, so keep frame/debug information and outgoing stack arguments
// out of the transformed region.
static bool foldCallSpillSequences(MachineFunction &MF,
                                   const C166InstrInfo &TII) {
  if (!MF.getFunction().hasOptSize() || MF.needsFrameMoves() ||
      MF.getFunction().getCallingConv() == CallingConv::C166_Interrupt)
    return false;

  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  bool Changed = false;

  DenseMap<int, unsigned> SlotReads;
  for (const MachineBasicBlock &MBB : MF)
    for (const MachineInstr &MI : MBB)
      if (MI.mayLoad())
        for (const MachineMemOperand *MMO : MI.memoperands())
          if (const auto *Stack = dyn_cast_or_null<FixedStackPseudoSourceValue>(
                  MMO->getPseudoValue()))
            ++SlotReads[Stack->getFrameIndex()];

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      SmallVector<MachineInstr *, 4> Stores;
      SmallVector<std::pair<int, int64_t>, 4> Slots;
      SmallVector<Register, 4> Registers;
      unsigned OriginalSize = 0;
      auto Scan = I;
      while (Scan != MBB.end()) {
        Register Base;
        Register Source;
        int64_t Offset;
        int FrameIndex;
        int64_t MemoryOffset;
        if (!getPhysicalStoreAddress(*Scan, Base, Offset, Source) ||
            Base != C166::R0 || Source == C166::R0 ||
            !getSpillSlotAccess(*Scan, MFI, FrameIndex, MemoryOffset) ||
            hasExplicitOrderedMemoryAccess(*Scan) ||
            isInsideExtensionWindow(MBB, Scan))
          break;
        Stores.push_back(&*Scan);
        Slots.emplace_back(FrameIndex, MemoryOffset);
        Registers.push_back(Source);
        OriginalSize += Scan->getOpcode() == C166::MOVmr ? 2 : 4;
        Scan = nextNonDebug(MBB, Scan);
      }
      if (Stores.empty()) {
        ++I;
        continue;
      }

      if (Scan == MBB.end() || !Scan->isCall() ||
          Scan->modifiesRegister(C166::R0, &TRI) ||
          isInsideExtensionWindow(MBB, Scan)) {
        I = std::next(Stores.back()->getIterator());
        continue;
      }
      MachineInstr &Call = *Scan;
      bool HasRegisterMask =
          llvm::any_of(Call.operands(),
                       [](const MachineOperand &MO) { return MO.isRegMask(); });
      if (!HasRegisterMask) {
        I = std::next(Stores.back()->getIterator());
        continue;
      }

      bool HasOutgoingStackArguments = false;
      for (auto Previous = previousNonDebug(MBB, Stores.front()->getIterator());
           Previous != MBB.end(); Previous = previousNonDebug(MBB, Previous)) {
        if (Previous->isCall() || Previous->isTerminator())
          break;
        if (Previous->getOpcode() == C166::PUSHARG ||
            Previous->getOpcode() == C166::ALLOCSP) {
          HasOutgoingStackArguments = true;
          break;
        }
      }
      if (HasOutgoingStackArguments) {
        I = std::next(Stores.back()->getIterator());
        continue;
      }

      SmallVector<MachineInstr *, 4> Loads;
      Scan = nextNonDebug(MBB, Call.getIterator());
      bool Matches = true;
      for (unsigned Index = Stores.size(); Index != 0; --Index) {
        Register Base;
        int64_t Offset;
        int FrameIndex;
        int64_t MemoryOffset;
        if (Scan == MBB.end() || !getPhysicalLoadAddress(*Scan, Base, Offset) ||
            Base != C166::R0 || !isWholePhysicalRegister(Scan->getOperand(0)) ||
            Scan->getOperand(0).getReg() == C166::R0 ||
            !getSpillSlotAccess(*Scan, MFI, FrameIndex, MemoryOffset) ||
            hasExplicitOrderedMemoryAccess(*Scan) ||
            isInsideExtensionWindow(MBB, Scan) ||
            std::pair(FrameIndex, MemoryOffset) != Slots[Index - 1] ||
            Scan->getOperand(0).getReg() != Registers[Index - 1]) {
          Matches = false;
          break;
        }
        Loads.push_back(&*Scan);
        OriginalSize += Scan->getOpcode() == C166::MOVrm ? 2 : 4;
        Scan = nextNonDebug(MBB, Scan);
      }
      HasOutgoingStackArguments |=
          Scan != MBB.end() && Scan->getOpcode() == C166::ADJSP;
      if (!Matches || HasOutgoingStackArguments ||
          4 * Stores.size() >= OriginalSize) {
        I = std::next(Stores.back()->getIterator());
        continue;
      }
      // Push/pop preserves registers, not the original frame slots. Reject
      // the replacement if any other instruction can read these slots.
      DenseMap<int, unsigned> MatchedReads;
      for (auto [FrameIndex, Offset] : Slots)
        ++MatchedReads[FrameIndex];
      if (llvm::any_of(MatchedReads, [&](const auto &Entry) {
            return SlotReads.lookup(Entry.first) != Entry.second;
          })) {
        I = std::next(Stores.back()->getIterator());
        continue;
      }
      bool HasDebugInstruction = false;
      for (auto Debug = Stores.front()->getIterator(); Debug != Scan; ++Debug)
        HasDebugInstruction |= Debug->isDebugInstr();
      if (HasDebugInstruction) {
        I = std::next(Stores.back()->getIterator());
        continue;
      }

      for (MachineInstr *Store : Stores) {
        unsigned SourceOperand = Store->getOpcode() == C166::MOVmr ? 1 : 2;
        MachineOperand Source = Store->getOperand(SourceOperand);
        MachineInstrBuilder Push = BuildMI(MBB, Call, MIMetadata(*Store),
                                           TII.get(C166::MOVmrPreDec), C166::R0)
                                       .addReg(C166::R0)
                                       .add(Source);
        copyPSWDefLiveness(*Push, *Store, TRI);
        Push->setFlags(Store->getFlags());
      }
      for (MachineInstr *Load : Loads) {
        MachineOperand Destination = Load->getOperand(0);
        MachineInstrBuilder Pop =
            BuildMI(MBB, *Load, MIMetadata(*Load), TII.get(C166::MOVrmPostInc),
                    Destination.getReg())
                .addReg(C166::R0, RegState::Define)
                .addReg(C166::R0);
        Pop->getOperand(0).setIsDead(Destination.isDead());
        copyPSWDefLiveness(*Pop, *Load, TRI);
        Pop->setFlags(Load->getFlags());
      }

      auto Resume = Scan;
      for (const auto &[FrameIndex, Count] : MatchedReads)
        SlotReads[FrameIndex] -= Count;
      for (MachineInstr *Store : Stores)
        Store->eraseFromParent();
      for (MachineInstr *Load : Loads)
        Load->eraseFromParent();
      Changed = true;
      I = Resume;
    }
  }

  return Changed;
}

static bool forwardAdjacentWordStoreLoads(MachineFunction &MF,
                                          const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Store = *I++;
      Register StoreBase;
      Register Source;
      int64_t StoreOffset;
      if (!getPhysicalStoreAddress(Store, StoreBase, StoreOffset, Source) ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Store.getIterator())))
        continue;

      auto Load = nextNonDebug(MBB, Store.getIterator());
      Register LoadBase;
      int64_t LoadOffset;
      if (Load == MBB.end() ||
          !getPhysicalLoadAddress(*Load, LoadBase, LoadOffset) ||
          Load->getOperand(0).getSubReg() || Load->getOperand(1).getSubReg() ||
          StoreBase != LoadBase || StoreOffset != LoadOffset ||
          isInsideExtensionWindow(MBB, Load))
        continue;

      Register Destination = Load->getOperand(0).getReg();
      if (Destination != Source && TRI.regsOverlap(Destination, Source))
        continue;
      if (Destination != Source && Load->getOpcode() == C166::MOVrm)
        continue;

      unsigned SourceOperand = Store.getOpcode() == C166::MOVmr ? 1 : 2;
      auto Resume = std::next(Load);
      if (Destination == Source) {
        if (!Load->getOperand(0).isDead())
          Store.getOperand(SourceOperand).setIsKill(false);
        copyPSWDefLiveness(Store, *Load, TRI);
        Store.setFlags(Store.getFlags() | Load->getFlags());
      } else {
        Store.getOperand(SourceOperand).setIsKill(false);
        bool SourceIsDead =
            MBB.computeRegisterLiveness(
                &TRI, Source, MachineBasicBlock::const_iterator(Resume)) ==
            MachineBasicBlock::LQR_Dead;
        MachineInstrBuilder Copy =
            BuildMI(MBB, *Load, MIMetadata(*Load), TII.get(C166::MOVrr),
                    Destination)
                .addReg(Source, getKillRegState(SourceIsDead));
        Copy->getOperand(0).setIsDead(Load->getOperand(0).isDead());
        copyPSWDefLiveness(*Copy, *Load, TRI);
        Copy->setFlags(Load->getFlags());
      }

      Load->eraseFromParent();
      Changed = true;
      I = Resume;
    }
  }

  return Changed;
}

static bool formSpacedPostIncrementLoadChains(MachineFunction &MF,
                                              const C166InstrInfo &TII) {
  static constexpr unsigned MaxSpanInstructions = 32;
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      Register Base;
      int64_t FirstOffset;
      if (!getPhysicalLoadAddress(*I, Base, FirstOffset) ||
          TRI.regsOverlap(I->getOperand(0).getReg(), Base) ||
          isInsideExtensionWindow(MBB, I)) {
        ++I;
        continue;
      }

      SmallVector<MachineInstr *, 4> Loads{&*I};
      unsigned OriginalSize = I->getOpcode() == C166::MOVrm ? 2 : 4;
      int64_t ExpectedOffset = FirstOffset + 2;
      auto Last = I;
      unsigned Span = 0;
      for (auto Scan = std::next(I);
           Scan != MBB.end() && Span != MaxSpanInstructions; ++Scan) {
        if (Scan->isMetaInstruction())
          continue;
        ++Span;
        if (Scan->isCall() || Scan->isInlineAsm() || Scan->isTerminator() ||
            Scan->hasUnmodeledSideEffects() ||
            Scan->modifiesRegister(Base, &TRI))
          break;

        Register LoadBase;
        int64_t Offset;
        if (!getPhysicalLoadAddress(*Scan, LoadBase, Offset) ||
            LoadBase != Base || Offset != ExpectedOffset)
          continue;

        Loads.push_back(&*Scan);
        OriginalSize += Scan->getOpcode() == C166::MOVrm ? 2 : 4;
        ExpectedOffset += 2;
        Last = Scan;
      }

      unsigned AddressSize = FirstOffset == 0 ? 2 : (FirstOffset <= 15 ? 4 : 6);
      unsigned ReplacementSize = AddressSize + 2 * Loads.size();
      if (ReplacementSize >= OriginalSize ||
          (FirstOffset &&
           MBB.computeRegisterLiveness(&TRI, C166::C,
                                       MachineBasicBlock::const_iterator(I)) !=
               MachineBasicBlock::LQR_Dead)) {
        ++I;
        continue;
      }

      Register Scratch = findPostRAScratch(MBB, I, Last, TRI);
      if (!Scratch) {
        for (unsigned Split = 1; Split != Loads.size(); ++Split) {
          auto Insertion = Loads[Split]->getIterator();
          if (isInsideExtensionWindow(
                  MBB, MachineBasicBlock::const_iterator(Insertion)) ||
              (FirstOffset &&
               MBB.computeRegisterLiveness(
                   &TRI, C166::C,
                   MachineBasicBlock::const_iterator(Insertion)) !=
                   MachineBasicBlock::LQR_Dead))
            continue;

          Register LateScratch = findPostRAScratch(MBB, Insertion, Last, TRI);
          if (!LateScratch)
            continue;

          SmallPtrSet<MachineInstr *, 4> DelayedLoads;
          DelayedLoads.insert(Loads.begin(), Loads.begin() + Split);
          if (!llvm::all_of(ArrayRef(Loads).take_front(Split),
                            [&](MachineInstr *Load) {
                              return canDelayPhysicalLoad(
                                  *Load, Insertion, DelayedLoads, Base, TRI);
                            }))
            continue;

          for (MachineInstr *Load : ArrayRef(Loads).take_front(Split))
            MBB.splice(Insertion, &MBB, Load->getIterator());
          I = Loads.front()->getIterator();
          Scratch = LateScratch;
          break;
        }
        if (!Scratch) {
          ++I;
          continue;
        }
      }

      if (FirstOffset >= 8 && FirstOffset <= 15) {
        MachineInstrBuilder Immediate =
            BuildMI(MBB, *I, MIMetadata(*I), TII.get(C166::MOVri4), Scratch)
                .addImm(FirstOffset);
        markRegisterDefDead(*Immediate, C166::PSW, TRI);
        MachineInstrBuilder Address =
            BuildMI(MBB, *I, MIMetadata(*I), TII.get(C166::ADDrr), Scratch)
                .addReg(Scratch, RegState::Kill)
                .addReg(Base);
        markRegisterDefDead(*Address, C166::PSW, TRI);
        markRegisterDefDead(*Address, C166::C, TRI);
      } else {
        MachineInstrBuilder Copy =
            BuildMI(MBB, *I, MIMetadata(*I), TII.get(C166::MOVrr), Scratch)
                .addReg(Base);
        markRegisterDefDead(*Copy, C166::PSW, TRI);
        if (FirstOffset) {
          MachineInstrBuilder Address =
              BuildMI(MBB, *I, MIMetadata(*I),
                      TII.get(FirstOffset <= 7 ? C166::ADDri3 : C166::ADDri16),
                      Scratch)
                  .addReg(Scratch, RegState::Kill)
                  .addImm(FirstOffset);
          markRegisterDefDead(*Address, C166::PSW, TRI);
          markRegisterDefDead(*Address, C166::C, TRI);
        }
      }

      for (auto [Index, Load] : llvm::enumerate(Loads)) {
        Register Destination = Load->getOperand(0).getReg();
        MachineInstrBuilder Replacement;
        if (Index + 1 == Loads.size()) {
          Replacement = BuildMI(MBB, *Load, MIMetadata(*Load),
                                TII.get(C166::MOVrm), Destination)
                            .addReg(Scratch, RegState::Kill);
        } else {
          Replacement = BuildMI(MBB, *Load, MIMetadata(*Load),
                                TII.get(C166::MOVrmPostInc), Destination)
                            .addDef(Scratch)
                            .addReg(Scratch, RegState::Kill);
        }
        Replacement->getOperand(0).setIsDead(Load->getOperand(0).isDead());
        Replacement.cloneMemRefs(*Load).setMIFlags(Load->getFlags());
        copyPSWDefLiveness(*Replacement, *Load, TRI);
      }

      for (MachineInstr *Load : Loads)
        Load->eraseFromParent();
      Changed = true;
      // A folded stream may have been interleaved with another contiguous
      // stream whose first accesses are now before the old resume point.
      // Rescan the block; post-increment forms do not match this fold again.
      I = MBB.begin();
    }
  }

  return Changed;
}

static unsigned stackAdjustmentSize(uint64_t Amount) {
  if (!Amount)
    return 0;
  return Amount <= 7 ? 2 : 4;
}

static bool foldFrameSetupStoreChain(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator AfterStores, ArrayRef<MachineInstr *> Stores,
    ArrayRef<std::pair<int64_t, MachineInstr *>> StoresByOffset,
    uint64_t StoresSize, const C166InstrInfo &TII,
    const TargetRegisterInfo &TRI) {
  if (MF.needsFrameMoves() || Stores.empty())
    return false;

  auto Adjust = previousNonDebug(MBB, Stores.front()->getIterator());
  if (Adjust == MBB.end() ||
      std::next(Adjust) != Stores.front()->getIterator() ||
      !Adjust->getFlag(MachineInstr::FrameSetup) ||
      (Adjust->getOpcode() != C166::SUBri3 &&
       Adjust->getOpcode() != C166::SUBri16) ||
      !Adjust->getOperand(0).isReg() ||
      Adjust->getOperand(0).getReg() != C166::R0 ||
      !Adjust->getOperand(1).isReg() ||
      Adjust->getOperand(1).getReg() != C166::R0 ||
      !Adjust->getOperand(2).isImm())
    return false;

  int64_t FrameSize = Adjust->getOperand(2).getImm();
  int64_t FirstOffset = StoresByOffset.front().first;
  int64_t EndOffset = StoresByOffset.back().first + 2;
  if (FrameSize <= 0 || FirstOffset < 0 || EndOffset > FrameSize ||
      ((FrameSize | FirstOffset | EndOffset) & 1))
    return false;

  uint64_t TopGap = FrameSize - EndOffset;
  uint64_t BottomGap = FirstOffset;
  // Allocate the gap above the stores, push the words from high to low, then
  // allocate the remaining gap. The final R0 and every store address are
  // identical to the original full adjustment plus displaced stores.
  uint64_t OriginalSize = TII.getInstSizeInBytes(*Adjust) + StoresSize;
  uint64_t ReplacementSize = stackAdjustmentSize(TopGap) + 2 * Stores.size() +
                             stackAdjustmentSize(BottomGap);
  if (ReplacementSize >= OriginalSize)
    return false;

  auto IsDead = [&](Register Reg) {
    return MBB.computeRegisterLiveness(
               &TRI, Reg, MachineBasicBlock::const_iterator(AfterStores)) ==
           MachineBasicBlock::LQR_Dead;
  };
  if (!IsDead(C166::PSW) || !IsDead(C166::C))
    return false;

  auto EmitAdjustment = [&](uint64_t Amount) {
    if (!Amount)
      return;
    MachineInstrBuilder MIB =
        BuildMI(MBB, *Adjust, MIMetadata(*Adjust),
                TII.get(Amount <= 7 ? C166::SUBri3 : C166::SUBri16), C166::R0)
            .addReg(C166::R0)
            .addImm(Amount);
    MIB->setFlags(Adjust->getFlags());
    markRegisterDefDead(*MIB, C166::PSW, TRI);
    markRegisterDefDead(*MIB, C166::C, TRI);
  };

  EmitAdjustment(TopGap);
  for (auto Entry : llvm::reverse(StoresByOffset)) {
    MachineInstr *Store = Entry.second;
    unsigned SourceOperand = Store->getOpcode() == C166::MOVmr ? 1 : 2;
    MachineOperand Source = Store->getOperand(SourceOperand);
    Source.setIsKill(false);
    MachineInstrBuilder Replacement =
        BuildMI(MBB, *Adjust, MIMetadata(*Store), TII.get(C166::MOVmrPreDec),
                C166::R0)
            .addReg(C166::R0)
            .add(Source);
    Replacement.cloneMemRefs(*Store);
    Replacement->setFlags(Store->getFlags() | MachineInstr::FrameSetup);
    markRegisterDefDead(*Replacement, C166::PSW, TRI);
  }
  EmitAdjustment(BottomGap);

  Adjust->eraseFromParent();
  for (MachineInstr *Store : Stores)
    Store->eraseFromParent();
  return true;
}

// Reuse a following address materialization as the cursor for contiguous
// stores.  Restoring the cursor after pre-decrement stores preserves the value
// expected by its later users without changing R0 across a call.
static bool foldStoresIntoFollowingAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator AddressSetup,
    ArrayRef<MachineInstr *> Stores,
    ArrayRef<std::pair<int64_t, MachineInstr *>> StoresByOffset,
    uint64_t StoresSize, Register Base, const C166InstrInfo &TII,
    const TargetRegisterInfo &TRI) {
  if (Stores.size() < 2 ||
      AddressSetup != std::next(Stores.back()->getIterator()) ||
      AddressSetup == MBB.end() || AddressSetup->getOpcode() != C166::MOVri4 ||
      !isWholePhysicalRegister(AddressSetup->getOperand(0)) ||
      !AddressSetup->getOperand(1).isImm())
    return false;

  Register Cursor = AddressSetup->getOperand(0).getReg();
  int64_t EndOffset = StoresByOffset.back().first + 2;
  if (AddressSetup->getOperand(1).getImm() != EndOffset ||
      TRI.regsOverlap(Cursor, Base))
    return false;

  auto Address = nextNonDebug(MBB, AddressSetup);
  if (Address == MBB.end() || Address != std::next(AddressSetup) ||
      Address->getOpcode() != C166::ADDrr || !Address->getOperand(0).isReg() ||
      Address->getOperand(0).getReg() != Cursor ||
      !Address->getOperand(1).isReg() ||
      Address->getOperand(1).getReg() != Cursor ||
      !Address->getOperand(2).isReg() ||
      Address->getOperand(2).getReg() != Base ||
      isInsideExtensionWindow(MBB, Address))
    return false;

  for (auto I = Stores.front()->getIterator(); I != std::next(Address); ++I)
    if (I->isDebugInstr())
      return false;

  for (MachineInstr *Store : Stores) {
    unsigned SourceOperand = Store->getOpcode() == C166::MOVmr ? 1 : 2;
    if (TRI.regsOverlap(Store->getOperand(SourceOperand).getReg(), Cursor))
      return false;
  }

  auto AfterAddress = nextNonDebug(MBB, Address);
  auto IsDead = [&](Register Reg) {
    return MBB.computeRegisterLiveness(
               &TRI, Reg, MachineBasicBlock::const_iterator(AfterAddress)) ==
           MachineBasicBlock::LQR_Dead;
  };
  if (!IsDead(C166::PSW) || !IsDead(C166::C))
    return false;

  uint64_t RestoreAmount = 2 * Stores.size();
  uint64_t ReplacementStoreSize =
      2 * Stores.size() + stackAdjustmentSize(RestoreAmount);
  if (ReplacementStoreSize >= StoresSize)
    return false;

  AddressSetup->getOperand(0).setIsDead(false);
  Address->getOperand(0).setIsDead(false);
  markRegisterDefDead(*AddressSetup, C166::PSW, TRI);
  markRegisterDefDead(*Address, C166::PSW, TRI);
  markRegisterDefDead(*Address, C166::C, TRI);
  MBB.splice(Stores.front()->getIterator(), &MBB, AddressSetup);
  MBB.splice(Stores.front()->getIterator(), &MBB, Address);

  for (auto Entry : llvm::reverse(StoresByOffset)) {
    MachineInstr *Store = Entry.second;
    unsigned SourceOperand = Store->getOpcode() == C166::MOVmr ? 1 : 2;
    MachineOperand Source = Store->getOperand(SourceOperand);
    Source.setIsKill(false);
    MachineInstrBuilder Replacement =
        BuildMI(MBB, *Stores.front(), MIMetadata(*Store),
                TII.get(C166::MOVmrPreDec), Cursor)
            .addReg(Cursor, RegState::Kill)
            .add(Source);
    Replacement.cloneMemRefs(*Store).setMIFlags(Store->getFlags());
    markRegisterDefDead(*Replacement, C166::PSW, TRI);
  }

  MachineInstrBuilder Restore =
      BuildMI(MBB, *Stores.front(), MIMetadata(*Address),
              TII.get(RestoreAmount <= 7 ? C166::ADDri3 : C166::ADDri16),
              Cursor)
          .addReg(Cursor, RegState::Kill)
          .addImm(RestoreAmount);
  Restore->setFlags(Address->getFlags());
  Restore->clearFlag(MachineInstr::NoSWrap);
  Restore->clearFlag(MachineInstr::NoUWrap);
  markRegisterDefDead(*Restore, C166::PSW, TRI);
  markRegisterDefDead(*Restore, C166::C, TRI);

  for (MachineInstr *Store : Stores)
    Store->eraseFromParent();
  return true;
}

static bool formPreDecrementStoreChains(MachineFunction &MF,
                                        const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      if ((I->getOpcode() != C166::MOVmr && I->getOpcode() != C166::MOVmr16) ||
          hasExplicitOrderedMemoryAccess(*I) || !I->getOperand(0).isReg() ||
          !I->getOperand(0).getReg().isPhysical() ||
          isInsideExtensionWindow(MBB, I)) {
        ++I;
        continue;
      }

      Register Base = I->getOperand(0).getReg();
      SmallVector<MachineInstr *, 4> Stores;
      SmallVector<MachineInstr *, 2> Copies;
      auto IsStore = [&](const MachineInstr &MI) {
        return (MI.getOpcode() == C166::MOVmr ||
                MI.getOpcode() == C166::MOVmr16) &&
               !hasExplicitOrderedMemoryAccess(MI) &&
               MI.getOperand(0).isReg() && MI.getOperand(0).getReg() == Base;
      };
      auto CanHoistCopy = [&](const MachineInstr &MI) {
        if (MI.getOpcode() != C166::MOVrr)
          return false;
        for (const MachineOperand &MO : MI.operands()) {
          if (!MO.isReg() || !MO.getReg() || MO.getReg() == C166::PSW ||
              MO.getReg() == C166::C)
            continue;
          if (TRI.regsOverlap(MO.getReg(), Base))
            return false;
          if (!MO.isDef())
            continue;
          for (const MachineInstr *Store : Stores) {
            unsigned SourceOperand = Store->getOpcode() == C166::MOVmr ? 1 : 2;
            if (TRI.regsOverlap(MO.getReg(),
                                Store->getOperand(SourceOperand).getReg()))
              return false;
          }
        }
        return true;
      };
      auto Scan = I;
      while (Scan != MBB.end()) {
        if (IsStore(*Scan)) {
          Stores.push_back(&*Scan);
          Scan = nextNonDebug(MBB, Scan);
          continue;
        }

        SmallVector<MachineInstr *, 2> PendingCopies;
        auto Lookahead = Scan;
        while (Lookahead != MBB.end() && CanHoistCopy(*Lookahead)) {
          PendingCopies.push_back(&*Lookahead);
          Lookahead = nextNonDebug(MBB, Lookahead);
        }
        if (PendingCopies.empty() || Lookahead == MBB.end() ||
            !IsStore(*Lookahead))
          break;
        llvm::append_range(Copies, PendingCopies);
        Scan = Lookahead;
      }
      SmallVector<std::pair<int64_t, MachineInstr *>, 4> StoresByOffset;
      uint64_t OriginalSize = 0;
      bool InvalidStore = false;
      for (MachineInstr *Store : Stores) {
        unsigned SourceOperand = Store->getOpcode() == C166::MOVmr ? 1 : 2;
        int64_t Offset = 0;
        if (Store->getOpcode() == C166::MOVmr16) {
          if (!Store->getOperand(1).isImm()) {
            InvalidStore = true;
            break;
          }
          Offset = Store->getOperand(1).getImm();
        }
        if (!Store->getOperand(SourceOperand).isReg() ||
            !Store->getOperand(SourceOperand).getReg().isPhysical() ||
            TRI.regsOverlap(Store->getOperand(SourceOperand).getReg(), Base) ||
            Offset < 0 || (Offset & 1)) {
          InvalidStore = true;
          break;
        }
        OriginalSize += Store->getOpcode() == C166::MOVmr ? 2 : 4;
        StoresByOffset.emplace_back(Offset, Store);
      }
      if (InvalidStore) {
        I = Scan;
        continue;
      }

      llvm::sort(StoresByOffset, [](const auto &Left, const auto &Right) {
        return Left.first < Right.first;
      });
      bool Contiguous = true;
      for (unsigned Index = 1; Index != StoresByOffset.size(); ++Index)
        Contiguous &=
            StoresByOffset[Index].first == StoresByOffset[Index - 1].first + 2;
      uint64_t EndOffset =
          static_cast<uint64_t>(StoresByOffset.back().first) + 2;
      if (!Contiguous || !isUInt<16>(EndOffset)) {
        I = Scan;
        continue;
      }

      if (Copies.empty() && Base == C166::R0 &&
          foldFrameSetupStoreChain(MF, MBB, Scan, Stores, StoresByOffset,
                                   OriginalSize, TII, TRI)) {
        Changed = true;
        I = Scan;
        continue;
      }
      if (Copies.empty() &&
          foldStoresIntoFollowingAddress(MBB, Scan, Stores, StoresByOffset,
                                         OriginalSize, Base, TII, TRI)) {
        Changed = true;
        I = MBB.begin();
        continue;
      }
      if (Stores.size() < 3) {
        I = Scan;
        continue;
      }

      MachineInstr &OriginalLast = *Stores.back();
      bool PSWIsDead =
          MBB.computeRegisterLiveness(
              &TRI, C166::PSW, MachineBasicBlock::const_iterator(Scan)) ==
          MachineBasicBlock::LQR_Dead;
      if (!PSWIsDead && StoresByOffset.front().second != &OriginalLast) {
        I = Scan;
        continue;
      }
      bool CarryIsDead =
          MBB.computeRegisterLiveness(
              &TRI, C166::C, MachineBasicBlock::const_iterator(Scan)) ==
          MachineBasicBlock::LQR_Dead;
      if (!CarryIsDead) {
        I = Scan;
        continue;
      }

      bool BaseIsDead =
          MBB.computeRegisterLiveness(
              &TRI, Base, MachineBasicBlock::const_iterator(Scan)) ==
          MachineBasicBlock::LQR_Dead;

      // Updating the base is cheapest when the chain restores a live base to
      // its original value.  Otherwise use a dead register as an address
      // cursor, but only when the complete replacement is smaller.
      uint64_t DirectSize =
          (EndOffset <= 7 ? 2 : 4) + 2 * StoresByOffset.size();
      uint64_t ScratchSize =
          (EndOffset <= 15 ? 4 : 6) + 2 * StoresByOffset.size();
      bool CanUpdateBase =
          Base != C166::R0 && (StoresByOffset.front().first == 0 || BaseIsDead);
      bool UseScratch = !CanUpdateBase || DirectSize >= OriginalSize;
      Register AddressReg = Base;
      if (UseScratch) {
        if (ScratchSize >= OriginalSize ||
            !(AddressReg = findPostRAScratch(
                  MBB, I, Stores.back()->getIterator(), TRI))) {
          I = Scan;
          continue;
        }
        if (EndOffset >= 8 && EndOffset <= 15) {
          MachineInstrBuilder Immediate =
              BuildMI(MBB, *Stores.front(), MIMetadata(*Stores.front()),
                      TII.get(C166::MOVri4), AddressReg)
                  .addImm(EndOffset);
          markRegisterDefDead(*Immediate, C166::PSW, TRI);
          MachineInstrBuilder Address =
              BuildMI(MBB, *Stores.front(), MIMetadata(*Stores.front()),
                      TII.get(C166::ADDrr), AddressReg)
                  .addReg(AddressReg, RegState::Kill)
                  .addReg(Base,
                          BaseIsDead ? RegState::Kill : RegState::NoFlags);
          markRegisterDefDead(*Address, C166::PSW, TRI);
          markRegisterDefDead(*Address, C166::C, TRI);
        } else {
          MachineInstrBuilder Copy =
              BuildMI(MBB, *Stores.front(), MIMetadata(*Stores.front()),
                      TII.get(C166::MOVrr), AddressReg)
                  .addReg(Base,
                          BaseIsDead ? RegState::Kill : RegState::NoFlags);
          markRegisterDefDead(*Copy, C166::PSW, TRI);
        }
      }

      if (!UseScratch || EndOffset < 8 || EndOffset > 15) {
        MachineInstrBuilder Address =
            BuildMI(MBB, *Stores.front(), MIMetadata(*Stores.front()),
                    TII.get(EndOffset <= 7 ? C166::ADDri3 : C166::ADDri16),
                    AddressReg)
                .addReg(AddressReg, RegState::Kill)
                .addImm(EndOffset);
        markRegisterDefDead(*Address, C166::PSW, TRI);
        markRegisterDefDead(*Address, C166::C, TRI);
      }

      for (MachineInstr *Copy : Copies) {
        for (MachineOperand &MO : Copy->operands())
          if (MO.isReg() && MO.isUse())
            MO.setIsKill(false);
        MBB.splice(Stores.front()->getIterator(), &MBB, Copy->getIterator());
      }

      for (auto [Index, Entry] :
           llvm::enumerate(llvm::reverse(StoresByOffset))) {
        MachineInstr *Store = Entry.second;
        unsigned SourceOperand = Store->getOpcode() == C166::MOVmr ? 1 : 2;
        MachineOperand Source = Store->getOperand(SourceOperand);
        Source.setIsKill(false);
        MachineInstrBuilder Replacement =
            BuildMI(MBB, *Stores.front(), MIMetadata(*Store),
                    TII.get(C166::MOVmrPreDec), AddressReg)
                .addReg(AddressReg, RegState::Kill)
                .add(Source);
        Replacement.cloneMemRefs(*Store).setMIFlags(Store->getFlags());
        if (Index + 1 == Stores.size()) {
          Replacement->getOperand(0).setIsDead(UseScratch || BaseIsDead);
          if (PSWIsDead)
            markRegisterDefDead(*Replacement, C166::PSW, TRI);
        } else {
          markRegisterDefDead(*Replacement, C166::PSW, TRI);
        }
      }

      for (MachineInstr *Store : Stores)
        Store->eraseFromParent();
      Changed = true;
      I = Scan;
    }
  }

  return Changed;
}

// Replace a two-word negation expressed as one's complement plus a carry
// materialized through control flow:
//
//   Carry = 1; cmp Low, 0; je Merge    =>   cpl High
// Clear: Carry = 0                          neg Low
// Merge: cpl High                            subc High, #0xffff
//        add High, Carry
//        neg Low
//
// NEG sets C exactly when 0 - Low borrows.  SUBC with 0xffff therefore adds
// one only when Low was zero, which gives ~(High) + (Low == 0).
static bool foldWideNegationDiamonds(MachineFunction &MF,
                                     const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  auto HasDeadFlagDefs = [&](const MachineInstr &MI) {
    for (Register Flag : {Register(C166::PSW), Register(C166::C)})
      if (const MachineOperand *Def = MI.findRegisterDefOperand(Flag, &TRI);
          Def && !Def->isDead())
        return false;
    return true;
  };

  for (MachineBasicBlock &MergeMBB : MF) {
    if (MergeMBB.pred_size() != 2)
      continue;

    MachineBasicBlock *ClearMBB = MergeMBB.getPrevNode();
    MachineBasicBlock *BranchMBB = ClearMBB ? ClearMBB->getPrevNode() : nullptr;
    if (!BranchMBB || ClearMBB->pred_size() != 1 ||
        *ClearMBB->pred_begin() != BranchMBB || ClearMBB->succ_size() != 1 ||
        *ClearMBB->succ_begin() != &MergeMBB ||
        !llvm::is_contained(MergeMBB.predecessors(), BranchMBB) ||
        !llvm::is_contained(MergeMBB.predecessors(), ClearMBB) ||
        BranchMBB->succ_size() != 2 || !BranchMBB->isSuccessor(ClearMBB) ||
        !BranchMBB->isSuccessor(&MergeMBB))
      continue;

    auto One = BranchMBB->begin();
    while (One != BranchMBB->end() && One->isDebugInstr())
      ++One;
    auto Compare = One == BranchMBB->end() ? BranchMBB->end()
                                           : nextNonDebug(*BranchMBB, One);
    auto Branch = Compare == BranchMBB->end()
                      ? BranchMBB->end()
                      : nextNonDebug(*BranchMBB, Compare);
    if (One == BranchMBB->end() || Compare == BranchMBB->end() ||
        Branch == BranchMBB->end() ||
        nextNonDebug(*BranchMBB, Branch) != BranchMBB->end() ||
        One->getOpcode() != C166::MOVri4 ||
        !isWholePhysicalRegister(One->getOperand(0)) ||
        !One->getOperand(1).isImm() || One->getOperand(1).getImm() != 1 ||
        One->isBundledWithPred() || One->isBundledWithSucc() ||
        Compare->getOpcode() != C166::CMPri3 ||
        !isWholePhysicalRegister(Compare->getOperand(0)) ||
        !Compare->getOperand(1).isImm() ||
        Compare->getOperand(1).getImm() != 0 || Compare->isBundledWithPred() ||
        Compare->isBundledWithSucc() || Branch->getOpcode() != C166::JMPR_EQ ||
        !Branch->getOperand(0).isMBB() ||
        Branch->getOperand(0).getMBB() != &MergeMBB ||
        Branch->isBundledWithPred() || Branch->isBundledWithSucc())
      continue;

    auto Zero = ClearMBB->begin();
    while (Zero != ClearMBB->end() && Zero->isDebugInstr())
      ++Zero;
    if (Zero == ClearMBB->end() ||
        nextNonDebug(*ClearMBB, Zero) != ClearMBB->end() ||
        Zero->getOpcode() != C166::MOVri4 ||
        !isWholePhysicalRegister(Zero->getOperand(0)) ||
        Zero->getOperand(0).getReg() != One->getOperand(0).getReg() ||
        !Zero->getOperand(1).isImm() || Zero->getOperand(1).getImm() != 0 ||
        Zero->isBundledWithPred() || Zero->isBundledWithSucc())
      continue;

    auto Complement = MergeMBB.begin();
    while (Complement != MergeMBB.end() && Complement->isDebugInstr())
      ++Complement;
    auto Add = Complement == MergeMBB.end()
                   ? MergeMBB.end()
                   : nextNonDebug(MergeMBB, Complement);
    auto Negate =
        Add == MergeMBB.end() ? MergeMBB.end() : nextNonDebug(MergeMBB, Add);
    if (Complement == MergeMBB.end() || Add == MergeMBB.end() ||
        Negate == MergeMBB.end() || Complement->getOpcode() != C166::CPL ||
        Add->getOpcode() != C166::ADDrr || Negate->getOpcode() != C166::NEG)
      continue;

    if (!isWholePhysicalRegister(Complement->getOperand(0)) ||
        !isWholePhysicalRegister(Complement->getOperand(1)) ||
        Complement->getOperand(0).getReg() !=
            Complement->getOperand(1).getReg() ||
        !isWholePhysicalRegister(Add->getOperand(0)) ||
        !isWholePhysicalRegister(Add->getOperand(1)) ||
        Add->getOperand(0).getReg() != Complement->getOperand(0).getReg() ||
        Add->getOperand(1).getReg() != Complement->getOperand(0).getReg() ||
        !isWholePhysicalRegister(Add->getOperand(2)) ||
        Add->getOperand(2).getReg() != One->getOperand(0).getReg() ||
        !Add->getOperand(2).isKill() ||
        !isWholePhysicalRegister(Negate->getOperand(0)) ||
        !isWholePhysicalRegister(Negate->getOperand(1)) ||
        Negate->getOperand(0).getReg() != Negate->getOperand(1).getReg() ||
        Complement->isBundledWithPred() || Complement->isBundledWithSucc() ||
        Add->isBundledWithPred() || Add->isBundledWithSucc() ||
        Negate->isBundledWithPred() || Negate->isBundledWithSucc())
      continue;

    Register Boolean = One->getOperand(0).getReg();
    Register High = Complement->getOperand(0).getReg();
    Register Low = Negate->getOperand(0).getReg();
    if (Compare->getOperand(0).getReg() != Low ||
        !formsGR32Pair(Low, High, TRI) || TRI.regsOverlap(Boolean, Low) ||
        TRI.regsOverlap(Boolean, High) || !HasDeadFlagDefs(*One) ||
        !HasDeadFlagDefs(*Zero) || !HasDeadFlagDefs(*Complement) ||
        !HasDeadFlagDefs(*Add) || !HasDeadFlagDefs(*Negate) ||
        isInsideExtensionWindow(*BranchMBB, One) ||
        isInsideExtensionWindow(*ClearMBB, Zero) ||
        isInsideExtensionWindow(MergeMBB, Complement) ||
        isInsideExtensionWindow(MergeMBB, Add) ||
        isInsideExtensionWindow(MergeMBB, Negate))
      continue;

    bool HasRelevantDebugUse = false;
    for (MachineBasicBlock *MBB : {BranchMBB, ClearMBB, &MergeMBB})
      for (MachineInstr &MI : *MBB)
        if (MI.isDebugInstr() && (readsPhysicalRegister(MI, Boolean, TRI) ||
                                  readsPhysicalRegister(MI, Low, TRI) ||
                                  readsPhysicalRegister(MI, High, TRI)))
          HasRelevantDebugUse = true;
    if (HasRelevantDebugUse)
      continue;

    unsigned OldSize =
        TII.getInstSizeInBytes(*One) + TII.getInstSizeInBytes(*Compare) +
        TII.getInstSizeInBytes(*Branch) + TII.getInstSizeInBytes(*Zero) +
        TII.getInstSizeInBytes(*Complement) + TII.getInstSizeInBytes(*Add) +
        TII.getInstSizeInBytes(*Negate);
    unsigned NewSize = TII.get(C166::CPL).getSize() +
                       TII.get(C166::NEG).getSize() +
                       TII.get(C166::SUBCri16).getSize();
    if (NewSize >= OldSize)
      continue;

    MergeMBB.splice(Add, &MergeMBB, Negate);
    MachineInstrBuilder Subtract =
        BuildMI(MergeMBB, *Add, MIMetadata(*Add), TII.get(C166::SUBCri16), High)
            .addReg(High, RegState::Kill)
            .addImm(0xffff);
    for (const MachineOperand &MO : Add->operands())
      if (MO.isImplicit() && MO.isReg() && MO.getReg() != C166::PSW &&
          MO.getReg() != C166::C)
        Subtract.add(MO);
    markRegisterDefDead(*Subtract, C166::PSW, TRI);
    markRegisterDefDead(*Subtract, C166::C, TRI);
    Subtract->setFlags(Complement->getFlags() | Add->getFlags() |
                       Negate->getFlags());
    Subtract->clearFlag(MachineInstr::NoSWrap);
    Subtract->clearFlag(MachineInstr::NoUWrap);
    if (MachineOperand *Carry = Negate->findRegisterDefOperand(C166::C, &TRI))
      Carry->setIsDead(false);

    TII.removeBranch(*BranchMBB);
    BranchMBB->removeSuccessor(&MergeMBB);
    if (MergeMBB.isLiveIn(Boolean))
      MergeMBB.removeLiveIn(Boolean);
    One->eraseFromParent();
    Compare->eraseFromParent();
    Zero->eraseFromParent();
    Add->eraseFromParent();
    Changed = true;
  }

  return Changed;
}

// A conditional value of +1/-1 can leave +1 in the destination on both
// outgoing edges and negate it only on the -1 edge. This replaces a
// four-byte full-width immediate with a two-byte NEG.
static bool foldNegativeOneMaterializations(MachineFunction &MF,
                                            const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &NegativeMBB : MF) {
    MachineBasicBlock *BranchMBB = NegativeMBB.getPrevNode();
    MachineBasicBlock *MergeMBB = NegativeMBB.getNextNode();
    if (!BranchMBB || !MergeMBB || NegativeMBB.pred_size() != 1 ||
        *NegativeMBB.pred_begin() != BranchMBB ||
        NegativeMBB.succ_size() != 1 || *NegativeMBB.succ_begin() != MergeMBB ||
        BranchMBB->succ_size() != 2 || !BranchMBB->isSuccessor(&NegativeMBB) ||
        !BranchMBB->isSuccessor(MergeMBB) ||
        !llvm::is_contained(MergeMBB->predecessors(), BranchMBB))
      continue;

    auto Negative = NegativeMBB.begin();
    if (Negative == NegativeMBB.end() || Negative->isDebugInstr() ||
        nextNonDebug(NegativeMBB, Negative) != NegativeMBB.end() ||
        Negative->getOpcode() != C166::MOVri16 ||
        !isWholePhysicalRegister(Negative->getOperand(0)) ||
        !Negative->getOperand(1).isImm() ||
        Negative->getOperand(1).getImm() != -1 ||
        Negative->isBundledWithPred() || Negative->isBundledWithSucc() ||
        isInsideExtensionWindow(NegativeMBB, Negative))
      continue;

    MachineBasicBlock *TBB = nullptr;
    MachineBasicBlock *FBB = nullptr;
    SmallVector<MachineOperand, 4> Cond;
    if (TII.analyzeBranch(*BranchMBB, TBB, FBB, Cond, false) ||
        TBB != MergeMBB || FBB || Cond.empty())
      continue;

    auto Branch = BranchMBB->getLastNonDebugInstr();
    auto Positive = Branch == BranchMBB->end()
                        ? BranchMBB->end()
                        : previousNonDebug(*BranchMBB, Branch);
    if (Branch == BranchMBB->end() || !Branch->isBranch() ||
        Positive == BranchMBB->end() || Positive->isDebugInstr() ||
        (Positive->getOpcode() != C166::MOVri4 &&
         Positive->getOpcode() != C166::MOVri16) ||
        !isWholePhysicalRegister(Positive->getOperand(0)) ||
        Positive->getOperand(0).isDead() ||
        Positive->getOperand(0).getReg() != Negative->getOperand(0).getReg() ||
        !Positive->getOperand(1).isImm() ||
        Positive->getOperand(1).getImm() != 1 ||
        Positive->isBundledWithPred() || Positive->isBundledWithSucc() ||
        Branch->isBundledWithPred() || Branch->isBundledWithSucc() ||
        isInsideExtensionWindow(*BranchMBB, Positive) ||
        isInsideExtensionWindow(*BranchMBB, Branch))
      continue;

    Register Value = Negative->getOperand(0).getReg();
    if (!C166::GR16RegClass.contains(Value) ||
        readsPhysicalRegister(*Branch, Value, TRI) ||
        NegativeMBB.computeRegisterLiveness(
            &TRI, C166::PSW,
            MachineBasicBlock::const_iterator(NegativeMBB.end())) !=
            MachineBasicBlock::LQR_Dead ||
        NegativeMBB.computeRegisterLiveness(
            &TRI, C166::C,
            MachineBasicBlock::const_iterator(NegativeMBB.end())) !=
            MachineBasicBlock::LQR_Dead)
      continue;

    if (TII.get(C166::NEG).getSize() >= TII.getInstSizeInBytes(*Negative))
      continue;

    MachineInstrBuilder Negate =
        BuildMI(NegativeMBB, *Negative, MIMetadata(*Negative),
                TII.get(C166::NEG), Value)
            .addReg(Value, RegState::Kill);
    Negate->getOperand(0).setIsDead(Negative->getOperand(0).isDead());
    markRegisterDefDead(*Negate, C166::PSW, TRI);
    markRegisterDefDead(*Negate, C166::C, TRI);
    Negate->setFlags(Negative->getFlags());
    Negate->clearFlag(MachineInstr::NoSWrap);
    Negate->clearFlag(MachineInstr::NoUWrap);
    if (!NegativeMBB.isLiveIn(Value))
      NegativeMBB.addLiveIn(Value);
    Negative->eraseFromParent();
    Changed = true;
  }

  return Changed;
}

// Fold a post-RA boolean diamond around a loaded value:
//
//   Boolean = 1; set flags; jcc Merge   =>   Boolean = load; set flags;
// Update: Boolean = 0                         inverted-jcc Merge
// Merge:  Value = load; Value |= Boolean      Update: bset Boolean.0
//                                              Merge: Value = Boolean
//
// Besides removing the materialized boolean and OR, hoisting the load can make
// it adjacent to other loads and expose a post-increment addressing fold.
static bool foldConditionalBooleanOrs(MachineFunction &MF,
                                      const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MergeMBB : MF) {
    if (MergeMBB.pred_size() != 2)
      continue;

    MachineBasicBlock *UpdateMBB = MergeMBB.getPrevNode();
    MachineBasicBlock *BranchMBB =
        UpdateMBB ? UpdateMBB->getPrevNode() : nullptr;
    if (!BranchMBB || UpdateMBB->pred_size() != 1 ||
        *UpdateMBB->pred_begin() != BranchMBB || UpdateMBB->succ_size() != 1 ||
        *UpdateMBB->succ_begin() != &MergeMBB ||
        !llvm::is_contained(MergeMBB.predecessors(), BranchMBB) ||
        !llvm::is_contained(MergeMBB.predecessors(), UpdateMBB) ||
        BranchMBB->succ_size() != 2 || !BranchMBB->isSuccessor(UpdateMBB) ||
        !BranchMBB->isSuccessor(&MergeMBB))
      continue;

    auto Load = MergeMBB.begin();
    while (Load != MergeMBB.end() && Load->isDebugInstr())
      ++Load;
    if (Load == MergeMBB.end() ||
        (Load->getOpcode() != C166::MOVrm &&
         Load->getOpcode() != C166::MOVrm16) ||
        hasExplicitOrderedMemoryAccess(*Load) ||
        Load->hasUnmodeledSideEffects())
      continue;

    auto Or = nextNonDebug(MergeMBB, Load);
    if (Or == MergeMBB.end() || Or->getOpcode() != C166::ORrr ||
        !isWholePhysicalRegister(Load->getOperand(0)) ||
        !isWholePhysicalRegister(Or->getOperand(0)) ||
        !isWholePhysicalRegister(Or->getOperand(1)) ||
        !isWholePhysicalRegister(Or->getOperand(2)))
      continue;

    Register Value = Load->getOperand(0).getReg();
    Register Boolean = Or->getOperand(2).getReg();
    if (!C166::GR16RegClass.contains(Value, Boolean) ||
        TRI.regsOverlap(Value, Boolean) ||
        Or->getOperand(0).getReg() != Value ||
        Or->getOperand(1).getReg() != Value || !Or->getOperand(1).isKill() ||
        !Or->getOperand(2).isKill())
      continue;

    const MachineOperand *OrPSW = Or->findRegisterDefOperand(C166::PSW, &TRI);
    const MachineOperand *OrCarry = Or->findRegisterDefOperand(C166::C, &TRI);
    if (!OrPSW || !OrPSW->isDead() || !OrCarry || !OrCarry->isDead() ||
        isInsideExtensionWindow(MergeMBB, Load) ||
        isInsideExtensionWindow(MergeMBB, Or))
      continue;

    bool LoadUsesBoolean =
        llvm::any_of(Load->operands(), [&](const MachineOperand &MO) {
          return MO.isReg() && MO.isUse() && MO.getReg() &&
                 TRI.regsOverlap(Boolean, MO.getReg());
        });
    if (LoadUsesBoolean)
      continue;

    auto Zero = UpdateMBB->begin();
    while (Zero != UpdateMBB->end() && Zero->isDebugInstr())
      ++Zero;
    if (Zero == UpdateMBB->end() ||
        (Zero->getOpcode() != C166::MOVri4 &&
         Zero->getOpcode() != C166::MOVri16) ||
        !isWholePhysicalRegister(Zero->getOperand(0)) ||
        Zero->getOperand(0).getReg() != Boolean ||
        !Zero->getOperand(1).isImm() || Zero->getOperand(1).getImm() != 0 ||
        nextNonDebug(*UpdateMBB, Zero) != UpdateMBB->end() ||
        isInsideExtensionWindow(*UpdateMBB, Zero))
      continue;

    MachineBasicBlock *TBB = nullptr;
    MachineBasicBlock *FBB = nullptr;
    SmallVector<MachineOperand, 4> Cond;
    if (TII.analyzeBranch(*BranchMBB, TBB, FBB, Cond, false) ||
        TBB != &MergeMBB || FBB || Cond.empty())
      continue;

    auto Branch = BranchMBB->getLastNonDebugInstr();
    if (Branch == BranchMBB->end() || !Branch->isBranch() ||
        isInsideExtensionWindow(*BranchMBB, Branch))
      continue;
    auto Producer = previousNonDebug(*BranchMBB, Branch);
    auto One = Producer == BranchMBB->end()
                   ? BranchMBB->end()
                   : previousNonDebug(*BranchMBB, Producer);
    if (Producer == BranchMBB->end() || One == BranchMBB->end() ||
        (One->getOpcode() != C166::MOVri4 &&
         One->getOpcode() != C166::MOVri16) ||
        !isWholePhysicalRegister(One->getOperand(0)) ||
        One->getOperand(0).getReg() != Boolean || !One->getOperand(1).isImm() ||
        One->getOperand(1).getImm() != 1 || Producer->mayLoad() ||
        Producer->mayStore() || Producer->isCall() || Producer->isInlineAsm() ||
        Producer->hasUnmodeledSideEffects() ||
        Producer->readsRegister(C166::PSW, &TRI) ||
        readsPhysicalRegister(*Producer, Boolean, TRI) ||
        readsPhysicalRegister(*Branch, Boolean, TRI) ||
        isInsideExtensionWindow(*BranchMBB, One) ||
        isInsideExtensionWindow(*BranchMBB, Producer))
      continue;

    // The load moves across the flag producer. Its address must retain the
    // same value, and the new destination must not overwrite a producer def.
    bool ClobbersAddress = Producer->modifiesRegister(Boolean, &TRI);
    for (const MachineOperand &MO : Load->operands())
      if (MO.isReg() && MO.isUse() && MO.getReg())
        ClobbersAddress |= Producer->modifiesRegister(MO.getReg(), &TRI);
    if (ClobbersAddress)
      continue;

    unsigned OldSize = TII.getInstSizeInBytes(*One) +
                       TII.getInstSizeInBytes(*Zero) +
                       TII.getInstSizeInBytes(*Or);
    unsigned NewSize =
        TII.get(C166::BSETreg).getSize() + TII.get(C166::MOVrr).getSize();
    if (NewSize >= OldSize)
      continue;

    DebugLoc BranchDL = Branch->getDebugLoc();
    uint32_t BranchFlags = Branch->getFlags();
    if (TII.reverseBranchCondition(Cond))
      continue;

    BranchProbability MergeProbability;
    BranchProbability UpdateProbability;
    bool HasProbabilities = BranchMBB->hasSuccessorProbabilities();
    auto MergeSuccessor = llvm::find(BranchMBB->successors(), &MergeMBB);
    auto UpdateSuccessor = llvm::find(BranchMBB->successors(), UpdateMBB);
    if (HasProbabilities) {
      MergeProbability = BranchMBB->getSuccProbability(MergeSuccessor);
      UpdateProbability = BranchMBB->getSuccProbability(UpdateSuccessor);
    }

    BranchMBB->splice(One, &MergeMBB, Load);
    Load->getOperand(0).setReg(Boolean);
    Load->getOperand(0).setIsDead(false);

    MachineInstrBuilder Set = BuildMI(*UpdateMBB, Zero, MIMetadata(*Zero),
                                      TII.get(C166::BSETreg), Boolean)
                                  .addReg(Boolean, RegState::Kill)
                                  .addImm(0);
    markRegisterDefDead(*Set, C166::PSW, TRI);
    markRegisterDefDead(*Set, C166::C, TRI);
    Set->setFlags(Zero->getFlags() | Or->getFlags());
    if (!UpdateMBB->isLiveIn(Boolean))
      UpdateMBB->addLiveIn(Boolean);

    MachineInstrBuilder Copy =
        BuildMI(MergeMBB, Or, MIMetadata(*Or), TII.get(C166::MOVrr), Value)
            .addReg(Boolean, RegState::Kill);
    Copy->getOperand(0).setIsDead(Or->getOperand(0).isDead());
    markRegisterDefDead(*Copy, C166::PSW, TRI);
    Copy->setFlags(Or->getFlags());

    One->eraseFromParent();
    Zero->eraseFromParent();
    Or->eraseFromParent();
    TII.removeBranch(*BranchMBB);
    TII.insertBranch(*BranchMBB, &MergeMBB, nullptr, Cond, BranchDL);
    auto NewBranch = BranchMBB->getLastNonDebugInstr();
    assert(NewBranch != BranchMBB->end() && "expected inverted branch");
    NewBranch->setFlags(BranchFlags);

    if (HasProbabilities) {
      BranchMBB->setSuccProbability(MergeSuccessor, UpdateProbability);
      BranchMBB->setSuccProbability(UpdateSuccessor, MergeProbability);
    }
    Changed = true;
  }

  return Changed;
}

static bool shortenNegativeAddImmediates(MachineFunction &MF,
                                         const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Add = *I++;
      if (Add.getOpcode() != C166::ADDri16 || !Add.getOperand(0).isReg() ||
          !Add.getOperand(0).getReg().isPhysical() ||
          Add.getOperand(0).getSubReg() || !Add.getOperand(1).isReg() ||
          Add.getOperand(1).getReg() != Add.getOperand(0).getReg() ||
          Add.getOperand(1).getSubReg() || !Add.getOperand(2).isImm())
        continue;

      uint16_t Immediate = Add.getOperand(2).getImm();
      uint16_t Amount = -Immediate;
      if (Amount == 0 || !isUInt<3>(Amount))
        continue;

      auto Next = nextNonDebug(MBB, Add.getIterator());
      bool CarryIsDead =
          MBB.computeRegisterLiveness(
              &TRI, C166::C, MachineBasicBlock::const_iterator(Next)) ==
          MachineBasicBlock::LQR_Dead;
      MachineInstr *CarryBranch = nullptr;
      unsigned BranchOpcode = 0;
      if (!CarryIsDead) {
        if (Next == MBB.end())
          continue;
        if (Next->getOpcode() == C166::JMPR_ULT)
          BranchOpcode = C166::JMPR_UGE;
        else if (Next->getOpcode() == C166::JMPR_UGE)
          BranchOpcode = C166::JMPR_ULT;
        else
          continue;

        auto AfterBranch = nextNonDebug(MBB, Next);
        if (MBB.computeRegisterLiveness(
                &TRI, C166::C,
                MachineBasicBlock::const_iterator(AfterBranch)) !=
            MachineBasicBlock::LQR_Dead)
          continue;
        CarryBranch = &*Next;
      }

      Register Value = Add.getOperand(0).getReg();
      MachineOperand Source = Add.getOperand(1);
      MachineInstrBuilder Replacement =
          BuildMI(MBB, Add, MIMetadata(Add), TII.get(C166::SUBri3), Value)
              .add(Source)
              .addImm(Amount);
      Replacement->getOperand(0).setIsDead(Add.getOperand(0).isDead());
      copyPSWDefLiveness(*Replacement, Add, TRI);
      if (MachineOperand *NewCarry =
              Replacement->findRegisterDefOperand(C166::C, &TRI)) {
        if (const MachineOperand *OldCarry =
                Add.findRegisterDefOperand(C166::C, &TRI))
          NewCarry->setIsDead(OldCarry->isDead());
      }
      Replacement->setFlags(Add.getFlags());
      if (CarryBranch)
        CarryBranch->setDesc(TII.get(BranchOpcode));
      Add.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

static bool combineAdjacentAddImmediates(MachineFunction &MF,
                                         const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &First = *I++;
      auto FirstIt = std::prev(I);
      if ((First.getOpcode() != C166::ADDri3 &&
           First.getOpcode() != C166::ADDri16) ||
          !First.getOperand(0).isReg() ||
          !First.getOperand(0).getReg().isPhysical() ||
          First.getOperand(0).getSubReg() || !First.getOperand(1).isReg() ||
          First.getOperand(1).getReg() != First.getOperand(0).getReg() ||
          First.getOperand(1).getSubReg() || !First.getOperand(2).isImm() ||
          First.isBundledWithPred() || First.isBundledWithSucc() ||
          isInsideExtensionWindow(MBB, FirstIt))
        continue;

      auto Second = I;
      if (Second == MBB.end() ||
          (Second->getOpcode() != C166::ADDri3 &&
           Second->getOpcode() != C166::ADDri16) ||
          !Second->getOperand(0).isReg() ||
          Second->getOperand(0).getReg() != First.getOperand(0).getReg() ||
          Second->getOperand(0).getSubReg() || !Second->getOperand(1).isReg() ||
          Second->getOperand(1).getReg() != First.getOperand(0).getReg() ||
          Second->getOperand(1).getSubReg() || !Second->getOperand(2).isImm() ||
          Second->isBundledWithPred() || Second->isBundledWithSucc() ||
          isInsideExtensionWindow(MBB, Second))
        continue;

      auto AfterSecond = std::next(Second);
      auto IsDeadAfterSecond = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterSecond)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterSecond(C166::PSW) || !IsDeadAfterSecond(C166::C))
        continue;

      uint16_t Immediate =
          static_cast<uint16_t>(First.getOperand(2).getImm()) +
          static_cast<uint16_t>(Second->getOperand(2).getImm());
      unsigned Opcode = isUInt<3>(Immediate) ? C166::ADDri3 : C166::ADDri16;
      unsigned OldSize =
          TII.getInstSizeInBytes(First) + TII.getInstSizeInBytes(*Second);
      if (TII.get(Opcode).getSize() >= OldSize)
        continue;

      Register Value = First.getOperand(0).getReg();
      MachineOperand Source = First.getOperand(1);
      MachineInstrBuilder Replacement =
          BuildMI(MBB, First, MIMetadata(First), TII.get(Opcode), Value)
              .add(Source)
              .addImm(Immediate);
      Replacement->getOperand(0).setIsDead(Second->getOperand(0).isDead());
      markRegisterDefDead(*Replacement, C166::PSW, TRI);
      markRegisterDefDead(*Replacement, C166::C, TRI);
      Replacement->setFlags(First.getFlags() | Second->getFlags());

      auto ReplacementIt = std::prev(FirstIt);
      First.eraseFromParent();
      Second->eraseFromParent();
      I = ReplacementIt;
      Changed = true;
    }
  }

  return Changed;
}

static bool foldReverseSmallImmediateSubtracts(MachineFunction &MF,
                                               const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Copy = *I++;
      if (Copy.getOpcode() != C166::MOVrr ||
          !isWholePhysicalRegister(Copy.getOperand(0)) ||
          !isWholePhysicalRegister(Copy.getOperand(1)) ||
          Copy.isBundledWithPred() || Copy.isBundledWithSucc() ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Copy.getIterator())))
        continue;

      auto Materialize = std::next(Copy.getIterator());
      if (Materialize == MBB.end() ||
          Materialize->getOpcode() != C166::MOVri4 ||
          !isWholePhysicalRegister(Materialize->getOperand(0)) ||
          !Materialize->getOperand(1).isImm() ||
          Materialize->getOperand(1).getImm() <= 0 ||
          !isUInt<3>(Materialize->getOperand(1).getImm()) ||
          Materialize->isBundledWithPred() || Materialize->isBundledWithSucc())
        continue;

      auto Subtract = std::next(Materialize);
      if (Subtract == MBB.end() || Subtract->getOpcode() != C166::SUBrr ||
          !isWholePhysicalRegister(Subtract->getOperand(0)) ||
          !isWholePhysicalRegister(Subtract->getOperand(1)) ||
          !isWholePhysicalRegister(Subtract->getOperand(2)) ||
          Subtract->isBundledWithPred() || Subtract->isBundledWithSucc())
        continue;

      Register Temporary = Copy.getOperand(0).getReg();
      Register Value = Copy.getOperand(1).getReg();
      if (TRI.regsOverlap(Temporary, Value) ||
          Materialize->getOperand(0).getReg() != Value ||
          Subtract->getOperand(0).getReg() != Value ||
          Subtract->getOperand(1).getReg() != Value ||
          Subtract->getOperand(2).getReg() != Temporary)
        continue;

      auto AfterSubtract = nextNonDebug(MBB, Subtract);
      auto IsDeadAfterSubtract = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg,
                   MachineBasicBlock::const_iterator(AfterSubtract)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterSubtract(Temporary) || !IsDeadAfterSubtract(C166::PSW) ||
          !IsDeadAfterSubtract(C166::C))
        continue;

      MachineInstrBuilder Negate =
          BuildMI(MBB, Copy, MIMetadata(Copy), TII.get(C166::NEG), Value)
              .addReg(Value, RegState::Kill);
      markRegisterDefDead(*Negate, C166::PSW, TRI);
      markRegisterDefDead(*Negate, C166::C, TRI);
      Negate->setFlags(Copy.getFlags() | Materialize->getFlags());

      MachineInstrBuilder Add =
          BuildMI(MBB, Copy, MIMetadata(*Subtract), TII.get(C166::ADDri3),
                  Value)
              .addReg(Value, RegState::Kill)
              .addImm(Materialize->getOperand(1).getImm());
      Add->getOperand(0).setIsDead(Subtract->getOperand(0).isDead());
      markRegisterDefDead(*Add, C166::PSW, TRI);
      markRegisterDefDead(*Add, C166::C, TRI);
      Add->setFlags(Subtract->getFlags());

      auto Resume = std::next(Subtract);
      Copy.eraseFromParent();
      Materialize->eraseFromParent();
      Subtract->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static bool foldNegativeOneSignedBranches(MachineFunction &MF,
                                          const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Compare = *I++;
      if (Compare.getOpcode() != C166::CMPri16 ||
          !Compare.getOperand(0).isReg() ||
          !Compare.getOperand(0).getReg().isPhysical() ||
          Compare.getOperand(0).getSubReg() || !Compare.getOperand(1).isImm() ||
          static_cast<uint16_t>(Compare.getOperand(1).getImm()) != 0xffff ||
          isInsideExtensionWindow(MBB, std::prev(I)))
        continue;

      auto Branch = nextNonDebug(MBB, Compare.getIterator());
      if (Branch == MBB.end() || (Branch->getOpcode() != C166::JMPR_SGT &&
                                  Branch->getOpcode() != C166::JMPR_SLE))
        continue;

      auto AfterBranch = nextNonDebug(MBB, Branch);
      auto IsDeadAfterBranch = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterBranch)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterBranch(C166::PSW) || !IsDeadAfterBranch(C166::C))
        continue;

      MachineOperand Value = Compare.getOperand(0);
      MachineInstrBuilder Replacement =
          BuildMI(MBB, Compare, MIMetadata(Compare),
                  TII.get(Branch->getOpcode() == C166::JMPR_SGT ? C166::JNBreg
                                                                : C166::JBreg))
              .add(Value)
              .addImm(15)
              .addMBB(Branch->getOperand(0).getMBB());
      Replacement->setFlags(Compare.getFlags() | Branch->getFlags());
      auto Resume = std::next(Branch);
      Compare.eraseFromParent();
      Branch->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static bool foldFullMaskEqualityBranches(MachineFunction &MF,
                                         const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &And = *I++;
      if ((And.getOpcode() != C166::ANDri3 &&
           And.getOpcode() != C166::ANDri16) ||
          !And.getOperand(0).isReg() ||
          !And.getOperand(0).getReg().isPhysical() ||
          And.getOperand(0).getSubReg() || !And.getOperand(1).isReg() ||
          And.getOperand(1).getReg() != And.getOperand(0).getReg() ||
          And.getOperand(1).getSubReg() || !And.getOperand(2).isImm() ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(And.getIterator())))
        continue;

      uint16_t Mask = static_cast<uint16_t>(And.getOperand(2).getImm());
      if (Mask == 0)
        continue;

      auto Compare = nextNonDebug(MBB, And.getIterator());
      if (Compare == MBB.end() ||
          (Compare->getOpcode() != C166::CMPri3 &&
           Compare->getOpcode() != C166::CMPri16) ||
          !Compare->getOperand(0).isReg() ||
          Compare->getOperand(0).getReg() != And.getOperand(0).getReg() ||
          Compare->getOperand(0).getSubReg() ||
          !Compare->getOperand(1).isImm() ||
          static_cast<uint16_t>(Compare->getOperand(1).getImm()) != Mask)
        continue;

      auto Branch = nextNonDebug(MBB, Compare);
      if (Branch == MBB.end() || (Branch->getOpcode() != C166::JMPR_EQ &&
                                  Branch->getOpcode() != C166::JMPR_NE))
        continue;

      Register Value = And.getOperand(0).getReg();
      auto AfterBranch = nextNonDebug(MBB, Branch);
      auto IsDeadAfterBranch = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterBranch)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterBranch(Value) || !IsDeadAfterBranch(C166::PSW) ||
          !IsDeadAfterBranch(C166::C) ||
          hasDebugUseBeforeOverwrite(And, Value, TRI))
        continue;

      unsigned OldSize =
          TII.getInstSizeInBytes(And) + TII.getInstSizeInBytes(*Compare);
      unsigned NewSize =
          TII.get(C166::CPL).getSize() + TII.getInstSizeInBytes(And);
      if (NewSize >= OldSize)
        continue;

      MachineOperand Source = And.getOperand(1);
      MachineInstrBuilder Complement =
          BuildMI(MBB, And, MIMetadata(And), TII.get(C166::CPL), Value)
              .add(Source);
      markRegisterDefDead(*Complement, C166::PSW, TRI);
      markRegisterDefDead(*Complement, C166::C, TRI);
      Complement->setFlags(And.getFlags());

      And.getOperand(0).setIsDead(true);
      And.getOperand(1).setIsKill(true);
      copyFlagDefLiveness(And, *Compare, TRI);
      And.setFlags(And.getFlags() | Compare->getFlags());
      auto Resume = std::next(Compare);
      Compare->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static bool foldCopiedOneBitBranches(MachineFunction &MF,
                                     const C166InstrInfo &TII) {
  constexpr unsigned MaxScanDistance = 8;
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Copy = *I++;
      if (Copy.getOpcode() != C166::MOVrr || !Copy.getOperand(0).isReg() ||
          !Copy.getOperand(0).getReg().isPhysical() ||
          Copy.getOperand(0).getSubReg() || !Copy.getOperand(1).isReg() ||
          !Copy.getOperand(1).getReg().isPhysical() ||
          Copy.getOperand(1).getSubReg() ||
          Copy.getOperand(0).getReg() == Copy.getOperand(1).getReg() ||
          isInsideExtensionWindow(MBB, std::prev(I)))
        continue;

      auto And = nextNonDebug(MBB, Copy.getIterator());
      if (And == MBB.end() ||
          (And->getOpcode() != C166::ANDri3 &&
           And->getOpcode() != C166::ANDri16) ||
          !And->getOperand(0).isReg() ||
          And->getOperand(0).getReg() != Copy.getOperand(0).getReg() ||
          And->getOperand(0).getSubReg() || !And->getOperand(1).isReg() ||
          And->getOperand(1).getReg() != Copy.getOperand(0).getReg() ||
          And->getOperand(1).getSubReg() || !And->getOperand(2).isImm() ||
          isInsideExtensionWindow(MBB, And))
        continue;

      uint16_t Mask = static_cast<uint16_t>(And->getOperand(2).getImm());
      if (!isPowerOf2_32(Mask))
        continue;

      Register Value = Copy.getOperand(0).getReg();
      Register Source = Copy.getOperand(1).getReg();
      bool Invalid = false;
      for (auto Debug = std::next(Copy.getIterator()); Debug != And; ++Debug)
        if (Debug->isDebugInstr() &&
            (readsPhysicalRegister(*Debug, Value, TRI) ||
             readsPhysicalRegister(*Debug, Source, TRI)))
          Invalid = true;
      if (Invalid)
        continue;

      MachineInstr *Compare = nullptr;
      MachineInstr *Branch = nullptr;
      auto Scan = nextNonDebug(MBB, And);
      for (unsigned Span = 0; Scan != MBB.end() && Span != MaxScanDistance;
           Scan = nextNonDebug(MBB, Scan), ++Span) {
        if ((Scan->getOpcode() == C166::JMPR_EQ ||
             Scan->getOpcode() == C166::JMPR_NE) &&
            Scan == nextNonDebug(MBB, And)) {
          Branch = &*Scan;
          break;
        }

        if ((Scan->getOpcode() == C166::CMPri3 ||
             Scan->getOpcode() == C166::CMPri16) &&
            Scan->getOperand(0).isReg() &&
            Scan->getOperand(0).getReg() == Value &&
            !Scan->getOperand(0).getSubReg() && Scan->getOperand(1).isImm() &&
            Scan->getOperand(1).getImm() == 0) {
          auto Next = nextNonDebug(MBB, Scan);
          if (Next != MBB.end() && (Next->getOpcode() == C166::JMPR_EQ ||
                                    Next->getOpcode() == C166::JMPR_NE)) {
            Compare = &*Scan;
            Branch = &*Next;
          }
          break;
        }

        if (Scan->isCall() || Scan->isInlineAsm() || Scan->isTerminator() ||
            isExtensionOpcode(Scan->getOpcode()) ||
            readsPhysicalRegister(*Scan, Value, TRI) ||
            readsPhysicalRegister(*Scan, Source, TRI) ||
            Scan->modifiesRegister(Value, &TRI) ||
            Scan->modifiesRegister(Source, &TRI) ||
            readsPhysicalRegister(*Scan, C166::PSW, TRI) ||
            readsPhysicalRegister(*Scan, C166::C, TRI)) {
          Invalid = true;
          break;
        }
      }
      if (Branch)
        for (auto Debug = std::next(And); Debug != Branch->getIterator();
             ++Debug)
          if (Debug->isDebugInstr() &&
              (readsPhysicalRegister(*Debug, Value, TRI) ||
               readsPhysicalRegister(*Debug, Source, TRI)))
            Invalid = true;
      if (Invalid || !Branch || isInsideExtensionWindow(MBB, Branch))
        continue;

      auto AfterBranch = nextNonDebug(MBB, Branch->getIterator());
      auto IsDeadAfterBranch = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterBranch)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterBranch(Value) || !IsDeadAfterBranch(C166::PSW) ||
          !IsDeadAfterBranch(C166::C))
        continue;

      MachineInstrBuilder Replacement =
          BuildMI(MBB, *Branch, MIMetadata(*Branch),
                  TII.get(Branch->getOpcode() == C166::JMPR_NE ? C166::JBreg
                                                               : C166::JNBreg))
              .addReg(Source, getKillRegState(IsDeadAfterBranch(Source)))
              .addImm(llvm::countr_zero(Mask))
              .addMBB(Branch->getOperand(0).getMBB());
      Replacement->setFlags(Copy.getFlags() | And->getFlags() |
                            (Compare ? Compare->getFlags() : 0) |
                            Branch->getFlags());
      auto Resume = std::next(Branch->getIterator());
      Copy.eraseFromParent();
      And->eraseFromParent();
      if (Compare)
        Compare->eraseFromParent();
      Branch->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static bool foldXorBitBranches(MachineFunction &MF, const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Copy = *I++;
      if (Copy.getOpcode() != C166::MOVrr || !Copy.getOperand(0).isReg() ||
          !Copy.getOperand(1).isReg() ||
          !Copy.getOperand(0).getReg().isPhysical() ||
          !Copy.getOperand(1).getReg().isPhysical() ||
          Copy.getOperand(0).getSubReg() || Copy.getOperand(1).getSubReg() ||
          Copy.getOperand(0).getReg() == Copy.getOperand(1).getReg() ||
          isInsideExtensionWindow(MBB, std::prev(I)))
        continue;

      auto Xor = I;
      if (Xor == MBB.end() || Xor->getOpcode() != C166::XORrr ||
          !Xor->getOperand(0).isReg() || !Xor->getOperand(1).isReg() ||
          !Xor->getOperand(2).isReg() || Xor->getOperand(0).getSubReg() ||
          Xor->getOperand(1).getSubReg() || Xor->getOperand(2).getSubReg())
        continue;

      Register Temporary = Copy.getOperand(0).getReg();
      Register LHS = Copy.getOperand(1).getReg();
      Register RHS = Xor->getOperand(2).getReg();
      if (Xor->getOperand(0).getReg() != Temporary ||
          Xor->getOperand(1).getReg() != Temporary || !RHS.isPhysical() ||
          RHS == Temporary)
        continue;

      auto Branch = std::next(Xor);
      if (Branch == MBB.end() ||
          (Branch->getOpcode() != C166::JBreg &&
           Branch->getOpcode() != C166::JNBreg) ||
          !Branch->getOperand(0).isReg() ||
          Branch->getOperand(0).getReg() != Temporary ||
          Branch->getOperand(0).getSubReg() || !Branch->getOperand(1).isImm() ||
          !Branch->getOperand(2).isMBB() ||
          isInsideExtensionWindow(MBB, Branch))
        continue;

      auto AfterBranch = nextNonDebug(MBB, Branch);
      auto IsDeadAfterBranch = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterBranch)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterBranch(Temporary) || !IsDeadAfterBranch(C166::PSW) ||
          !IsDeadAfterBranch(C166::C))
        continue;

      unsigned Bit = Branch->getOperand(1).getImm();
      MachineInstrBuilder Compare =
          BuildMI(MBB, Copy, MIMetadata(Copy), TII.get(C166::BCMPreg));
      Compare.addReg(LHS, getKillRegState(IsDeadAfterBranch(LHS)));
      Compare.addReg(RHS,
                     getKillRegState(RHS != LHS && IsDeadAfterBranch(RHS)));
      Compare.addImm(Bit).addImm(Bit);
      Compare->setFlags(Copy.getFlags() | Xor->getFlags());

      MachineInstrBuilder Replacement =
          BuildMI(MBB, *Branch, MIMetadata(*Branch),
                  TII.get(Branch->getOpcode() == C166::JBreg ? C166::JMPR_N
                                                             : C166::JMPR_NN));
      Replacement.addMBB(Branch->getOperand(2).getMBB());
      Replacement->setFlags(Branch->getFlags());

      auto Resume = std::next(Branch);
      Copy.eraseFromParent();
      Xor->eraseFromParent();
      Branch->eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

struct ZeroProvenance {
  uint64_t Words = 0;
  bool Known = false;
};

static constexpr unsigned PSWZeroBit = 12;

static bool isSimpleStore(const MachineInstr &MI) {
  return MI.mayStore() && !MI.mayLoad() && !MI.isCall() && !MI.isInlineAsm() &&
         !MI.isTerminator() && !MI.hasUnmodeledSideEffects();
}

static bool matchWideZeroBranch(MachineBasicBlock &MBB, MachineInstr &Branch,
                                MachineInstr *&Compare, Register &Root,
                                MachineBasicBlock::iterator &ReductionEnd) {
  if ((Branch.getOpcode() != C166::JMPR_EQ &&
       Branch.getOpcode() != C166::JMPR_NE) ||
      !Branch.getOperand(0).isMBB())
    return false;

  auto Previous = previousNonDebug(MBB, Branch.getIterator());
  if (Previous == MBB.end())
    return false;

  Compare = nullptr;
  ReductionEnd = Branch.getIterator();
  if ((Previous->getOpcode() == C166::CMPri3 ||
       Previous->getOpcode() == C166::CMPri16) &&
      isWholePhysicalRegister(Previous->getOperand(0)) &&
      Previous->getOperand(1).isImm() &&
      Previous->getOperand(1).getImm() == 0) {
    Compare = &*Previous;
    Root = Previous->getOperand(0).getReg();
    ReductionEnd = Previous;
    return true;
  }

  if (Previous->getOpcode() != C166::ORrr ||
      !isWholePhysicalRegister(Previous->getOperand(0)))
    return false;

  Root = Previous->getOperand(0).getReg();
  return true;
}

static bool matchWideSubtractChain(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator ReductionEnd,
                                   const TargetRegisterInfo &TRI,
                                   MachineInstr *&LastSubtract,
                                   SmallVectorImpl<Register> &Words) {
  LastSubtract = nullptr;
  unsigned Remaining = 24;
  for (auto Scan = ReductionEnd; Scan != MBB.begin() && Remaining;) {
    --Scan;
    if (Scan->isDebugInstr() || Scan->isMetaInstruction())
      continue;
    --Remaining;
    if (Scan->getOpcode() == C166::SUBCCarryrr) {
      LastSubtract = &*Scan;
      break;
    }
    if (Scan->getOpcode() == C166::MOVrr || Scan->getOpcode() == C166::ORrr ||
        isSimpleStore(*Scan))
      continue;
    break;
  }
  if (!LastSubtract ||
      isInsideExtensionWindow(
          MBB, MachineBasicBlock::const_iterator(LastSubtract->getIterator())))
    return false;

  SmallVector<MachineInstr *, 8> Subtracts;
  MachineInstr *Subtract = LastSubtract;
  while (Subtract->getOpcode() == C166::SUBCCarryrr) {
    Subtracts.push_back(Subtract);
    auto Previous = previousNonDebug(MBB, Subtract->getIterator());
    if (Previous == MBB.end()) {
      Subtract = nullptr;
      break;
    }
    Subtract = &*Previous;
  }
  if (!Subtract || Subtract->getOpcode() != C166::SUBCarryrr)
    return false;

  Subtracts.push_back(Subtract);
  std::reverse(Subtracts.begin(), Subtracts.end());
  if (Subtracts.size() > 63)
    return false;

  for (unsigned Index = 0; Index != Subtracts.size(); ++Index) {
    MachineInstr &MI = *Subtracts[Index];
    if (!isWholePhysicalRegister(MI.getOperand(0)) ||
        !MI.getOperand(1).isReg() || MI.getOperand(1).getReg() != C166::C ||
        !MI.getOperand(2).isReg() ||
        MI.getOperand(2).getReg() != MI.getOperand(0).getReg() ||
        MI.getOperand(2).getSubReg() ||
        (Index == 0) != (MI.getOpcode() == C166::SUBCarryrr) ||
        (Index != 0 &&
         (!MI.getOperand(4).isReg() || MI.getOperand(4).getReg() != C166::C)))
      return false;

    Register Word = MI.getOperand(0).getReg();
    if (llvm::any_of(Words, [&](Register Other) {
          return TRI.regsOverlap(Word, Other);
        }))
      return false;
    Words.push_back(Word);
  }

  return LastSubtract->getOperand(1).isDead();
}

static bool matchWideZeroReduction(MachineBasicBlock &MBB,
                                   MachineInstr &LastSubtract,
                                   MachineBasicBlock::iterator ReductionEnd,
                                   MachineBasicBlock::iterator AfterBranch,
                                   Register Root, ArrayRef<Register> Words,
                                   const TargetRegisterInfo &TRI,
                                   SmallVectorImpl<MachineInstr *> &Logic,
                                   SmallVectorImpl<MachineInstr *> &Stores) {
  DenseMap<Register, ZeroProvenance> Provenance;
  for (unsigned Index = 0; Index != Words.size(); ++Index)
    Provenance[Words[Index]] = {uint64_t(1) << Index, true};

  SmallVector<MachineInstr *, 8> Observers;
  for (auto Scan = std::next(LastSubtract.getIterator()); Scan != ReductionEnd;
       ++Scan) {
    if (Scan->isDebugInstr()) {
      Observers.push_back(&*Scan);
      continue;
    }
    if (Scan->isMetaInstruction())
      continue;

    if (isSimpleStore(*Scan)) {
      for (const MachineOperand &MO : Scan->operands())
        if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical() &&
            MO.getReg() != C166::PSW && MO.getReg() != C166::C)
          return false;
      Observers.push_back(&*Scan);
      Stores.push_back(&*Scan);
      continue;
    }

    if ((Scan->getOpcode() != C166::MOVrr && Scan->getOpcode() != C166::ORrr) ||
        !isWholePhysicalRegister(Scan->getOperand(0)))
      return false;

    Register Destination = Scan->getOperand(0).getReg();
    if (llvm::any_of(Words, [&](Register Word) {
          return TRI.regsOverlap(Destination, Word);
        }))
      return false;

    ZeroProvenance Result;
    if (Scan->getOpcode() == C166::MOVrr) {
      if (!isWholePhysicalRegister(Scan->getOperand(1)))
        return false;
      Result = Provenance.lookup(Scan->getOperand(1).getReg());
    } else {
      if (!isWholePhysicalRegister(Scan->getOperand(1)) ||
          !isWholePhysicalRegister(Scan->getOperand(2)))
        return false;
      ZeroProvenance LHS = Provenance.lookup(Scan->getOperand(1).getReg());
      ZeroProvenance RHS = Provenance.lookup(Scan->getOperand(2).getReg());
      if (LHS.Known && RHS.Known)
        Result = {LHS.Words | RHS.Words, true};
    }
    Provenance[Destination] = Result;
    Logic.push_back(&*Scan);
  }

  uint64_t AllWords = (uint64_t(1) << Words.size()) - 1;
  ZeroProvenance RootValue = Provenance.lookup(Root);
  if (!RootValue.Known || RootValue.Words != AllWords || Logic.empty())
    return false;

  // Every instruction in the reduction must contribute to Root because all of
  // them are removed by the fold.
  SmallVector<Register, 8> Needed(1, Root);
  auto AddNeeded = [&](Register Reg) {
    if (!llvm::is_contained(Needed, Reg))
      Needed.push_back(Reg);
  };
  for (MachineInstr *MI : llvm::reverse(Logic)) {
    Register Destination = MI->getOperand(0).getReg();
    auto NeededPosition = llvm::find(Needed, Destination);
    if (NeededPosition == Needed.end())
      return false;
    Needed.erase(NeededPosition);
    AddNeeded(MI->getOperand(1).getReg());
    if (MI->getOpcode() == C166::ORrr)
      AddNeeded(MI->getOperand(2).getReg());
  }
  if (Needed.size() != Words.size())
    return false;
  for (Register Word : Words)
    if (!llvm::is_contained(Needed, Word))
      return false;

  SmallVector<Register, 8> Temporaries;
  for (MachineInstr *MI : Logic) {
    Register Destination = MI->getOperand(0).getReg();
    if (!llvm::is_contained(Temporaries, Destination))
      Temporaries.push_back(Destination);
  }
  for (Register Temporary : Temporaries) {
    for (MachineInstr *Observer : Observers)
      if (readsPhysicalRegister(*Observer, Temporary, TRI))
        return false;
    if (MBB.computeRegisterLiveness(
            &TRI, Temporary, MachineBasicBlock::const_iterator(AfterBranch)) !=
        MachineBasicBlock::LQR_Dead)
      return false;
  }

  return true;
}

static const AllocaInst *
getCommonStoredAlloca(ArrayRef<MachineInstr *> Stores) {
  const AllocaInst *Allocation = nullptr;
  for (MachineInstr *Store : Stores) {
    if (Store->memoperands_empty())
      return nullptr;
    for (const MachineMemOperand *MMO : Store->memoperands()) {
      if (!MMO->isStore() || MMO->isLoad() || MMO->isVolatile() ||
          MMO->isAtomic())
        return nullptr;
      const Value *MemoryValue = MMO->getValue();
      if (!MemoryValue)
        return nullptr;
      const Value *Object = getUnderlyingObject(MemoryValue);
      const auto *Current = dyn_cast_or_null<AllocaInst>(Object);
      if (!Current || (Allocation && Current != Allocation))
        return nullptr;
      Allocation = Current;
    }
  }
  return Allocation;
}

static bool pathDoesNotObserveAlloca(MachineBasicBlock &Start,
                                     MachineBasicBlock &Fallthrough,
                                     const AllocaInst &Allocation) {
  SmallVector<MachineBasicBlock *, 8> Worklist(1, &Start);
  SmallPtrSet<MachineBasicBlock *, 8> Visited;
  while (!Worklist.empty()) {
    MachineBasicBlock *MBB = Worklist.pop_back_val();
    if (MBB == &Fallthrough)
      return false;
    if (!Visited.insert(MBB).second)
      continue;

    for (MachineInstr &MI : *MBB) {
      if (MI.getFlag(MachineInstr::FrameDestroy))
        continue;
      if (MI.isCall() || MI.isInlineAsm() || MI.hasUnmodeledSideEffects())
        return false;
      if (!MI.mayLoad())
        continue;
      if (MI.memoperands_empty())
        return false;
      for (const MachineMemOperand *MMO : MI.memoperands()) {
        if (!MMO->isLoad())
          continue;
        const Value *MemoryValue = MMO->getValue();
        if (!MemoryValue)
          return false;
        const Value *Object = getUnderlyingObject(MemoryValue);
        if (Object == &Allocation || !isIdentifiedObject(Object))
          return false;
      }
    }
    llvm::append_range(Worklist, MBB->successors());
  }
  return true;
}

static MachineBasicBlock *
getStoreSinkFallthrough(MachineBasicBlock &MBB, MachineInstr &Branch,
                        MachineInstr &LastSubtract,
                        ArrayRef<MachineInstr *> Stores) {
  assert(!Stores.empty() && "expected stores to sink");

  MachineBasicBlock *Fallthrough = MBB.getNextNode();
  MachineBasicBlock *Taken = Branch.getOperand(0).getMBB();
  if (!Fallthrough || Taken == Fallthrough || !MBB.isSuccessor(Fallthrough) ||
      Fallthrough->pred_size() != 1 || *Fallthrough->pred_begin() != &MBB)
    return nullptr;

  for (auto I = std::next(LastSubtract.getIterator());
       I != Branch.getIterator(); ++I)
    if (I->isDebugInstr())
      return nullptr;

  const AllocaInst *Allocation = getCommonStoredAlloca(Stores);
  if (!Allocation ||
      !pathDoesNotObserveAlloca(*Taken, *Fallthrough, *Allocation))
    return nullptr;
  return Fallthrough;
}

static void sinkStoresToFallthrough(MachineBasicBlock &Fallthrough,
                                    ArrayRef<MachineInstr *> Stores) {
  auto Insert = Fallthrough.getFirstNonPHI();
  for (MachineInstr *Store : Stores) {
    for (const MachineOperand &MO : Store->operands()) {
      if (!MO.isReg() || !MO.isUse() || !MO.getReg().isPhysical() ||
          MO.isUndef() || MO.getReg() == C166::R0 || MO.getReg() == C166::PSW ||
          MO.getReg() == C166::C)
        continue;
      if (!Fallthrough.isLiveIn(MO.getReg()))
        Fallthrough.addLiveIn(MO.getReg());
    }
    Fallthrough.splice(Insert, Store->getParent(), Store->getIterator());
  }
}

static bool foldWideSubZeroBranches(MachineFunction &MF,
                                    const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Branch = *I++;
      MachineInstr *Compare = nullptr;
      Register Root;
      MachineBasicBlock::iterator ReductionEnd = Branch.getIterator();
      if (!matchWideZeroBranch(MBB, Branch, Compare, Root, ReductionEnd))
        continue;

      auto AfterBranch = nextNonDebug(MBB, Branch.getIterator());
      auto IsDeadAfterBranch = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterBranch)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterBranch(Root) || !IsDeadAfterBranch(C166::PSW) ||
          !IsDeadAfterBranch(C166::C))
        continue;

      MachineInstr *LastSubtract = nullptr;
      SmallVector<Register, 8> Words;
      if (!matchWideSubtractChain(MBB, ReductionEnd, TRI, LastSubtract, Words))
        continue;

      SmallVector<MachineInstr *, 8> Logic;
      SmallVector<MachineInstr *, 4> Stores;
      if (!matchWideZeroReduction(MBB, *LastSubtract, ReductionEnd, AfterBranch,
                                  Root, Words, TRI, Logic, Stores))
        continue;

      MachineBasicBlock *Fallthrough = nullptr;
      if (!Stores.empty())
        Fallthrough =
            getStoreSinkFallthrough(MBB, Branch, *LastSubtract, Stores);

      if (Stores.empty() || Fallthrough) {
        if (Fallthrough)
          sinkStoresToFallthrough(*Fallthrough, Stores);
        LastSubtract->clearRegisterDeads(C166::PSW);
        for (MachineInstr *MI : Logic)
          MI->eraseFromParent();
        if (Compare)
          Compare->eraseFromParent();
        Changed = true;
        continue;
      }

      unsigned OldSize = TII.getInstSizeInBytes(Branch);
      if (Compare)
        OldSize += TII.getInstSizeInBytes(*Compare);
      for (MachineInstr *MI : Logic)
        OldSize += TII.getInstSizeInBytes(*MI);
      unsigned NewSize =
          TII.get(C166::BMOVPSWreg).getSize() + TII.get(C166::JBreg).getSize();
      if (NewSize >= OldSize)
        continue;

      auto Snapshot =
          BuildMI(MBB, std::next(LastSubtract->getIterator()),
                  MIMetadata(*LastSubtract), TII.get(C166::BMOVPSWreg), Root)
              .addReg(Root, RegState::Undef)
              .addImm(0)
              .addImm(PSWZeroBit);
      markRegisterDefDead(*Snapshot, C166::PSW, TRI);
      markRegisterDefDead(*Snapshot, C166::C, TRI);
      LastSubtract->clearRegisterDeads(C166::PSW);

      MachineInstrBuilder Replacement =
          BuildMI(MBB, Branch, MIMetadata(Branch),
                  TII.get(Branch.getOpcode() == C166::JMPR_EQ ? C166::JBreg
                                                              : C166::JNBreg));
      Replacement.addReg(Root, RegState::Kill)
          .addImm(0)
          .addMBB(Branch.getOperand(0).getMBB());
      Replacement->setFlags(Branch.getFlags());

      auto Resume = std::next(Branch.getIterator());
      for (MachineInstr *MI : Logic)
        MI->eraseFromParent();
      if (Compare)
        Compare->eraseFromParent();
      Branch.eraseFromParent();
      I = Resume;
      Changed = true;
    }
  }

  return Changed;
}

static bool isZeroFlagSettingLogic(const MachineInstr &MI, Register Value) {
  switch (MI.getOpcode()) {
  case C166::XORrr:
  case C166::XORri3:
  case C166::XORri16:
  case C166::ANDrr:
  case C166::ANDri3:
  case C166::ANDri16:
  case C166::ORrr:
  case C166::ORri3:
  case C166::ORri16:
    return MI.getOperand(0).isReg() && MI.getOperand(0).getReg() == Value &&
           !MI.getOperand(0).getSubReg();
  default:
    return false;
  }
}

static bool sinkZeroFlagSettingLogic(MachineFunction &MF,
                                     const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Compare = *I++;
      if ((Compare.getOpcode() != C166::CMPri3 &&
           Compare.getOpcode() != C166::CMPri16) ||
          !Compare.getOperand(0).isReg() ||
          !Compare.getOperand(0).getReg().isPhysical() ||
          Compare.getOperand(0).getSubReg() || !Compare.getOperand(1).isImm() ||
          Compare.getOperand(1).getImm() != 0)
        continue;

      auto Branch = nextNonDebug(MBB, Compare.getIterator());
      if (Branch == MBB.end() || (Branch->getOpcode() != C166::JMPR_EQ &&
                                  Branch->getOpcode() != C166::JMPR_NE))
        continue;

      auto AfterBranch = nextNonDebug(MBB, Branch);
      auto IsDeadAfterBranch = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterBranch)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterBranch(C166::PSW) || !IsDeadAfterBranch(C166::C))
        continue;

      Register Value = Compare.getOperand(0).getReg();
      MachineInstr *Logic = nullptr;
      unsigned Remaining = 12;
      for (auto Scan = Compare.getIterator();
           Scan != MBB.begin() && Remaining;) {
        --Scan;
        if (Scan->isDebugInstr() || Scan->isMetaInstruction())
          continue;
        --Remaining;
        if (Scan->modifiesRegister(Value, &TRI)) {
          if (isZeroFlagSettingLogic(*Scan, Value))
            Logic = &*Scan;
          break;
        }
        if (Scan->readsRegister(Value, &TRI) || Scan->isCall() ||
            Scan->isInlineAsm() || Scan->isTerminator() ||
            Scan->hasUnmodeledSideEffects() ||
            readsPhysicalRegister(*Scan, C166::PSW, TRI) ||
            readsPhysicalRegister(*Scan, C166::C, TRI))
          break;
      }
      if (!Logic ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Logic->getIterator())))
        continue;

      SmallVector<Register, 3> Sources;
      for (const MachineOperand &MO : Logic->operands())
        if (MO.isReg() && MO.isUse() && MO.getReg().isPhysical() &&
            MO.getReg() != C166::PSW && MO.getReg() != C166::C)
          Sources.push_back(MO.getReg());

      bool Invalid = false;
      for (auto Scan = std::next(Logic->getIterator());
           Scan != Compare.getIterator(); ++Scan) {
        if (isExtensionOpcode(Scan->getOpcode()) ||
            isInsideExtensionWindow(MBB,
                                    MachineBasicBlock::const_iterator(Scan))) {
          Invalid = true;
          break;
        }
        if (Scan->isDebugInstr()) {
          Invalid |= readsPhysicalRegister(*Scan, Value, TRI);
          for (Register Source : Sources)
            Invalid |= readsPhysicalRegister(*Scan, Source, TRI);
          continue;
        }
        if (Scan->isMetaInstruction())
          continue;
        if (Scan->isCall() || Scan->isInlineAsm() || Scan->isTerminator() ||
            Scan->hasUnmodeledSideEffects() ||
            readsPhysicalRegister(*Scan, C166::PSW, TRI) ||
            readsPhysicalRegister(*Scan, C166::C, TRI)) {
          Invalid = true;
          break;
        }
        for (Register Source : Sources)
          Invalid |= Scan->modifiesRegister(Source, &TRI);
        if (Invalid)
          break;
      }
      if (Invalid)
        continue;

      MBB.splice(Compare.getIterator(), &MBB, Logic->getIterator());
      if (MachineOperand *Def = Logic->findRegisterDefOperand(Value, &TRI))
        Def->setIsDead(IsDeadAfterBranch(Value));
      copyFlagDefLiveness(*Logic, Compare, TRI);
      Logic->setFlags(Logic->getFlags() | Compare.getFlags());
      Compare.eraseFromParent();
      I = std::next(Branch);
      Changed = true;
    }
  }

  return Changed;
}

// MOV instructions set Z and N from the transferred value. Restrict the fold
// below to opcodes with those documented flag semantics.
static bool moveSetsResultFlags(const MachineInstr &MI, Register Value) {
  switch (MI.getOpcode()) {
  case C166::MOVrr:
  case C166::MOVBrr:
  case C166::MOVBZrr:
  case C166::MOVBSrr:
  case C166::MOVri4:
  case C166::MOVBri4:
  case C166::MOVri16:
  case C166::MOVrm:
  case C166::MOVrm16:
  case C166::MOVrmPostInc:
  case C166::MOVBrm:
  case C166::MOVBrm16:
  case C166::MOVBrmPostInc:
  case C166::MOVgd:
  case C166::MOVabsgd:
  case C166::MOVgsfr:
  case C166::MOVgdDPP1:
  case C166::MOVgdDPP2:
  case C166::MOVBZgd:
  case C166::MOVBZabsgd:
  case C166::MOVBZgdDPP1:
  case C166::MOVBZgdDPP2:
  case C166::MOVBSgd:
  case C166::MOVBSabsgd:
  case C166::MOVBSgdDPP1:
  case C166::MOVBSgdDPP2:
    return MI.getOperand(0).isReg() && MI.getOperand(0).getReg() == Value &&
           !MI.getOperand(0).getSubReg();
  default:
    return false;
  }
}

static std::optional<unsigned> getMoveZeroBranchOpcode(unsigned Opcode) {
  switch (Opcode) {
  case C166::JMPR_EQ:
  case C166::JMPR_NE:
  case C166::JMPA_EQ:
  case C166::JMPA_NE:
    return Opcode;
  case C166::JMPR_SLT:
    return C166::JMPR_N;
  case C166::JMPR_SGE:
    return C166::JMPR_NN;
  case C166::JMPA_SLT:
    return C166::JMPA_N;
  case C166::JMPA_SGE:
    return C166::JMPA_NN;
  default:
    return std::nullopt;
  }
}

static bool foldMoveZeroTests(MachineFunction &MF, const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Move = *I++;
      if (!Move.findRegisterDefOperand(C166::PSW, &TRI) ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Move.getIterator())))
        continue;

      auto Test = nextNonDebug(MBB, Move.getIterator());
      if (Test == MBB.end() ||
          isInsideExtensionWindow(MBB, MachineBasicBlock::const_iterator(Test)))
        continue;

      Register Value;
      bool PreservesValue = false;
      if ((Test->getOpcode() == C166::CMPri3 ||
           Test->getOpcode() == C166::CMPri16) &&
          Test->getOperand(0).isReg() && !Test->getOperand(0).getSubReg() &&
          Test->getOperand(1).isImm() && Test->getOperand(1).getImm() == 0) {
        Value = Test->getOperand(0).getReg();
      } else if ((Test->getOpcode() == C166::ORri3 ||
                  Test->getOpcode() == C166::ORri16) &&
                 Test->getOperand(0).isReg() && Test->getOperand(1).isReg() &&
                 Test->getOperand(0).getReg() == Test->getOperand(1).getReg() &&
                 !Test->getOperand(0).getSubReg() &&
                 !Test->getOperand(1).getSubReg() &&
                 Test->getOperand(2).isImm() &&
                 Test->getOperand(2).getImm() == 0) {
        Value = Test->getOperand(0).getReg();
        PreservesValue = true;
      } else {
        continue;
      }

      if (!Value.isPhysical() || !moveSetsResultFlags(Move, Value))
        continue;

      auto Branch = nextNonDebug(MBB, Test);
      if (Branch == MBB.end())
        continue;
      std::optional<unsigned> ReplacementOpcode =
          getMoveZeroBranchOpcode(Branch->getOpcode());
      if (!ReplacementOpcode)
        continue;

      auto AfterBranch = nextNonDebug(MBB, Branch);
      auto IsDeadAfterBranch = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterBranch)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterBranch(C166::PSW) || !IsDeadAfterBranch(C166::C))
        continue;

      Branch->setDesc(TII.get(*ReplacementOpcode));
      copyPSWDefLiveness(Move, *Test, TRI);
      Move.setFlags(Move.getFlags() | Test->getFlags());
      if (PreservesValue && IsDeadAfterBranch(Value))
        markRegisterDefDead(Move, Value, TRI);
      I = std::next(Branch);
      Test->eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

static bool getIncrementByOne(const MachineInstr &MI, Register &Value) {
  unsigned SourceOperand;
  unsigned ImmediateOperand;
  switch (MI.getOpcode()) {
  case C166::ADDri3:
  case C166::ADDri16:
    SourceOperand = 1;
    ImmediateOperand = 2;
    break;
  case C166::ADDCarryri3:
    SourceOperand = 2;
    ImmediateOperand = 3;
    break;
  default:
    return false;
  }

  if (!isWholePhysicalRegister(MI.getOperand(0)) ||
      !isWholePhysicalRegister(MI.getOperand(SourceOperand)) ||
      MI.getOperand(SourceOperand).getReg() != MI.getOperand(0).getReg() ||
      !MI.getOperand(ImmediateOperand).isImm() ||
      MI.getOperand(ImmediateOperand).getImm() != 1)
    return false;

  Value = MI.getOperand(0).getReg();
  return true;
}

static bool isDecrementByOne(const MachineInstr &MI, Register Value) {
  if (MI.getOpcode() != C166::SUBri3 && MI.getOpcode() != C166::SUBri16)
    return false;
  return isWholePhysicalRegister(MI.getOperand(0)) &&
         MI.getOperand(0).getReg() == Value &&
         isWholePhysicalRegister(MI.getOperand(1)) &&
         MI.getOperand(1).getReg() == Value && MI.getOperand(2).isImm() &&
         MI.getOperand(2).getImm() == 1;
}

static bool foldZeroTerminatedIncrementLoops(MachineFunction &MF,
                                             const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    if (MBB.pred_size() != 2 || MBB.succ_size() != 2 ||
        !llvm::is_contained(MBB.predecessors(), &MBB) ||
        !llvm::is_contained(MBB.successors(), &MBB))
      continue;

    MachineBasicBlock *Preheader = nullptr;
    for (MachineBasicBlock *Pred : MBB.predecessors())
      if (Pred != &MBB)
        Preheader = Pred;
    MachineBasicBlock *Exit = nullptr;
    for (MachineBasicBlock *Succ : MBB.successors())
      if (Succ != &MBB)
        Exit = Succ;
    if (!Preheader || !Exit || Preheader->succ_size() != 1 ||
        *Preheader->succ_begin() != &MBB)
      continue;

    auto Update = MBB.begin();
    while (Update != MBB.end() &&
           (Update->isDebugInstr() || Update->isMetaInstruction()))
      ++Update;
    Register Value;
    if (Update == MBB.end() || !getIncrementByOne(*Update, Value) ||
        Update->isBundledWithPred() || Update->isBundledWithSucc() ||
        isInsideExtensionWindow(MBB, Update))
      continue;

    auto Branch = MBB.getLastNonDebugInstr();
    if (Branch == MBB.end() || Branch->getOpcode() != C166::JMPR_UGE ||
        Branch->getNumExplicitOperands() != 1 ||
        !Branch->getOperand(0).isMBB() ||
        Branch->getOperand(0).getMBB() != &MBB || Branch->isBundledWithPred() ||
        Branch->isBundledWithSucc())
      continue;

    auto Compare = previousNonDebug(MBB, Branch);
    if (Compare == MBB.end() || Compare->getOpcode() != C166::CMPri3 ||
        !isWholePhysicalRegister(Compare->getOperand(0)) ||
        Compare->getOperand(0).getReg() != Value ||
        !Compare->getOperand(1).isImm() ||
        Compare->getOperand(1).getImm() != 1 || Compare->isBundledWithPred() ||
        Compare->isBundledWithSucc() || isInsideExtensionWindow(MBB, Compare))
      continue;

    bool Invalid = false;
    for (auto I = std::next(Update); I != Compare; ++I) {
      if (I->isDebugInstr()) {
        Invalid |= readsPhysicalRegister(*I, Value, TRI);
        continue;
      }
      if (I->isMetaInstruction())
        continue;
      if (I->isCall() || I->isInlineAsm() || I->isTerminator() ||
          I->hasUnmodeledSideEffects() ||
          readsPhysicalRegister(*I, Value, TRI) ||
          I->modifiesRegister(Value, &TRI) ||
          readsPhysicalRegister(*I, C166::PSW, TRI) ||
          readsPhysicalRegister(*I, C166::C, TRI)) {
        Invalid = true;
        break;
      }
    }
    if (Invalid || isLiveInWithAliases(*Exit, Value, TRI) ||
        isLiveInWithAliases(*Exit, C166::PSW, TRI) ||
        isLiveInWithAliases(*Exit, C166::C, TRI))
      continue;

    MachineBasicBlock::iterator Decrement = Preheader->end();
    for (auto I = Preheader->end(); I != Preheader->begin();) {
      --I;
      if (I->isDebugInstr()) {
        if (readsPhysicalRegister(*I, Value, TRI))
          Invalid = true;
        continue;
      }
      if (I->isMetaInstruction())
        continue;
      if (isDecrementByOne(*I, Value)) {
        Decrement = I;
        break;
      }
      if (readsPhysicalRegister(*I, Value, TRI) ||
          I->modifiesRegister(Value, &TRI) ||
          readsPhysicalRegister(*I, C166::PSW, TRI) ||
          readsPhysicalRegister(*I, C166::C, TRI)) {
        Invalid = true;
        break;
      }
    }
    if (Invalid || Decrement == Preheader->end() ||
        Decrement->isBundledWithPred() || Decrement->isBundledWithSucc() ||
        isInsideExtensionWindow(*Preheader, Decrement))
      continue;

    MachineInstrBuilder LoopCompare = BuildMI(MBB, Compare, MIMetadata(*Update),
                                              TII.get(C166::CMPI1ri4), Value)
                                          .addReg(Value, RegState::Kill)
                                          .addImm(0);
    copyPSWDefLiveness(*LoopCompare, *Compare, TRI);
    markRegisterDefDead(*LoopCompare, C166::C, TRI);
    LoopCompare->setFlags(Update->getFlags() | Compare->getFlags());

    MachineInstrBuilder NewBranch =
        BuildMI(MBB, Branch, MIMetadata(*Branch), TII.get(C166::JMPR_NE))
            .addMBB(&MBB);
    NewBranch->setFlags(Branch->getFlags());

    Decrement->eraseFromParent();
    Update->eraseFromParent();
    Compare->eraseFromParent();
    Branch->eraseFromParent();
    Changed = true;
  }

  return Changed;
}

static bool foldLoopCompareUpdates(MachineFunction &MF,
                                   const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Update = *I;
      bool IsDecrement;
      switch (Update.getOpcode()) {
      case C166::SUBri3:
      case C166::SUBri16:
        IsDecrement = true;
        break;
      case C166::ADDri3:
      case C166::ADDri16:
        IsDecrement = false;
        break;
      default:
        ++I;
        continue;
      }

      if (!Update.getOperand(0).isReg() ||
          !Update.getOperand(0).getReg().isPhysical() ||
          Update.getOperand(0).getSubReg() || !Update.getOperand(1).isReg() ||
          Update.getOperand(1).getReg() != Update.getOperand(0).getReg() ||
          Update.getOperand(1).getSubReg() || !Update.getOperand(2).isImm()) {
        ++I;
        continue;
      }
      uint16_t Amount = Update.getOperand(2).getImm();
      if (Amount != 1 && Amount != 2) {
        ++I;
        continue;
      }

      Register Value = Update.getOperand(0).getReg();
      if (isInsideExtensionWindow(MBB, I)) {
        ++I;
        continue;
      }

      auto Compare = std::next(I);
      bool Invalid = false;
      unsigned ScannedInstructions = 0;
      for (; Compare != MBB.end(); ++Compare) {
        if (Compare->isDebugInstr()) {
          Invalid |= readsPhysicalRegister(*Compare, Value, TRI);
          continue;
        }
        if (Compare->isMetaInstruction())
          continue;
        if (++ScannedInstructions > 8) {
          Invalid = true;
          break;
        }

        bool IsMatchingCompare = (Compare->getOpcode() == C166::CMPri3 ||
                                  Compare->getOpcode() == C166::CMPri16) &&
                                 Compare->getOperand(0).isReg() &&
                                 Compare->getOperand(0).getReg() == Value &&
                                 !Compare->getOperand(0).getSubReg() &&
                                 Compare->getOperand(1).isImm();
        if (IsMatchingCompare)
          break;

        if (Compare->isCall() || Compare->isInlineAsm() ||
            Compare->isTerminator() || Compare->hasUnmodeledSideEffects() ||
            isExtensionOpcode(Compare->getOpcode()) ||
            isInsideExtensionWindow(MBB, Compare) ||
            readsPhysicalRegister(*Compare, Value, TRI) ||
            Compare->modifiesRegister(Value, &TRI) ||
            readsPhysicalRegister(*Compare, C166::PSW, TRI) ||
            readsPhysicalRegister(*Compare, C166::C, TRI)) {
          Invalid = true;
          break;
        }
      }
      if (Invalid || Compare == MBB.end() ||
          isInsideExtensionWindow(MBB, Compare)) {
        ++I;
        continue;
      }

      auto Branch = nextNonDebug(MBB, Compare);
      if (Branch == MBB.end() || (Branch->getOpcode() != C166::JMPR_EQ &&
                                  Branch->getOpcode() != C166::JMPR_NE)) {
        ++I;
        continue;
      }

      auto AfterBranch = nextNonDebug(MBB, Branch);
      auto IsDeadAfterBranch = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterBranch)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterBranch(C166::PSW) || !IsDeadAfterBranch(C166::C)) {
        ++I;
        continue;
      }

      uint16_t Immediate = Compare->getOperand(1).getImm();
      // CMPD/CMPI compare the old value, then update it.  Translate the
      // original comparison of the updated value into that form.
      Immediate = IsDecrement ? Immediate + Amount : Immediate - Amount;
      bool HasShortImmediate = isUInt<4>(Immediate);
      unsigned Opcode;
      if (IsDecrement) {
        if (Amount == 1)
          Opcode = HasShortImmediate ? C166::CMPD1ri4 : C166::CMPD1ri16;
        else
          Opcode = HasShortImmediate ? C166::CMPD2ri4 : C166::CMPD2ri16;
      } else if (Amount == 1) {
        Opcode = HasShortImmediate ? C166::CMPI1ri4 : C166::CMPI1ri16;
      } else {
        Opcode = HasShortImmediate ? C166::CMPI2ri4 : C166::CMPI2ri16;
      }

      MachineInstrBuilder Replacement =
          BuildMI(MBB, Compare, MIMetadata(Update), TII.get(Opcode), Value)
              .addReg(Value, RegState::Kill)
              .addImm(Immediate);
      Replacement->getOperand(0).setIsDead(IsDeadAfterBranch(Value));
      markRegisterDefDead(*Replacement, C166::C, TRI);
      Replacement->setFlags(Update.getFlags() | Compare->getFlags());

      Update.eraseFromParent();
      Compare->eraseFromParent();
      Changed = true;
      I = std::next(Branch);
    }
  }

  return Changed;
}

static bool shortenDeadNegativeEqualityCompares(MachineFunction &MF,
                                                const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Compare = *I++;
      if (Compare.getOpcode() != C166::CMPri16 ||
          !Compare.getOperand(0).isReg() ||
          !Compare.getOperand(0).getReg().isPhysical() ||
          Compare.getOperand(0).getSubReg() || !Compare.getOperand(1).isImm() ||
          Compare.isBundledWithPred() || Compare.isBundledWithSucc() ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Compare.getIterator())))
        continue;

      auto Branch = nextNonDebug(MBB, Compare.getIterator());
      if (Branch == MBB.end() || (Branch->getOpcode() != C166::JMPR_EQ &&
                                  Branch->getOpcode() != C166::JMPR_NE))
        continue;

      uint16_t Immediate =
          static_cast<uint16_t>(Compare.getOperand(1).getImm());
      uint16_t Amount = -Immediate;
      if (!isUInt<3>(Amount) || Amount == 0)
        continue;

      Register Value = Compare.getOperand(0).getReg();
      if (Value == C166::R0)
        continue;
      auto AfterBranch = nextNonDebug(MBB, Branch);
      auto IsDeadAfterBranch = [&](Register Reg) {
        return MBB.computeRegisterLiveness(
                   &TRI, Reg, MachineBasicBlock::const_iterator(AfterBranch)) ==
               MachineBasicBlock::LQR_Dead;
      };
      if (!IsDeadAfterBranch(Value) || !IsDeadAfterBranch(C166::PSW) ||
          !IsDeadAfterBranch(C166::C) ||
          hasDebugUseBeforeOverwrite(Compare, Value, TRI))
        continue;

      MachineInstrBuilder Replacement = BuildMI(
          MBB, Compare, MIMetadata(Compare), TII.get(C166::ADDri3), Value);
      Replacement.addReg(Value, RegState::Kill).addImm(Amount);
      Replacement->getOperand(0).setIsDead(true);
      markRegisterDefDead(*Replacement, C166::C, TRI);
      Replacement->setFlags(Compare.getFlags());
      Compare.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

static bool foldLowByteHighBitSets(MachineFunction &MF,
                                   const C166InstrInfo &TII) {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Mask = *I++;
      if (Mask.getOpcode() != C166::ANDri16 ||
          !isWholePhysicalRegister(Mask.getOperand(0)) ||
          !isWholePhysicalRegister(Mask.getOperand(1)) ||
          Mask.getOperand(0).getReg() != Mask.getOperand(1).getReg() ||
          !Mask.getOperand(2).isImm() || Mask.getOperand(2).getImm() != 127 ||
          Mask.isBundledWithPred() || Mask.isBundledWithSucc() ||
          isInsideExtensionWindow(
              MBB, MachineBasicBlock::const_iterator(Mask.getIterator())))
        continue;

      auto Set = nextNonDebug(MBB, Mask.getIterator());
      Register Value = Mask.getOperand(0).getReg();
      Register LowByte = TRI.getSubReg(Value, sub_lo8);
      if (Set == MBB.end() || Set->getOpcode() != C166::BSETreg ||
          Set->getNumExplicitOperands() != 3 ||
          !isWholePhysicalRegister(Set->getOperand(0)) ||
          !isWholePhysicalRegister(Set->getOperand(1)) ||
          Set->getOperand(0).getReg() != Value ||
          Set->getOperand(1).getReg() != Value || !Set->getOperand(2).isImm() ||
          Set->getOperand(2).getImm() != 7 || !LowByte ||
          Set->isBundledWithPred() || Set->isBundledWithSucc() ||
          readsPhysicalRegister(*Set, C166::PSW, TRI) ||
          readsPhysicalRegister(*Set, C166::C, TRI))
        continue;

      MachineInstrBuilder Extension =
          BuildMI(MBB, Mask, MIMetadata(Mask), TII.get(C166::MOVBZrr), Value)
              .addReg(LowByte);
      for (unsigned Operand = Mask.getNumExplicitOperands();
           Operand != Mask.getNumOperands(); ++Operand) {
        const MachineOperand &MO = Mask.getOperand(Operand);
        if (!MO.isReg() || (MO.getReg() != C166::PSW && MO.getReg() != C166::C))
          Extension.add(MO);
      }
      markRegisterDefDead(*Extension, C166::PSW, TRI);
      Extension->setFlags(Mask.getFlags());
      Mask.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

class C166PostRA : public MachineFunctionPass {
public:
  static char ID;

  C166PostRA() : MachineFunctionPass(ID) {
    initializeC166PostRAPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "C166 post-register-allocation optimizations";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const auto &TII =
        *static_cast<const C166InstrInfo *>(MF.getSubtarget().getInstrInfo());
    bool Changed = foldSFRShuttles(MF, TII);
    Changed |= foldSFROperands(MF, TII);
    Changed |= foldMaskedCarryShifts(MF, TII);
    Changed |= foldZeroExtendedWideAdds(MF, TII);
    Changed |= replaceZeroAnds(MF, TII);
    Changed |= removeDeadPhysicalLoads(MF, TII);
    Changed |= foldIndirectMemoryCopies(MF, TII);
    Changed |= foldZeroExtendedByteComparisons(MF, TII);
    Changed |= foldZeroExtendedByteImmediateComparisons(MF, TII);
    Changed |= removeDeadByteExtensions(MF, TII);
    Changed |= narrowDeadByteALU(MF, TII);
    Changed |= removeRedundantSuccessorExtensions(MF, TII);
    Changed |= hoistMovesBeforeRepeatedSuccessorCompares(MF, TII);
    Changed |= removeRedundantSuccessorCompares(MF, TII);
    Changed |= removeEqualityProvenImmediateMoves(MF, TII);
    Changed |= forwardExplicitJumpOnlyBlocks(MF);
    Changed |= foldWideNegationDiamonds(MF, TII);
    Changed |= foldNegativeOneMaterializations(MF, TII);
    Changed |= foldConditionalBooleanOrs(MF, TII);
    Changed |= foldInvertedBitExtractions(MF, TII);
    Changed |= removeRedundantPhysicalImmediates(MF, TII);
    Changed |= foldStoredWideAddChains(MF, TII);
    Changed |= foldPreservedRegisterShuttles(MF, TII);
    Changed |= foldLoopRegisterShuttles(MF, TII);
    Changed |= removeUnreadSpillWordStores(MF, TII);
    Changed |= forwardLiveSpillReloads(MF, TII);
    Changed |= removeUnreadSpillWordStores(MF, TII);
    Changed |= foldCallSpillSequences(MF, TII);
    Changed |= forwardAdjacentWordStoreLoads(MF, TII);
    Changed |= orderSplitWordLoadChains(MF, TII);
    Changed |= formPostIncrementLoadChains(MF, TII);
    Changed |= formSpacedPostIncrementLoadChains(MF, TII);
    Changed |= foldPhysicalIndirectALULoads(MF, TII);
    Changed |= foldWideSubZeroBranches(MF, TII);
    Changed |= formPreDecrementStoreChains(MF, TII);
    Changed |= combineAdjacentAddImmediates(MF, TII);
    Changed |= foldReverseSmallImmediateSubtracts(MF, TII);
    Changed |= shortenNegativeAddImmediates(MF, TII);
    Changed |= foldFullMaskEqualityBranches(MF, TII);
    Changed |= foldCopiedOneBitBranches(MF, TII);
    Changed |= sinkZeroFlagSettingLogic(MF, TII);
    Changed |= foldMoveZeroTests(MF, TII);
    Changed |= foldNegativeOneSignedBranches(MF, TII);
    Changed |= foldXorBitBranches(MF, TII);
    Changed |= foldZeroTerminatedIncrementLoops(MF, TII);
    Changed |= foldLoopCompareUpdates(MF, TII);
    Changed |= shortenDeadNegativeEqualityCompares(MF, TII);
    Changed |= foldLowByteHighBitSets(MF, TII);
    return Changed;
  }
};

} // end anonymous namespace

char C166PostRA::ID = 0;

INITIALIZE_PASS(C166PostRA, DEBUG_TYPE,
                "C166 post-register-allocation optimizations", false, false)

FunctionPass *llvm::createC166PostRAPass() { return new C166PostRA(); }
