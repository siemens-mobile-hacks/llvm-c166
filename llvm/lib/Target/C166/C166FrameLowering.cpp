//===-- C166FrameLowering.cpp - C166 frame lowering ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166FrameLowering.h"
#include "C166.h"
#include "C166CFI.h"
#include "C166InstrInfo.h"
#include "C166MachineFunctionInfo.h"
#include "C166Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/C166TargetParser.h"
#include <optional>

using namespace llvm;

static bool isInterruptHandler(const MachineFunction &MF) {
  return MF.getFunction().getCallingConv() == CallingConv::C166_Interrupt;
}

static bool hasNamedRegisterBank(const MachineFunction &MF) {
  return isInterruptHandler(MF) &&
         MF.getFunction().hasFnAttribute("c166-register-bank");
}

bool C166FrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return MF.getFrameInfo().hasVarSizedObjects() ||
         MF.getInfo<C166MachineFunctionInfo>()->needsStableFramePointer();
}

void C166FrameLowering::determineCalleeSaves(MachineFunction &MF,
                                             BitVector &SavedRegs,
                                             RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  if (hasFP(MF))
    SavedRegs.set(C166::R6);
}

void C166FrameLowering::orderFrameObjects(
    const MachineFunction &MF, SmallVectorImpl<int> &ObjectsToAllocate) const {
  if (ObjectsToAllocate.size() < 2)
    return;

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  DenseMap<int, uint64_t> ZeroOffsetUses;
  for (int FI : ObjectsToAllocate)
    ZeroOffsetUses[FI] = 0;

  for (const MachineBasicBlock &MBB : MF)
    for (const MachineInstr &MI : MBB) {
      if (MI.isMetaInstruction())
        continue;
      for (unsigned I = 0; I != MI.getNumOperands(); ++I) {
        const MachineOperand &MO = MI.getOperand(I);
        if (MO.isFI()) {
          auto Use = ZeroOffsetUses.find(MO.getIndex());
          if (Use != ZeroOffsetUses.end() &&
              (I + 1 == MI.getNumOperands() || !MI.getOperand(I + 1).isImm() ||
               MI.getOperand(I + 1).getImm() == 0))
            ++Use->second;
        }
      }
    }

  // R0-relative accesses use a two-byte instruction only at displacement
  // zero.  For every other displacement they need the four-byte form.  As the
  // stack grows down, the object at the end of this list receives offset zero.
  // Put the object with the most accesses to its own offset zero there; an
  // access to another offset within the object cannot use the short form.
  llvm::stable_sort(ObjectsToAllocate, [&](int A, int B) {
    if (ZeroOffsetUses.lookup(A) != ZeroOffsetUses.lookup(B))
      return ZeroOffsetUses.lookup(A) < ZeroOffsetUses.lookup(B);
    return MFI.getObjectAlign(A) < MFI.getObjectAlign(B);
  });
}

static void markPSWDefDead(MachineInstr &MI) {
  for (MachineOperand &Operand : MI.operands())
    if (Operand.isReg() && Operand.isDef() && Operand.getReg() == C166::PSW)
      Operand.setIsDead(true);
}

static std::optional<uint64_t> getEntryStackAllocation(const MachineInstr &MI) {
  if (!MI.getFlag(MachineInstr::FrameSetup))
    return std::nullopt;
  if (MI.getOpcode() == C166::MOVmrPreDec && MI.getOperand(0).isReg() &&
      MI.getOperand(0).getReg() == C166::R0)
    return 2;
  if ((MI.getOpcode() == C166::SUBri3 || MI.getOpcode() == C166::SUBri16) &&
      MI.getOperand(0).isReg() && MI.getOperand(0).getReg() == C166::R0 &&
      MI.getOperand(1).isReg() && MI.getOperand(1).getReg() == C166::R0 &&
      MI.getOperand(2).isImm() && MI.getOperand(2).getImm() >= 0)
    return MI.getOperand(2).getImm();
  return std::nullopt;
}

static void hoistEntryFixedStackLoad(MachineFunction &MF,
                                     const TargetRegisterInfo &TRI) {
  static constexpr unsigned MaxInstructions = 32;

  MachineFrameInfo &MFI = MF.getFrameInfo();
  if (MF.needsFrameMoves() || isInterruptHandler(MF) || !MFI.getStackSize() ||
      MF.getSubtarget().getFrameLowering()->hasFP(MF))
    return;

  MachineBasicBlock &MBB = MF.front();
  auto Insert = MBB.begin();
  while (Insert != MBB.end() && Insert->isMetaInstruction())
    ++Insert;

  uint64_t Allocated = 0;
  unsigned Scanned = 0;
  for (auto I = Insert; I != MBB.end() && Scanned != MaxInstructions; ++I) {
    MachineInstr &MI = *I;
    if (MI.isMetaInstruction())
      continue;
    ++Scanned;

    if (MI.getOpcode() == C166::MOVfi && MI.getOperand(0).isReg() &&
        MI.getOperand(0).getReg().isPhysical() && MI.getOperand(1).isFI() &&
        MI.getOperand(2).isImm() && !MI.hasOrderedMemoryRef()) {
      int FI = MI.getOperand(1).getIndex();
      Register Destination = MI.getOperand(0).getReg();
      int64_t ObjectOffset =
          MFI.getObjectOffset(FI) + MI.getOperand(2).getImm();
      const MachineOperand *PSWDef = MI.findRegisterDefOperand(C166::PSW, &TRI);
      if (!MFI.isFixedObjectIndex(FI) || !MFI.isImmutableObjectIndex(FI) ||
          ObjectOffset != 0 || Allocated != MFI.getStackSize() || !PSWDef ||
          !PSWDef->isDead())
        return;

      for (auto Before = Insert; Before != I; ++Before) {
        if (Before->isMetaInstruction())
          continue;
        if (Before->readsRegister(Destination, &TRI) ||
            Before->modifiesRegister(Destination, &TRI))
          return;

        if (getEntryStackAllocation(*Before))
          continue;
        if (Before->mayLoadOrStore() || Before->isCall() ||
            Before->isInlineAsm() || Before->isTerminator() ||
            Before->hasUnmodeledSideEffects() ||
            Before->readsRegister(C166::PSW, &TRI) ||
            Before->readsRegister(C166::R0, &TRI) ||
            Before->modifiesRegister(C166::R0, &TRI))
          return;
      }

      MI.getOperand(2).setImm(MI.getOperand(2).getImm() - MFI.getStackSize());
      MBB.splice(Insert, &MBB, I);
      return;
    }

    if (std::optional<uint64_t> Amount = getEntryStackAllocation(MI)) {
      Allocated += *Amount;
      continue;
    }
    if (MI.mayLoadOrStore() || MI.isCall() || MI.isInlineAsm() ||
        MI.isTerminator() || MI.hasUnmodeledSideEffects() ||
        MI.readsRegister(C166::R0, &TRI) || MI.modifiesRegister(C166::R0, &TRI))
      return;
  }
}

