//===-- C166SFRBitfieldLowering.cpp - Lower volatile SFR bit fields -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicsC166.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/TargetParser/C166TargetParser.h"
#include "llvm/Transforms/Utils/Local.h"
#include <optional>

using namespace llvm;
using namespace llvm::PatternMatch;

namespace {

struct BitUpdate {
  LoadInst *Load = nullptr;
  Value *BitValue = nullptr;
  unsigned Bit = 0;
};

struct DeclaredBit {
  uint16_t Address;
  unsigned Bit;
  bool IsESFR;
};

static std::optional<DeclaredBit> getDeclaredBit(Value *Pointer) {
  auto *Global = dyn_cast<GlobalVariable>(Pointer->stripPointerCasts());
  MDNode *MD = Global ? Global->getMetadata(C166::SFRBitMetadataName) : nullptr;
  if (!MD || MD->getNumOperands() != 3)
    return std::nullopt;

  auto *Address = mdconst::dyn_extract<ConstantInt>(MD->getOperand(0));
  auto *Bit = mdconst::dyn_extract<ConstantInt>(MD->getOperand(1));
  auto *IsESFR = mdconst::dyn_extract<ConstantInt>(MD->getOperand(2));
  if (!Address || !Bit || !IsESFR || !Address->getValue().isIntN(16) ||
      Bit->getZExtValue() >= 16)
    return std::nullopt;
  return DeclaredBit{static_cast<uint16_t>(Address->getZExtValue()),
                     static_cast<unsigned>(Bit->getZExtValue()),
                     IsESFR->isOne()};
}

static bool lowerDeclaredBitAccesses(Function &F) {
  SmallVector<Instruction *, 4> Accesses;
  for (Instruction &I : instructions(F)) {
    Value *Pointer = nullptr;
    if (auto *Load = dyn_cast<LoadInst>(&I))
      Pointer = Load->getPointerOperand();
    else if (auto *Store = dyn_cast<StoreInst>(&I))
      Pointer = Store->getPointerOperand();
    if (Pointer && getDeclaredBit(Pointer))
      Accesses.push_back(&I);
  }

  for (Instruction *Access : Accesses) {
    Value *Pointer = isa<LoadInst>(Access)
                         ? cast<LoadInst>(Access)->getPointerOperand()
                         : cast<StoreInst>(Access)->getPointerOperand();
    DeclaredBit Bit = *getDeclaredBit(Pointer);
    IRBuilder<> Builder(Access);
    Value *Address = Builder.getInt16(Bit.Address);
    Value *Index = Builder.getInt16(Bit.Bit);
    Value *Extended = Builder.getInt1(Bit.IsESFR);

    if (auto *Load = dyn_cast<LoadInst>(Access)) {
      Function *Read = Intrinsic::getOrInsertDeclaration(
          F.getParent(), Intrinsic::c166_sfr_bit_read);
      CallInst *Call = Builder.CreateCall(Read, {Address, Index, Extended});
      Call->setDebugLoc(Load->getDebugLoc());
      Load->replaceAllUsesWith(Call);
    } else {
      auto *Store = cast<StoreInst>(Access);
      Value *Nonzero = Builder.CreateICmpNE(Store->getValueOperand(),
                                            Builder.getInt16(0));
      Value *BitValue = Builder.CreateZExt(Nonzero, Builder.getInt16Ty());
      Function *Write = Intrinsic::getOrInsertDeclaration(
          F.getParent(), Intrinsic::c166_sfr_bit_write);
      CallInst *Call = Builder.CreateCall(
          Write, {Address, Index, BitValue, Extended});
      Call->setDebugLoc(Store->getDebugLoc());
    }
    Access->eraseFromParent();
  }
  return !Accesses.empty();
}

static bool matchAndMask(Value *V, Value *&Other, uint16_t &Mask) {
  auto *And = dyn_cast<BinaryOperator>(V);
  if (!And || And->getOpcode() != Instruction::And)
    return false;
  auto *C = dyn_cast<ConstantInt>(And->getOperand(1));
  if (!C)
    C = dyn_cast<ConstantInt>(And->getOperand(0));
  if (!C)
    return false;
  Other = C == And->getOperand(0) ? And->getOperand(1) : And->getOperand(0);
  Mask = static_cast<uint16_t>(C->getZExtValue());
  return true;
}

static LoadInst *matchClearedBit(Value *V, unsigned &Bit) {
  Value *Other;
  uint16_t Mask;
  if (!matchAndMask(V, Other, Mask))
    return nullptr;
  uint16_t Cleared = static_cast<uint16_t>(~Mask);
  if (!isPowerOf2_32(Cleared))
    return nullptr;
  auto *Load = dyn_cast<LoadInst>(Other);
  if (!Load || !Load->isVolatile() || !Load->hasOneUse())
    return nullptr;
  Bit = countr_zero(Cleared);
  return Load;
}

static Value *stripOrZero(Value *V) {
  auto *Or = dyn_cast<BinaryOperator>(V);
  if (!Or || Or->getOpcode() != Instruction::Or)
    return V;
  if (match(Or->getOperand(0), m_Zero()))
    return Or->getOperand(1);
  if (match(Or->getOperand(1), m_Zero()))
    return Or->getOperand(0);
  return V;
}

static Value *matchInsertedBit(Value *V, unsigned Bit) {
  Value *Source = V;
  if (auto *And = dyn_cast<BinaryOperator>(Source);
      And && And->getOpcode() == Instruction::And) {
    Value *Other;
    uint16_t Mask;
    if (!matchAndMask(And, Other, Mask) || Mask != uint16_t(1U << Bit))
      return nullptr;
    Source = Other;
  }

  if (Bit != 0) {
    auto *Shift = dyn_cast<BinaryOperator>(Source);
    auto *Amount = Shift && Shift->getOpcode() == Instruction::Shl
                       ? dyn_cast<ConstantInt>(Shift->getOperand(1))
                       : nullptr;
    if (!Amount || Amount->getZExtValue() != Bit)
      return nullptr;
    Source = Shift->getOperand(0);
  }

  Value *Other;
  uint16_t Mask;
  if (matchAndMask(Source, Other, Mask)) {
    if (Mask != 1)
      return nullptr;
    Source = Other;
  }
  return Source->getType()->isIntegerTy() ? Source : nullptr;
}

static std::optional<BitUpdate> matchBitUpdate(StoreInst &Store) {
  if (!Store.getValueOperand()->getType()->isIntegerTy(16))
    return std::nullopt;

  Value *Stored = stripOrZero(Store.getValueOperand());

  unsigned Bit;
  if (LoadInst *Load = matchClearedBit(Stored, Bit))
    return BitUpdate{
        Load, ConstantInt::get(Store.getValueOperand()->getType(), 0), Bit};

  auto *Or = dyn_cast<BinaryOperator>(Stored);
  if (!Or || Or->getOpcode() != Instruction::Or)
    return std::nullopt;

  for (unsigned ClearedOperand = 0; ClearedOperand != 2; ++ClearedOperand) {
    unsigned CandidateBit;
    LoadInst *Load =
        matchClearedBit(Or->getOperand(ClearedOperand), CandidateBit);
    if (!Load)
      continue;
    Value *Inserted =
        matchInsertedBit(Or->getOperand(1 - ClearedOperand), CandidateBit);
    if (Inserted)
      return BitUpdate{Load, Inserted, CandidateBit};
  }

  for (unsigned ConstantOperand = 0; ConstantOperand != 2; ++ConstantOperand) {
    auto *C = dyn_cast<ConstantInt>(Or->getOperand(ConstantOperand));
    if (!C)
      continue;
    uint16_t Mask = static_cast<uint16_t>(C->getZExtValue());
    if (!isPowerOf2_32(Mask))
      continue;
    unsigned CandidateBit = countr_zero(Mask);
    Value *Other = Or->getOperand(1 - ConstantOperand);
    LoadInst *Load = dyn_cast<LoadInst>(Other);
    if (!Load) {
      unsigned ClearedBit;
      Load = matchClearedBit(Other, ClearedBit);
      if (!Load || ClearedBit != CandidateBit)
        continue;
    }
    if (!Load->isVolatile() || !Load->hasOneUse())
      continue;
    return BitUpdate{Load,
                     ConstantInt::get(Store.getValueOperand()->getType(), 1),
                     CandidateBit};
  }
  return std::nullopt;
}

static std::optional<uint16_t> getConstantAddress(Value *Pointer) {
  auto *Cast = dyn_cast<ConstantExpr>(Pointer);
  if (!Cast || Cast->getOpcode() != Instruction::IntToPtr)
    return std::nullopt;
  auto *Address = dyn_cast<ConstantInt>(Cast->getOperand(0));
  if (!Address || !Address->getValue().isIntN(16))
    return std::nullopt;
  return static_cast<uint16_t>(Address->getZExtValue());
}

static bool getBitAddress(Value *Pointer, unsigned Bit, uint16_t &Address,
                          bool &IsESFR) {
  unsigned AddressSpace = Pointer->getType()->getPointerAddressSpace();
  if (AddressSpace != C166::SFRAddressSpace &&
      AddressSpace != C166::ESFRAddressSpace)
    return false;
  std::optional<uint16_t> Physical = getConstantAddress(Pointer);
  if (!Physical || Bit >= 16)
    return false;

  IsESFR = AddressSpace == C166::ESFRAddressSpace;
  uint16_t Base = IsESFR ? 0xf000 : 0xfe00;
  if (*Physical < Base || *Physical > Base + 2 * 0xef ||
      ((*Physical - Base) & 1))
    return false;
  unsigned Word = (*Physical - Base) / 2;
  if (Word < 0x80)
    return false;
  Address = *Physical;
  return true;
}

class C166SFRBitfieldLowering : public FunctionPass {
public:
  static char ID;

