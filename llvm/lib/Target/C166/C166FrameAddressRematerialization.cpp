//===-- C166FrameAddressRematerialization.cpp - Shorten frame addresses ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166InstrInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "c166-frame-address-rematerialization"

namespace {

// A derived frame address that is unused inside a loop should not remain live
// through that loop. Making it an independent LEA lets register allocation
// rematerialize it at the later use instead of spilling it. Keep addresses
// used by a loop tied to their shared base; separating those increases
// pressure in indexed loops.
static bool isLiveAcrossLoop(Register Base, Register Derived,
                             const MachineInstr &Definition,
                             MachineRegisterInfo &MRI, MachineLoopInfo &MLI,
                             MachineDominatorTree &MDT) {
  bool HasDerivedUse = false;
  for (const MachineInstr &Use : MRI.use_nodbg_instructions(Derived)) {
    if (MLI.getLoopFor(Use.getParent()))
      return false;
    HasDerivedUse = true;
  }
  if (!HasDerivedUse)
    return false;

  for (const MachineInstr &BaseUse : MRI.use_nodbg_instructions(Base)) {
    const MachineLoop *Loop = MLI.getLoopFor(BaseUse.getParent());
    if (!Loop || Loop->contains(Definition.getParent()))
      continue;

    bool UsedAfterLoop = false;
    for (const MachineInstr &DerivedUse : MRI.use_nodbg_instructions(Derived)) {
      if (Loop->contains(DerivedUse.getParent()) ||
          !MDT.dominates(Loop->getHeader(), DerivedUse.getParent())) {
        UsedAfterLoop = false;
        break;
      }
      UsedAfterLoop = true;
    }
    if (UsedAfterLoop)
      return true;
  }
  return false;
}

static bool foldFrameAddressOffsets(MachineFunction &MF,
                                    const C166InstrInfo &TII,
                                    MachineLoopInfo &MLI,
                                    MachineDominatorTree &MDT) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &MI = *I++;
      if (MI.getOpcode() != C166::ADDri3 && MI.getOpcode() != C166::ADDri16)
        continue;

      Register Destination = MI.getOperand(0).getReg();
      Register Base = MI.getOperand(1).getReg();
      int64_t Addend = MI.getOperand(2).getImm();
      MachineInstr *BaseDef = Base.isVirtual() ? MRI.getVRegDef(Base) : nullptr;
      const MachineOperand *PSWDef = MI.findRegisterDefOperand(C166::PSW, TRI);
      const MachineOperand *CarryDef = MI.findRegisterDefOperand(C166::C, TRI);
      if (!Destination.isVirtual() || !BaseDef ||
          BaseDef->getOpcode() != C166::LEAfi || !PSWDef || !PSWDef->isDead() ||
          !CarryDef || !CarryDef->isDead() ||
          !isLiveAcrossLoop(Base, Destination, MI, MRI, MLI, MDT))
        continue;

      int64_t Offset = BaseDef->getOperand(2).getImm() + Addend;
      if (!isUInt<16>(Offset))
        continue;

      MachineInstrBuilder Address =
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(C166::LEAfi), Destination)
              .add(BaseDef->getOperand(1))
              .addImm(Offset)
              .setMIFlags(MI.getFlags());
      Address->findRegisterDefOperand(C166::PSW, TRI)->setIsDead(true);
      MI.eraseFromParent();
      Changed = true;
    }
  }
  return Changed;
}

static bool isSupportedUse(const MachineInstr &MI) {
  return MI.isCopy() || MI.getOpcode() == C166::PUSHARG ||
         MI.getOpcode() == C166::ADDrr;
}

static bool getExtractedWord(const MachineInstr &MI, Register Address,
                             Register &Word, unsigned &SubReg) {
  if (!MI.isCopy() || !MI.getOperand(0).getReg().isVirtual() ||
      MI.getOperand(1).getReg() != Address)
    return false;
  SubReg = MI.getOperand(1).getSubReg();
  if (SubReg != sub_lo16 && SubReg != sub_hi16)
    return false;
  Word = MI.getOperand(0).getReg();
  return true;
}