static bool hasLivePSWDef(const MachineInstr &MI) {
  return llvm::any_of(MI.operands(), [](const MachineOperand &Operand) {
    return Operand.isReg() && Operand.isDef() &&
           Operand.getReg() == C166::PSW && !Operand.isDead();
  });
}

// A callee-saved scratch register could make the prologue larger than an
// access chain saves.
static constexpr MCPhysReg FrameAccessScratchCandidates[] = {
    C166::R1,  C166::R2,  C166::R3,  C166::R4,  C166::R5, C166::R10,
    C166::R11, C166::R12, C166::R13, C166::R14, C166::R15};

static Register findFrameAccessScratch(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator InsertBefore,
                                       ArrayRef<MachineInstr *> Accesses,
                                       const TargetRegisterInfo &TRI) {
  for (MCRegister Candidate : FrameAccessScratchCandidates) {
    bool UsedBySequence =
        llvm::any_of(Accesses, [&](const MachineInstr *Access) {
          return llvm::any_of(
              Access->operands(), [&](const MachineOperand &Operand) {
                return Operand.isReg() && Operand.getReg() &&
                       TRI.regsOverlap(Candidate, Operand.getReg());
              });
        });
    if (UsedBySequence)
      continue;
    if (MBB.computeRegisterLiveness(
            &TRI, Candidate, MachineBasicBlock::const_iterator(InsertBefore)) ==
        MachineBasicBlock::LQR_Dead)
      return Candidate;
  }
  return Register();
}

static Register findFrameAccessScratchAcross(MachineBasicBlock &MBB,
                                             MachineBasicBlock::iterator First,
                                             MachineBasicBlock::iterator Last,
                                             const TargetRegisterInfo &TRI) {
  for (MCRegister Candidate : FrameAccessScratchCandidates) {
    if (MBB.computeRegisterLiveness(&TRI, Candidate,
                                    MachineBasicBlock::const_iterator(First)) !=
        MachineBasicBlock::LQR_Dead)
      continue;

    bool Used = false;
    for (auto I = First;; ++I) {
      Used |= llvm::any_of(I->operands(), [&](const MachineOperand &Operand) {
        return Operand.isReg() && Operand.getReg() &&
               TRI.regsOverlap(Candidate, Operand.getReg());
      });
      if (I == Last)
        break;
    }
    if (!Used)
      return Candidate;
  }
  return Register();
}

static int64_t getFrameAccessOffset(const MachineFunction &MF,
                                    const MachineInstr &MI,
                                    unsigned FrameOperand,
                                    unsigned DisplacementOperand,
                                    bool HasFinalLayout) {
  int64_t Offset = MI.getOperand(DisplacementOperand).getImm();
  if (HasFinalLayout) {
    const MachineFrameInfo &MFI = MF.getFrameInfo();
    Offset += MFI.getObjectOffset(MI.getOperand(FrameOperand).getIndex()) +
              MFI.getStackSize();
  }
  return Offset;
}

static void removeUnreferencedFrameObjects(MachineFunction &MF) {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const int ObjectCount = MFI.getObjectIndexEnd();
  SmallBitVector Referenced(ObjectCount);

  for (const MachineBasicBlock &MBB : MF)
    for (const MachineInstr &MI : MBB)
      for (const MachineOperand &Operand : MI.operands())
        if (Operand.isFI() && Operand.getIndex() >= 0)
          Referenced.set(Operand.getIndex());

  for (int FrameIndex = 0; FrameIndex != ObjectCount; ++FrameIndex) {
    if (Referenced.test(FrameIndex) || MFI.isDeadObjectIndex(FrameIndex) ||
        MFI.isVariableSizedObjectIndex(FrameIndex) ||
        MFI.isSpillSlotObjectIndex(FrameIndex) ||
        MFI.isStatepointSpillSlotObjectIndex(FrameIndex) ||
        MFI.isCalleeSavedObjectIndex(FrameIndex) ||
        MFI.isObjectPreAllocated(FrameIndex) ||
        MFI.getStackID(FrameIndex) != TargetStackID::Default ||
        MFI.getObjectSSPLayout(FrameIndex) != MachineFrameInfo::SSPLK_None ||
        (MFI.hasStackProtectorIndex() &&
         MFI.getStackProtectorIndex() == FrameIndex) ||
        (MFI.hasFunctionContextIndex() &&
         MFI.getFunctionContextIndex() == FrameIndex))
      continue;
    MFI.RemoveStackObject(FrameIndex);
  }
}

static void splitDeadFrameWordPairs(MachineFunction &MF,
                                    const C166InstrInfo &TII,
                                    const TargetRegisterInfo &TRI) {
  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &MI = *I++;
      const bool IsLoad = MI.getOpcode() == C166::FRAMELOAD32;
      const bool IsStore = MI.getOpcode() == C166::FRAMESTORE32;
      if (!IsLoad && !IsStore)
        continue;
      const bool DeadPSW = !hasLivePSWDef(MI) ||
                           TII.isRegisterOverwrittenBeforeUse(MI, C166::PSW);
      if (!DeadPSW ||
          llvm::any_of(MI.memoperands(), [](const MachineMemOperand *MMO) {
            return MMO->isVolatile() || MMO->isAtomic();
          }))
        continue;

      const unsigned FrameOperand = IsLoad ? 1 : 0;
      const unsigned DisplacementOperand = IsLoad ? 2 : 1;
      const unsigned RegisterOperand = IsLoad ? 0 : 2;
      Register Pair = MI.getOperand(RegisterOperand).getReg();
      int64_t Displacement = MI.getOperand(DisplacementOperand).getImm();
      if (!Pair.isPhysical() || !isUInt<16>(Displacement + 2))
        continue;

      auto EmitWord = [&](Register Word, int64_t WordDisplacement) {
        MachineInstrBuilder MIB;
        if (IsLoad) {
          MIB = BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(C166::MOVfi), Word)
                    .add(MI.getOperand(FrameOperand))
                    .addImm(WordDisplacement);
        } else {
          MIB = BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(C166::MOVfiStore))
                    .add(MI.getOperand(FrameOperand))
                    .addImm(WordDisplacement)
                    .addReg(Word);
        }
        MIB.cloneMemRefs(MI).setMIFlags(MI.getFlags());
        markPSWDefDead(*MIB);
      };

      EmitWord(TRI.getSubReg(Pair, sub_lo16), Displacement);
      EmitWord(TRI.getSubReg(Pair, sub_hi16), Displacement + 2);
      MI.eraseFromParent();
    }
  }
}