  C166SFRBitfieldLowering() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override { return lowerC166SFRBitfields(F); }

  StringRef getPassName() const override {
    return "C166 SFR bit-field lowering";
  }
};

} // namespace

char C166SFRBitfieldLowering::ID = 0;

INITIALIZE_PASS(C166SFRBitfieldLowering, "c166-sfr-bitfield-lowering",
                "C166 SFR bit-field lowering", false, false)

Pass *llvm::createC166SFRBitfieldLoweringPass() {
  return new C166SFRBitfieldLowering();
}

bool llvm::lowerC166SFRBitfields(Function &F) {
  bool Changed = lowerDeclaredBitAccesses(F);
  SmallVector<StoreInst *, 4> Stores;
  for (Instruction &I : instructions(F))
    if (auto *Store = dyn_cast<StoreInst>(&I); Store && Store->isVolatile())
      Stores.push_back(Store);

  for (StoreInst *Store : Stores) {
    if (!Store->getMetadata(C166::SFRBitfieldMetadataName))
      continue;

    std::optional<BitUpdate> Update = matchBitUpdate(*Store);
    if (!Update ||
        Update->Load->getPointerOperand() != Store->getPointerOperand())
      continue;

    uint16_t Address;
    bool IsESFR;
    if (!getBitAddress(Store->getPointerOperand(), Update->Bit, Address,
                       IsESFR))
      continue;

    IRBuilder<> Builder(Store);
    Value *BitValue =
        Builder.CreateZExtOrTrunc(Update->BitValue, Builder.getInt16Ty());
    Function *Write = Intrinsic::getOrInsertDeclaration(
        F.getParent(), Intrinsic::c166_sfr_bit_write);
    CallInst *Call = Builder.CreateCall(
        Write, {Builder.getInt16(Address), Builder.getInt16(Update->Bit),
                BitValue, Builder.getInt1(IsESFR)});
    Call->setDebugLoc(Store->getDebugLoc());

    Value *Stored = Store->getValueOperand();
    LoadInst *Load = Update->Load;
    Store->eraseFromParent();
    RecursivelyDeleteTriviallyDeadInstructions(Stored);
    if (Load->use_empty())
      Load->eraseFromParent();
    Changed = true;
  }
  return Changed;
}