static bool rematerializeFrameAddresses(MachineFunction &MF,
                                        const C166InstrInfo &TII,
                                        MachineLoopInfo &MLI,
                                        MachineDominatorTree &MDT) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  SmallVector<MachineInstr *, 8> Addresses;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.getOpcode() == C166::LEAfi || MI.getOpcode() == C166::FRAMEADDR32)
        Addresses.push_back(&MI);

  bool Changed = foldFrameAddressOffsets(MF, TII, MLI, MDT);
  for (MachineInstr *Address : Addresses) {
    Register Original = Address->getOperand(0).getReg();
    if (!Original.isVirtual())
      continue;

    MachineBasicBlock *MBB = Address->getParent();
    DenseMap<Register, unsigned> ExtractedWords;
    SmallVector<MachineOperand *, 4> PhiUses;
    bool Supported = true;
    for (MachineInstr &Use : MRI.use_nodbg_instructions(Original)) {
      Register Word;
      unsigned SubReg;
      if (getExtractedWord(Use, Original, Word, SubReg)) {
        ExtractedWords.insert({Word, SubReg});
        continue;
      }
      if (Use.isPHI()) {
        for (unsigned I = 1; I + 1 < Use.getNumOperands(); I += 2) {
          MachineOperand &Value = Use.getOperand(I);
          if (!Value.isReg() || Value.getReg() != Original)
            continue;
          if (Value.getSubReg() || !Use.getOperand(I + 1).isMBB() ||
              Use.getOperand(I + 1).getMBB() != MBB) {
            Supported = false;
            break;
          }
          PhiUses.push_back(&Value);
        }
        if (!Supported)
          break;
        continue;
      }
      if (Use.getParent() != MBB || !isSupportedUse(Use)) {
        Supported = false;
        break;
      }
    }
    for (const auto &[Word, SubReg] : ExtractedWords) {
      (void)SubReg;
      if (!llvm::all_of(MRI.use_nodbg_instructions(Word),
                        [&](const MachineInstr &Use) {
                          return Use.getParent() == MBB && !Use.isPHI() &&
                                 isSupportedUse(Use);
                        })) {
        Supported = false;
        break;
      }
    }
    if (!Supported)
      continue;

    unsigned MaterializationSites = 0;
    bool OriginalUsed = false;
    bool CurrentIsOriginal = true;
    bool CallSeen = false;
    for (MachineBasicBlock::iterator I = std::next(Address->getIterator());
         I != MBB->end(); ++I) {
      if (I->isCall()) {
        CallSeen = true;
        CurrentIsOriginal = true;
        continue;
      }
      bool UsesAddress = llvm::any_of(I->operands(), [&](MachineOperand &MO) {
        return MO.isReg() && MO.isUse() &&
               (MO.getReg() == Original ||
                ExtractedWords.contains(MO.getReg()));
      });
      if (!UsesAddress)
        continue;
      if (CallSeen &&
          MBB->computeRegisterLiveness(TRI, C166::PSW,
                                       MachineBasicBlock::const_iterator(I)) ==
              MachineBasicBlock::LQR_Dead) {
        ++MaterializationSites;
        CallSeen = false;
        CurrentIsOriginal = false;
      } else if (CurrentIsOriginal) {
        OriginalUsed = true;
      }
    }
    MaterializationSites += OriginalUsed;

    int FrameIndex = Address->getOperand(1).getIndex();
    int64_t MinimumOffset = Address->getOperand(2).getImm();
    if (FrameIndex < 0)
      MinimumOffset += MF.getFrameInfo().getObjectOffset(FrameIndex);

    // Three six-byte address materializations cost more than one
    // materialization, a word save/restore pair, and three copies.
    if (Address->getOpcode() == C166::LEAfi && PhiUses.empty() &&
        MinimumOffset > 15 && MaterializationSites >= 3)
      continue;

    Register CurrentAddress = Original;
    DenseMap<unsigned, Register> CurrentWords;
    bool SawCall = false;
    for (MachineBasicBlock::iterator I = std::next(Address->getIterator());
         I != MBB->end(); ++I) {
      MachineInstr &MI = *I;
      if (MI.isCall()) {
        SawCall = true;
        CurrentAddress = Original;
        CurrentWords.clear();
        continue;
      }

      bool UsesAddress = false;
      bool UsesExtractedWord = false;
      for (MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isUse())
          continue;
        if (MO.getReg() == Original)
          UsesAddress = true;
        auto Word = ExtractedWords.find(MO.getReg());
        if (Word != ExtractedWords.end())
          UsesExtractedWord = true;
      }
      if (!UsesAddress && !UsesExtractedWord)
        continue;

      if (SawCall &&
          MBB->computeRegisterLiveness(TRI, C166::PSW,
                                       MachineBasicBlock::const_iterator(I)) ==
              MachineBasicBlock::LQR_Dead) {
        CurrentAddress = MRI.createVirtualRegister(MRI.getRegClass(Original));
        BuildMI(*MBB, I, MI.getDebugLoc(), TII.get(Address->getOpcode()),
                CurrentAddress)
            .add(Address->getOperand(1))
            .add(Address->getOperand(2));
        CurrentWords.clear();
        SawCall = false;
        Changed = true;
      }

      if (CurrentAddress == Original)
        continue;

      for (MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isUse())
          continue;
        if (MO.getReg() == Original) {
          MO.setReg(CurrentAddress);
          continue;
        }
        auto Word = ExtractedWords.find(MO.getReg());
        if (Word == ExtractedWords.end())
          continue;
        Register &CurrentWord = CurrentWords[Word->second];
        if (!CurrentWord) {
          CurrentWord = MRI.createVirtualRegister(MRI.getRegClass(Word->first));
          BuildMI(*MBB, I, MI.getDebugLoc(), TII.get(TargetOpcode::COPY),
                  CurrentWord)
              .addReg(CurrentAddress, RegState{}, Word->second);
        }
        MO.setReg(CurrentWord);
      }
    }

    if (!PhiUses.empty()) {
      if (SawCall) {
        MachineBasicBlock::iterator InsertAt = MBB->getFirstTerminator();
        if (MBB->computeRegisterLiveness(
                TRI, C166::PSW, MachineBasicBlock::const_iterator(InsertAt)) ==
            MachineBasicBlock::LQR_Dead) {
          CurrentAddress = MRI.createVirtualRegister(MRI.getRegClass(Original));
          BuildMI(*MBB, InsertAt, Address->getDebugLoc(),
                  TII.get(Address->getOpcode()), CurrentAddress)
              .add(Address->getOperand(1))
              .add(Address->getOperand(2));
          Changed = true;
        } else {
          CurrentAddress = Original;
        }
      }
      if (CurrentAddress != Original)
        for (MachineOperand *Use : PhiUses)
          Use->setReg(CurrentAddress);
    }
  }
  return Changed;
}