static bool getFrameWordAccess(const MachineInstr &MI, bool &IsLoad,
                               int &FrameIndex, int64_t &Displacement) {
  IsLoad = MI.getOpcode() == C166::MOVfi;
  const bool IsStore = MI.getOpcode() == C166::MOVfiStore;
  if (!IsLoad && !IsStore)
    return false;

  const unsigned FrameOperand = IsLoad ? 1 : 0;
  const unsigned DisplacementOperand = IsLoad ? 2 : 1;
  if (!MI.getOperand(FrameOperand).isFI() ||
      !MI.getOperand(DisplacementOperand).isImm())
    return false;
  FrameIndex = MI.getOperand(FrameOperand).getIndex();
  Displacement = MI.getOperand(DisplacementOperand).getImm();
  return true;
}

static void compactDeadSpillWords(MachineFunction &MF,
                                  const C166InstrInfo &TII) {
  if (!MF.getFunction().hasOptSize())
    return;

  const TargetRegisterInfo &TRI = TII.getRegisterInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Load = *I++;
      bool IsLoad;
      int FrameIndex;
      int64_t Displacement;
      if (!getFrameWordAccess(Load, IsLoad, FrameIndex, Displacement) ||
          !IsLoad || !MFI.isSpillSlotObjectIndex(FrameIndex) ||
          (!Load.memoperands_empty() && Load.hasOrderedMemoryRef()) ||
          !Load.getOperand(0).isReg() ||
          !Load.getOperand(0).getReg().isPhysical() ||
          Load.getOperand(0).getSubReg())
        continue;

      Register Value = Load.getOperand(0).getReg();
      const MachineOperand *PSWDef =
          Load.findRegisterDefOperand(C166::PSW, &TRI);
      if (!TII.isRegisterOverwrittenBeforeUse(Load, Value) ||
          (PSWDef && !PSWDef->isDead() &&
           !TII.isRegisterOverwrittenBeforeUse(Load, C166::PSW)))
        continue;
      Load.eraseFromParent();
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &Store = *I++;
      bool IsLoad;
      int FrameIndex;
      int64_t Displacement;
      if (!getFrameWordAccess(Store, IsLoad, FrameIndex, Displacement) ||
          IsLoad || !MFI.isSpillSlotObjectIndex(FrameIndex) ||
          (!Store.memoperands_empty() && Store.hasOrderedMemoryRef()))
        continue;

      const MachineOperand *PSWDef =
          Store.findRegisterDefOperand(C166::PSW, &TRI);
      if (PSWDef && !PSWDef->isDead() &&
          !TII.isRegisterOverwrittenBeforeUse(Store, C166::PSW))
        continue;

      bool Read = false;
      bool UnknownAccess = false;
      for (const MachineBasicBlock &OtherMBB : MF) {
        for (const MachineInstr &Access : OtherMBB) {
          if (&Access == &Store)
            continue;
          bool OtherIsLoad;
          int OtherFrameIndex;
          int64_t OtherDisplacement;
          if (getFrameWordAccess(Access, OtherIsLoad, OtherFrameIndex,
                                 OtherDisplacement)) {
            if (OtherFrameIndex == FrameIndex && OtherIsLoad &&
                OtherDisplacement == Displacement)
              Read = true;
            continue;
          }
          if (llvm::any_of(Access.operands(), [&](const MachineOperand &MO) {
                return MO.isFI() && MO.getIndex() == FrameIndex;
              }))
            UnknownAccess = true;
        }
      }
      if (!Read && !UnknownAccess)
        Store.eraseFromParent();
    }
  }

  for (int FrameIndex = 0; FrameIndex != MFI.getObjectIndexEnd();
       ++FrameIndex) {
    if (MFI.isDeadObjectIndex(FrameIndex) ||
        !MFI.isSpillSlotObjectIndex(FrameIndex))
      continue;

    int64_t UsedSize = 0;
    bool UnknownAccess = false;
    for (const MachineBasicBlock &MBB : MF) {
      for (const MachineInstr &MI : MBB) {
        bool IsLoad;
        int AccessFrameIndex;
        int64_t Displacement;
        if (getFrameWordAccess(MI, IsLoad, AccessFrameIndex, Displacement)) {
          if (AccessFrameIndex == FrameIndex) {
            if (Displacement < 0) {
              UnknownAccess = true;
              continue;
            }
            UsedSize = std::max(UsedSize, Displacement + 2);
          }
          continue;
        }
        if (llvm::any_of(MI.operands(), [&](const MachineOperand &MO) {
              return MO.isFI() && MO.getIndex() == FrameIndex;
            }))
          UnknownAccess = true;
      }
    }
    if (!UnknownAccess && UsedSize && UsedSize < MFI.getObjectSize(FrameIndex))
      MFI.setObjectSize(FrameIndex, UsedSize);
  }
}