class C166FrameAddressRematerialization : public MachineFunctionPass {
public:
  static char ID;

  C166FrameAddressRematerialization() : MachineFunctionPass(ID) {
    initializeC166FrameAddressRematerializationPass(
        *PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "C166 frame address rematerialization";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const auto &TII =
        *static_cast<const C166InstrInfo *>(MF.getSubtarget().getInstrInfo());
    MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
    MachineDominatorTree &MDT =
        getAnalysis<MachineDominatorTreeWrapperPass>().getDomTree();
    return rematerializeFrameAddresses(MF, TII, MLI, MDT);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.addRequired<MachineDominatorTreeWrapperPass>();
    AU.addPreserved<MachineLoopInfoWrapperPass>();
    AU.addPreserved<MachineDominatorTreeWrapperPass>();
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char C166FrameAddressRematerialization::ID = 0;

INITIALIZE_PASS_BEGIN(C166FrameAddressRematerialization, DEBUG_TYPE,
                      "C166 frame address rematerialization", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTreeWrapperPass)
INITIALIZE_PASS_END(C166FrameAddressRematerialization, DEBUG_TYPE,
                    "C166 frame address rematerialization", false, false)

FunctionPass *llvm::createC166FrameAddressRematerializationPass() {
  return new C166FrameAddressRematerialization();
}