static void formFrameAccessChains(MachineFunction &MF, const C166InstrInfo &TII,
                                  const TargetRegisterInfo &TRI,
                                  bool HasFinalLayout) {
  // Three word accesses are enough to cover address materialization without
  // growing the code. Earlier MOV flag definitions are overwritten by the
  // next access; the final definition must be dead before reordering.
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      const bool IsLoad = I->getOpcode() == C166::MOVfi;
      const bool IsStore = I->getOpcode() == C166::MOVfiStore;
      if (!IsLoad && !IsStore) {
        ++I;
        continue;
      }

      const unsigned FrameOperand = IsLoad ? 1 : 0;
      const unsigned DisplacementOperand = IsLoad ? 2 : 1;
      const unsigned RegisterOperand = IsLoad ? 0 : 2;
      if (!I->getOperand(RegisterOperand).getReg().isPhysical() ||
          !I->getOperand(FrameOperand).isFI() ||
          !I->getOperand(DisplacementOperand).isImm()) {
        ++I;
        continue;
      }

      const int FrameIndex = I->getOperand(FrameOperand).getIndex();
      if (IsStore && !HasFinalLayout &&
          (!MF.getFunction().hasOptSize() ||
           MFI.isSpillSlotObjectIndex(FrameIndex))) {
        ++I;
        continue;
      }
      // Non-size builds retain allocator spill stores until the late target
      // pass can account for every resolved stack access.
      if (IsStore && HasFinalLayout && !MF.getFunction().hasOptSize() &&
          MFI.isSpillSlotObjectIndex(FrameIndex)) {
        ++I;
        continue;
      }
      SmallVector<MachineInstr *, 4> Accesses;
      auto End = I;
      while (
          End != MBB.end() && End->getOpcode() == I->getOpcode() &&
          End->getOperand(RegisterOperand).getReg().isPhysical() &&
          End->getOperand(FrameOperand).isFI() &&
          (HasFinalLayout ||
           End->getOperand(FrameOperand).getIndex() == FrameIndex) &&
          End->getOperand(DisplacementOperand).isImm() &&
          llvm::none_of(End->memoperands(), [](const MachineMemOperand *MMO) {
            return MMO->isVolatile() || MMO->isAtomic();
          })) {
        Accesses.push_back(&*End++);
      }

      if (Accesses.empty()) {
        ++I;
        continue;
      }
      if (Accesses.size() < 3) {
        I = End;
        continue;
      }
      if (hasLivePSWDef(*Accesses.back())) {
        I = End;
        continue;
      }

      bool IndependentLoads = true;
      if (IsLoad)
        for (unsigned Left = 0; Left != Accesses.size(); ++Left)
          for (unsigned Right = Left + 1; Right != Accesses.size(); ++Right)
            IndependentLoads &= !TRI.regsOverlap(
                Accesses[Left]->getOperand(RegisterOperand).getReg(),
                Accesses[Right]->getOperand(RegisterOperand).getReg());
      if (!IndependentLoads) {
        I = End;
        continue;
      }

      llvm::sort(
          Accesses, [&](const MachineInstr *Left, const MachineInstr *Right) {
            return getFrameAccessOffset(MF, *Left, FrameOperand,
                                        DisplacementOperand, HasFinalLayout) <
                   getFrameAccessOffset(MF, *Right, FrameOperand,
                                        DisplacementOperand, HasFinalLayout);
          });
      bool Contiguous =
          getFrameAccessOffset(MF, *Accesses.front(), FrameOperand,
                               DisplacementOperand, HasFinalLayout) >= 0;
      for (unsigned Index = 1; Index != Accesses.size(); ++Index)
        Contiguous &=
            getFrameAccessOffset(MF, *Accesses[Index], FrameOperand,
                                 DisplacementOperand, HasFinalLayout) ==
            getFrameAccessOffset(MF, *Accesses[Index - 1], FrameOperand,
                                 DisplacementOperand, HasFinalLayout) +
                2;
      MachineInstr *AddressAccess = Accesses[IsLoad ? 0 : Accesses.size() - 1];
      const int64_t AddressOffset =
          AddressAccess->getOperand(DisplacementOperand).getImm() +
          (IsStore ? 2 : 0);
      const int64_t FinalAddress =
          getFrameAccessOffset(MF, *AddressAccess, FrameOperand,
                               DisplacementOperand, HasFinalLayout) +
          (IsStore ? 2 : 0);
      Contiguous &= isUInt<16>(AddressOffset) &&
                    (!HasFinalLayout || isUInt<14>(FinalAddress));
      const bool AddressClobbersCarry = !HasFinalLayout || FinalAddress != 0;
      const bool CarryIsDead =
          !AddressClobbersCarry ||
          MBB.computeRegisterLiveness(&TRI, C166::C,
                                      MachineBasicBlock::const_iterator(I)) ==
              MachineBasicBlock::LQR_Dead;
      Register Scratch = Contiguous && CarryIsDead
                             ? findFrameAccessScratch(MBB, I, Accesses, TRI)
                             : Register();
      if (!Scratch) {
        I = End;
        continue;
      }

      MachineInstr &InsertBefore = *I;
      MachineInstrBuilder Address =
          BuildMI(MBB, InsertBefore, InsertBefore.getDebugLoc(),
                  TII.get(C166::LEAfi), Scratch)
              .addFrameIndex(AddressAccess->getOperand(FrameOperand).getIndex())
              .addImm(AddressOffset);
      markPSWDefDead(*Address);

      if (IsLoad) {
        for (auto [Index, Load] : llvm::enumerate(Accesses)) {
          Register Destination = Load->getOperand(RegisterOperand).getReg();
          MachineInstrBuilder MIB;
          if (Index + 1 == Accesses.size()) {
            MIB = BuildMI(MBB, InsertBefore, Load->getDebugLoc(),
                          TII.get(C166::MOVrm), Destination)
                      .addReg(Scratch, RegState::Kill);
          } else {
            MIB = BuildMI(MBB, InsertBefore, Load->getDebugLoc(),
                          TII.get(C166::MOVrmPostInc), Destination)
                      .addDef(Scratch)
                      .addReg(Scratch, RegState::Kill);
          }
          MIB.cloneMemRefs(*Load).setMIFlags(Load->getFlags());
          markPSWDefDead(*MIB);
        }
      } else {
        for (auto Store = Accesses.rbegin(); Store != Accesses.rend();
             ++Store) {
          MachineOperand Source = (*Store)->getOperand(RegisterOperand);
          Source.setIsKill(false);
          MachineInstrBuilder MIB =
              BuildMI(MBB, InsertBefore, (*Store)->getDebugLoc(),
                      TII.get(C166::MOVmrPreDec), Scratch)
                  .addReg(Scratch, RegState::Kill)
                  .add(Source);
          MIB.cloneMemRefs(**Store).setMIFlags((*Store)->getFlags());
          markPSWDefDead(*MIB);
          if (std::next(Store) == Accesses.rend())
            MIB->getOperand(0).setIsDead(true);
        }
      }

      for (MachineInstr *Access : Accesses)
        Access->eraseFromParent();
      I = End;
    }
  }
}

static unsigned frameAddressMaterializationSize(int64_t Offset) {
  if (!Offset)
    return 2;
  if (Offset <= 15)
    return 4;
  return 6;
}

static void formSpacedFrameAccessChains(MachineFunction &MF,
                                        const C166InstrInfo &TII,
                                        const TargetRegisterInfo &TRI) {
  // Keep a post-increment load or pre-decrement store cursor through a short
  // straight-line region. The accesses stay in place, so intervening data and
  // flag dependencies are unchanged.
  static constexpr unsigned MaxSpanInstructions = 32;
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      const bool IsLoad = I->getOpcode() == C166::MOVfi;
      const bool IsStore = I->getOpcode() == C166::MOVfiStore;
      if (!IsLoad && !IsStore) {
        ++I;
        continue;
      }

      const unsigned FrameOperand = IsLoad ? 1 : 0;
      const unsigned DisplacementOperand = IsLoad ? 2 : 1;
      const unsigned RegisterOperand = IsLoad ? 0 : 2;
      auto IsEligibleAccess = [&](const MachineInstr &MI) {
        return MI.getOpcode() == I->getOpcode() &&
               MI.getOperand(RegisterOperand).getReg().isPhysical() &&
               MI.getOperand(FrameOperand).isFI() &&
               (!IsStore || !MFI.isSpillSlotObjectIndex(
                                MI.getOperand(FrameOperand).getIndex())) &&
               MI.getOperand(DisplacementOperand).isImm() &&
               llvm::none_of(MI.memoperands(),
                             [](const MachineMemOperand *MMO) {
                               return MMO->isVolatile() || MMO->isAtomic();
                             });
      };
      if (!IsEligibleAccess(*I)) {
        ++I;
        continue;
      }

      SmallVector<MachineInstr *, 4> Accesses{&*I};
      int64_t ExpectedOffset = getFrameAccessOffset(MF, *I, FrameOperand,
                                                    DisplacementOperand, true) +
                               (IsLoad ? 2 : -2);
      auto Last = I;
      unsigned Span = 0;
      for (auto Scan = std::next(I);
           Scan != MBB.end() && Span != MaxSpanInstructions; ++Scan) {
        if (Scan->isMetaInstruction())
          continue;
        ++Span;
        if (Scan->isCall() || Scan->isInlineAsm() || Scan->isTerminator() ||
            Scan->hasUnmodeledSideEffects() ||
            Scan->modifiesRegister(C166::R0, &TRI))
          break;
        if (Scan->getOpcode() != I->getOpcode())
          continue;
        if (!IsEligibleAccess(*Scan))
          break;

        int64_t Offset = getFrameAccessOffset(MF, *Scan, FrameOperand,
                                              DisplacementOperand, true);
        if (Offset != ExpectedOffset)
          break;
        Accesses.push_back(&*Scan);
        Last = Scan;
        ExpectedOffset += IsLoad ? 2 : -2;
      }

      const int64_t FirstOffset = getFrameAccessOffset(
          MF, *Accesses.front(), FrameOperand, DisplacementOperand, true);
      const int64_t AddressOffset =
          Accesses.front()->getOperand(DisplacementOperand).getImm() +
          (IsStore ? 2 : 0);
      const int64_t FinalAddress = FirstOffset + (IsStore ? 2 : 0);
      unsigned OriginalSize = 0;
      for (const MachineInstr *Access : Accesses)
        OriginalSize += getFrameAccessOffset(MF, *Access, FrameOperand,
                                             DisplacementOperand, true)
                            ? 4
                            : 2;
      const unsigned ReplacementSize =
          frameAddressMaterializationSize(FinalAddress) + 2 * Accesses.size();
      if (ReplacementSize >= OriginalSize || !isUInt<16>(AddressOffset) ||
          !isUInt<14>(FinalAddress) ||
          (FinalAddress != 0 &&
           MBB.computeRegisterLiveness(&TRI, C166::C,
                                       MachineBasicBlock::const_iterator(I)) !=
               MachineBasicBlock::LQR_Dead)) {
        ++I;
        continue;
      }

      Register Scratch = findFrameAccessScratchAcross(MBB, I, Last, TRI);
      if (!Scratch) {
        ++I;
        continue;
      }

      MachineInstrBuilder Address =
          BuildMI(MBB, I, I->getDebugLoc(), TII.get(C166::LEAfi), Scratch)
              .addFrameIndex(
                  Accesses.front()->getOperand(FrameOperand).getIndex())
              .addImm(AddressOffset);
      markPSWDefDead(*Address);

      for (auto [Index, Access] : llvm::enumerate(Accesses)) {
        MachineInstrBuilder MIB;
        if (IsLoad) {
          Register Destination = Access->getOperand(RegisterOperand).getReg();
          if (Index + 1 == Accesses.size()) {
            MIB = BuildMI(MBB, *Access, Access->getDebugLoc(),
                          TII.get(C166::MOVrm), Destination)
                      .addReg(Scratch, RegState::Kill);
          } else {
            MIB = BuildMI(MBB, *Access, Access->getDebugLoc(),
                          TII.get(C166::MOVrmPostInc), Destination)
                      .addDef(Scratch)
                      .addReg(Scratch, RegState::Kill);
          }
        } else {
          MIB = BuildMI(MBB, *Access, Access->getDebugLoc(),
                        TII.get(C166::MOVmrPreDec), Scratch)
                    .addReg(Scratch, RegState::Kill)
                    .add(Access->getOperand(RegisterOperand));
          if (Index + 1 == Accesses.size())
            MIB->getOperand(0).setIsDead(true);
        }
        MIB.cloneMemRefs(*Access).setMIFlags(Access->getFlags());
        if (!hasLivePSWDef(*Access))
          markPSWDefDead(*MIB);
      }

      auto Resume = std::next(Last);
      for (MachineInstr *Access : Accesses)
        Access->eraseFromParent();
      I = Resume;
    }
  }
}

void C166FrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  if (MF.getTarget().getOptLevel() == CodeGenOptLevel::None)
    return;
  const auto &Subtarget = MF.getSubtarget<C166Subtarget>();
  removeUnreferencedFrameObjects(MF);
  splitDeadFrameWordPairs(MF, *Subtarget.getInstrInfo(),
                          *Subtarget.getRegisterInfo());
  compactDeadSpillWords(MF, *Subtarget.getInstrInfo());
  formFrameAccessChains(MF, *Subtarget.getInstrInfo(),
                        *Subtarget.getRegisterInfo(), false);
}

void C166FrameLowering::processFunctionBeforeFrameIndicesReplaced(
    MachineFunction &MF, RegScavenger *RS) const {
  if (MF.getTarget().getOptLevel() == CodeGenOptLevel::None)
    return;
  const auto &Subtarget = MF.getSubtarget<C166Subtarget>();
  hoistEntryFixedStackLoad(MF, *Subtarget.getRegisterInfo());
  formFrameAccessChains(MF, *Subtarget.getInstrInfo(),
                        *Subtarget.getRegisterInfo(), true);
  formSpacedFrameAccessChains(MF, *Subtarget.getInstrInfo(),
                              *Subtarget.getRegisterInfo());
}

bool C166FrameLowering::needsFrameIndexResolution(
    const MachineFunction &MF) const {
  // Frame-index resolution also removes non-reserved call-frame pseudos.  A
  // leaf-sized caller may have an outgoing area but no local stack objects,
  // so the generic stack-object-only criterion is insufficient here.
  return TargetFrameLowering::needsFrameIndexResolution(MF) ||
         MF.getFrameInfo().adjustsStack();
}

bool C166FrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *TRI,
    std::vector<CalleeSavedInfo> &CSI) const {
  if (!isInterruptHandler(MF))
    return false;

  // Interrupt callee-saves are pushed on the CPU system stack, not reserved
  // in the R0 user-stack frame.  Fixed spill objects describe their actual
  // entry-SP-relative positions to PEI without increasing user StackSize.
  int64_t Offset = hasNamedRegisterBank(MF) ? -4 : -2;
  for (CalleeSavedInfo &Info : CSI) {
    int FrameIndex = MF.getFrameInfo().CreateFixedSpillStackObject(2, Offset);
    Info.setFrameIdx(FrameIndex);
    Offset -= 2;
  }
  return true;
}

static void emitR0CFI(MachineFunction &MF, MachineBasicBlock &MBB,
                      MachineBasicBlock::iterator I, const DebugLoc &DL,
                      const C166InstrInfo &TII, uint64_t CallerOffset,
                      MachineInstr::MIFlag Flag) {
  if (!MF.needsFrameMoves())
    return;
  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  const unsigned DwarfR0 = MRI->getDwarfRegNum(C166::R0, true);
  MCCFIInstruction Inst =
      CallerOffset ? C166CFI::createUserStackValue(DwarfR0, CallerOffset)
                   : MCCFIInstruction::createRestore(nullptr, DwarfR0);
  C166CFI::build(MBB, I, DL, TII, Inst, Flag);
}

static void emitNearSystemStackCFI(MachineFunction &MF, MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator I,
                                   const DebugLoc &DL,
                                   const C166InstrInfo &TII) {
  if (!MF.needsFrameMoves() ||
      MF.getFunction().getAddressSpace() != C166::NearAddressSpace)
    return;

  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  const unsigned DwarfSP = MRI->getDwarfRegNum(C166::SP, true);
  const unsigned DwarfCSP = MRI->getDwarfRegNum(C166::CSP, true);
  const unsigned DwarfRA = MRI->getDwarfRegNum(C166::RA, true);

  // The common CIE describes the default Large CALLS/RETS entry.  A near
  // function is entered through CALLA/CALLI/CALLR instead, which pushes only
  // IP.  Override every rule that depends on the system-stack delta at the
  // first address in this FDE.
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::cfiDefCfa(nullptr, DwarfSP, 2));
  C166CFI::build(MBB, I, DL, TII,
                 C166CFI::createNearReturnAddress(DwarfRA, DwarfCSP));
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::createSameValue(nullptr, DwarfCSP));
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::createValOffset(nullptr, DwarfSP, 0));
}

static void emitInterruptSystemStackCFI(MachineFunction &MF,
                                        MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator I,
                                        const DebugLoc &DL,
                                        const C166InstrInfo &TII) {
  if (!MF.needsFrameMoves() || !isInterruptHandler(MF))
    return;

  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  const unsigned DwarfSP = MRI->getDwarfRegNum(C166::SP, true);
  const unsigned DwarfCSP = MRI->getDwarfRegNum(C166::CSP, true);
  const unsigned DwarfRA = MRI->getDwarfRegNum(C166::RA, true);
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::cfiDefCfa(nullptr, DwarfSP, 6),
                 MachineInstr::FrameSetup);
  C166CFI::build(MBB, I, DL, TII,
                 C166CFI::createInterruptReturnAddress(DwarfRA),
                 MachineInstr::FrameSetup);
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::createOffset(nullptr, DwarfCSP, -4),
                 MachineInstr::FrameSetup);
  C166CFI::build(MBB, I, DL, TII,
                 MCCFIInstruction::createValOffset(nullptr, DwarfSP, 0),
                 MachineInstr::FrameSetup);
}

static void adjustUserStack(MachineFunction &MF, MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator I, const DebugLoc &DL,
                            const C166InstrInfo &TII, uint64_t Amount,
                            bool Allocate, uint64_t CallerOffset) {
  MachineInstr::MIFlag Flag =
      Allocate ? MachineInstr::FrameSetup : MachineInstr::FrameDestroy;
  unsigned Opcode;
  if (Allocate)
    Opcode = Amount <= 7 ? C166::SUBri3 : C166::SUBri16;
  else
    Opcode = Amount <= 7 ? C166::ADDri3 : C166::ADDri16;
  BuildMI(MBB, I, DL, TII.get(Opcode), C166::R0)
      .addReg(C166::R0)
      .addImm(Amount)
      .setMIFlag(Flag);
  emitR0CFI(MF, MBB, I, DL, TII, CallerOffset, Flag);
}

void C166FrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  unsigned CSSize =
      MF.getInfo<C166MachineFunctionInfo>()->getCalleeSavedFrameSize();
  uint64_t LocalSize = StackSize - CSSize;
  const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
  MachineBasicBlock::iterator I = MBB.begin();
  const DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  const bool IsInterrupt = isInterruptHandler(MF);
  if (IsInterrupt) {
    emitInterruptSystemStackCFI(MF, MBB, I, DL, TII);
    if (hasNamedRegisterBank(MF)) {
      // Keep the synthetic hardware-frame CFI at the true entry point, then
      // switch CP before any generated callee-save or body instruction.
      I = MBB.begin();
      while (I != MBB.end() && I->isMetaInstruction())
        ++I;

      StringRef Bank = MF.getFunction()
                           .getFnAttribute("c166-register-bank")
                           .getValueAsString();
      const char *BankSymbol = MF.createExternalSymbolName(Bank);
      MBB.addLiveIn(C166::R0);
      MBB.addLiveIn(C166::CP);
      BuildMI(MBB, I, DL, TII.get(C166::MOVabsdg))
          .addExternalSymbol(BankSymbol)
          .addReg(C166::R0)
          .setMIFlag(MachineInstr::FrameSetup);
      MachineInstrBuilder Switch =
          BuildMI(MBB, I, DL, TII.get(C166::SCXTri16), C166::CP)
              .addReg(C166::CP, RegState::Kill)
              .addExternalSymbol(BankSymbol)
              .setMIFlag(MachineInstr::FrameSetup);

      // SCXT CP delays the new register-bank mapping by one instruction.  A
      // callee-save operation accesses only the system stack/SFR space and
      // fills that slot. A banked leaf needs NOP.
      if (MF.getFrameInfo().getCalleeSavedInfo().empty())
        BuildMI(MBB, I, DL, TII.get(C166::NOP))
            .setMIFlag(MachineInstr::FrameSetup);
      if (MF.needsFrameMoves()) {
        auto AfterSwitch = std::next(Switch->getIterator());
        C166CFI::build(MBB, AfterSwitch, DL, TII,
                       MCCFIInstruction::cfiDefCfaOffset(nullptr, 8),
                       MachineInstr::FrameSetup);
      }
    }
    I = MBB.begin();
    while (I != MBB.end() && I->getFlag(MachineInstr::FrameSetup))
      ++I;
  } else {
    emitNearSystemStackCFI(MF, MBB, I, DL, TII);
    while (I != MBB.end() && I->getOpcode() == C166::MOVmrPreDec &&
           I->getFlag(MachineInstr::FrameSetup))
      ++I;
  }

  if (!StackSize)
    return;
  // The user stack is in the 14-bit DPP1 page. A frame
  // may fill that page, but it must never make R0 address another DPP page.
  if (StackSize > 0x4000) {
    MF.getFunction().getContext().emitError(
        "C166 automatic data exceeds the 16K user stack");
    return;
  }
  if (LocalSize)
    adjustUserStack(MF, MBB, I, DL, TII, LocalSize, true, StackSize);
  else
    emitR0CFI(MF, MBB, I, DL, TII, CSSize, MachineInstr::FrameSetup);

  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  Register FrameReg = TRI.getFrameRegister(MF);
  if (hasFP(MF)) {
    BuildMI(MBB, I, DL, TII.get(C166::MOVrr), FrameReg)
        .addReg(C166::R0)
        .setMIFlag(MachineInstr::FrameSetup);
    for (MachineBasicBlock &Block : llvm::drop_begin(MF))
      Block.addLiveIn(FrameReg);

    if (MF.needsFrameMoves()) {
      const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
      unsigned DwarfR0 = MRI->getDwarfRegNum(C166::R0, true);
      unsigned DwarfFrameReg = MRI->getDwarfRegNum(FrameReg, true);
      C166CFI::build(
          MBB, I, DL, TII,
          C166CFI::createUserStackValue(DwarfR0, StackSize, DwarfFrameReg),
          MachineInstr::FrameSetup);
    }
  }

  if (!MF.needsFrameMoves() || IsInterrupt)
    return;

  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  const unsigned DwarfDPP1 = MRI->getDwarfRegNum(C166::DPP1, true);
  const unsigned DwarfFrameReg = MRI->getDwarfRegNum(FrameReg, true);
  ArrayRef<CalleeSavedInfo> CSI = MF.getFrameInfo().getCalleeSavedInfo();
  for (auto [Index, Info] : llvm::enumerate(CSI)) {
    if (Info.isSpilledToReg())
      continue;
    unsigned DwarfReg = MRI->getDwarfRegNum(Info.getReg(), true);
    int64_t Offset = LocalSize + 2 * (CSI.size() - Index - 1);
    C166CFI::build(
        MBB, I, DL, TII,
        C166CFI::createUserStackLocation(DwarfReg, Offset, DwarfDPP1,
                                         DwarfFrameReg),
        MachineInstr::FrameSetup);
  }
}

void C166FrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  unsigned CSSize =
      MF.getInfo<C166MachineFunctionInfo>()->getCalleeSavedFrameSize();
  uint64_t LocalSize = StackSize - CSSize;
  const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
  MachineBasicBlock::iterator I = MBB.getFirstTerminator();
  const DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  const bool IsInterrupt = isInterruptHandler(MF);
  if (StackSize && StackSize <= 0x4000 && IsInterrupt) {
    for (MachineBasicBlock::iterator Candidate = MBB.begin();
         Candidate != MBB.end(); ++Candidate) {
      if (Candidate->getOpcode() == C166::POP &&
          Candidate->getFlag(MachineInstr::FrameDestroy)) {
        I = Candidate;
        break;
      }
    }
  }

  if (StackSize && StackSize <= 0x4000) {
    if (!IsInterrupt && CSSize) {
      MachineBasicBlock::iterator FirstRestore = I;
      while (FirstRestore != MBB.begin()) {
        MachineBasicBlock::iterator Previous = std::prev(FirstRestore);
        if (Previous->getOpcode() != C166::MOVrmPostInc ||
            !Previous->getFlag(MachineInstr::FrameDestroy))
          break;
        FirstRestore = Previous;
      }
      I = FirstRestore;
    }

    if (hasFP(MF))
      BuildMI(MBB, I, DL, TII.get(C166::MOVrr), C166::R0)
          .addReg(MF.getSubtarget().getRegisterInfo()->getFrameRegister(MF))
          .setMIFlag(MachineInstr::FrameDestroy);

    if (LocalSize)
      adjustUserStack(MF, MBB, I, DL, TII, LocalSize, false, CSSize);

    if (!IsInterrupt && CSSize && MF.needsFrameMoves()) {
      const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
      unsigned Remaining = CSSize;
      for (MachineBasicBlock::iterator Restore = I;
           Restore != MBB.getFirstTerminator() && Remaining;) {
        if (Restore->getOpcode() != C166::MOVrmPostInc) {
          ++Restore;
          continue;
        }
        unsigned DwarfReg =
            MRI->getDwarfRegNum(Restore->getOperand(0).getReg(), true);
        MachineBasicBlock::iterator AfterRestore = std::next(Restore);
        C166CFI::build(MBB, AfterRestore, DL, TII,
                       MCCFIInstruction::createRestore(nullptr, DwarfReg),
                       MachineInstr::FrameDestroy);
        Remaining -= 2;
        emitR0CFI(MF, MBB, AfterRestore, DL, TII, Remaining,
                  MachineInstr::FrameDestroy);
        Restore = AfterRestore;
      }
      assert(!Remaining && "C166 callee-save restore was not found");
    }
  }

  if (hasNamedRegisterBank(MF)) {
    I = MBB.getFirstTerminator();
    MachineInstrBuilder Pop = BuildMI(MBB, I, DL, TII.get(C166::POP), C166::CP)
                                  .setMIFlag(MachineInstr::FrameDestroy);
    if (MF.needsFrameMoves()) {
      auto AfterPop = std::next(Pop->getIterator());
      C166CFI::build(MBB, AfterPop, DL, TII,
                     MCCFIInstruction::cfiDefCfaOffset(nullptr, 6),
                     MachineInstr::FrameDestroy);
    }
  }
}

bool C166FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  MachineFunction &MF = *MBB.getParent();
  if (!isInterruptHandler(MF)) {
    if (CSI.empty())
      return false;
    const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
    MF.getInfo<C166MachineFunctionInfo>()->setCalleeSavedFrameSize(CSI.size() *
                                                                   2);
    DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
    for (const CalleeSavedInfo &Info : CSI) {
      MCRegister Reg = Info.getReg();
      MBB.addLiveIn(Reg);
      BuildMI(MBB, MI, DL, TII.get(C166::MOVmrPreDec), C166::R0)
          .addReg(C166::R0)
          .addReg(Reg, RegState::Kill)
          .setMIFlag(MachineInstr::FrameSetup);
    }
    return true;
  }
  const bool HasBank = hasNamedRegisterBank(MF);
  if (CSI.empty())
    return false;

  const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
  int64_t CFAOffset = HasBank ? 8 : 6;
  for (const CalleeSavedInfo &I : CSI) {
    MCRegister Reg = I.getReg();
    MBB.addLiveIn(Reg);
    MachineInstrBuilder Push =
        Reg == C166::MDC ? BuildMI(MBB, MI, DL, TII.get(C166::SCXTri16), Reg)
                               .addReg(Reg, RegState::Kill)
                               .addImm(0x10)
                               .setMIFlag(MachineInstr::FrameSetup)
                         : BuildMI(MBB, MI, DL, TII.get(C166::PUSH))
                               .addReg(Reg, RegState::Kill)
                               .setMIFlag(MachineInstr::FrameSetup);
    CFAOffset += 2;
    if (!MF.needsFrameMoves())
      continue;
    auto AfterPush = std::next(Push->getIterator());
    C166CFI::build(MBB, AfterPush, DL, TII,
                   MCCFIInstruction::cfiDefCfaOffset(nullptr, CFAOffset),
                   MachineInstr::FrameSetup);
    int DwarfReg = MRI->getDwarfRegNum(Reg, true);
    if (DwarfReg >= 0)
      C166CFI::build(
          MBB, AfterPush, DL, TII,
          MCCFIInstruction::createOffset(nullptr, DwarfReg, -CFAOffset),
          MachineInstr::FrameSetup);
  }
  return true;
}

bool C166FrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  MachineFunction &MF = *MBB.getParent();
  if (!isInterruptHandler(MF)) {
    if (CSI.empty())
      return false;
    const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
    DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
    for (const CalleeSavedInfo &Info : llvm::reverse(CSI))
      BuildMI(MBB, MI, DL, TII.get(C166::MOVrmPostInc), Info.getReg())
          .addReg(C166::R0, RegState::Define)
          .addReg(C166::R0)
          .setMIFlag(MachineInstr::FrameDestroy);
    return true;
  }
  const bool HasBank = hasNamedRegisterBank(MF);
  if (CSI.empty())
    return false;

  const C166InstrInfo &TII = *MF.getSubtarget<C166Subtarget>().getInstrInfo();
  const MCRegisterInfo *MRI = MF.getContext().getRegisterInfo();
  DebugLoc DL = MI != MBB.end() ? MI->getDebugLoc() : DebugLoc();
  int64_t CFAOffset = 6 + 2 * CSI.size() + (HasBank ? 2 : 0);
  for (const CalleeSavedInfo &I : llvm::reverse(CSI)) {
    MCRegister Reg = I.getReg();
    MachineInstrBuilder Pop = BuildMI(MBB, MI, DL, TII.get(C166::POP), Reg)
                                  .setMIFlag(MachineInstr::FrameDestroy);
    if (MF.needsFrameMoves()) {
      auto AfterPop = std::next(Pop->getIterator());
      int DwarfReg = MRI->getDwarfRegNum(Reg, true);
      if (DwarfReg >= 0)
        C166CFI::build(MBB, AfterPop, DL, TII,
                       MCCFIInstruction::createRestore(nullptr, DwarfReg),
                       MachineInstr::FrameDestroy);
      CFAOffset -= 2;
      C166CFI::build(MBB, AfterPop, DL, TII,
                     MCCFIInstruction::cfiDefCfaOffset(nullptr, CFAOffset),
                     MachineInstr::FrameDestroy);
    } else {
      CFAOffset -= 2;
    }
  }
  return true;
}

MachineBasicBlock::iterator C166FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  // Caller cleanup and fixed-frame deallocation are separated only by the
  // call-sequence marker.  Merge them while PEI removes that marker.
  if (I->getOpcode() == C166::ADJCALLSTACKUP && I != MBB.begin()) {
    MachineBasicBlock::iterator Cleanup = std::prev(I);
    MachineBasicBlock::iterator Deallocate = std::next(I);
    if (Cleanup->getOpcode() == C166::ADJSP && Cleanup->getOperand(0).isImm() &&
        Deallocate != MBB.end() &&
        (Deallocate->getOpcode() == C166::ADDri3 ||
         Deallocate->getOpcode() == C166::ADDri16) &&
        Deallocate->getFlag(MachineInstr::FrameDestroy) &&
        Deallocate->getOperand(2).isImm()) {
      uint64_t CleanupSize = Cleanup->getOperand(0).getImm();
      uint64_t FrameSize = Deallocate->getOperand(2).getImm();
      if (CleanupSize == static_cast<uint64_t>(I->getOperand(0).getImm()) &&
          isUInt<16>(CleanupSize + FrameSize)) {
        uint64_t CombinedSize = CleanupSize + FrameSize;
        const C166InstrInfo &TII =
            *MF.getSubtarget<C166Subtarget>().getInstrInfo();
        Deallocate->setDesc(
            TII.get(CombinedSize <= 7 ? C166::ADDri3 : C166::ADDri16));
        Deallocate->getOperand(2).setImm(CombinedSize);
        Cleanup->eraseFromParent();
      }
    }
  }
  return MBB.erase(I);
}
