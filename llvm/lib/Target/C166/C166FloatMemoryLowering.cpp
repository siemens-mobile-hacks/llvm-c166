//===-- C166FloatMemoryLowering.cpp - C166 float storage lowering --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The C166 ABI stores IEEE binary32 and binary64 values most-significant
// word first, while integers use the ordinary C166 low-word-first
// representation.  LLVM's DataLayout cannot express type-dependent word
// order, so encode the physical floating representation immediately before
// instruction selection.
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {

constexpr StringLiteral C166FloatStorageMetadata = "c166.float.storage";

static APInt reverseWords(APInt Bits) {
  assert(Bits.getBitWidth() % 16 == 0 && "expected whole C166 words");
  APInt Result(Bits.getBitWidth(), 0);
  unsigned Words = Bits.getBitWidth() / 16;
  for (unsigned I = 0; I != Words; ++I) {
    APInt Word = Bits.lshr(I * 16).trunc(16).zext(Bits.getBitWidth());
    Result |= Word.shl((Words - 1 - I) * 16);
  }
  return Result;
}

static bool containsFloatStorage(Type *Ty) {
  if (Ty->isFloatTy() || Ty->isDoubleTy())
    return true;
  if (auto *ArrayTy = dyn_cast<ArrayType>(Ty))
    return containsFloatStorage(ArrayTy->getElementType());
  if (auto *StructTy = dyn_cast<StructType>(Ty)) {
    if (StructTy->isOpaque())
      return false;
    for (Type *ElementTy : StructTy->elements())
      if (containsFloatStorage(ElementTy))
        return true;
    return false;
  }
  if (auto *VectorTy = dyn_cast<VectorType>(Ty))
    return containsFloatStorage(VectorTy->getElementType());
  return false;
}

static Constant *encodeFloatInitializer(Constant *C) {
  Type *Ty = C->getType();
  if (Ty->isFloatTy() || Ty->isDoubleTy()) {
    if (isa<UndefValue, PoisonValue>(C))
      return C;
    auto *FP = dyn_cast<ConstantFP>(C);
    if (!FP) {
      // A symbolic ConstantExpr can have floating type in hand-written IR,
      // but splitting its relocation into the ABI's reversed 16-bit word
      // order is not representable by the C166 ELF relocation set.  Ordinary
      // C floating initializers are ConstantFPs.  Reject the unsupported IR
      // form as a normal target diagnostic instead of terminating llc.
      C->getContext().emitError(
          "C166 cannot encode a symbolic floating-point global initializer");
      return C;
    }
    APInt Bits = reverseWords(FP->getValueAPF().bitcastToAPInt());
    const fltSemantics &Semantics =
        Ty->isFloatTy() ? APFloat::IEEEsingle() : APFloat::IEEEdouble();
    return ConstantFP::get(Ty, APFloat(Semantics, std::move(Bits)));
  }

  if (isa<ConstantAggregateZero>(C))
    return C;

  SmallVector<Constant *, 8> Elements;
  if (auto *Data = dyn_cast<ConstantDataSequential>(C)) {
    for (unsigned I = 0; I != Data->getNumElements(); ++I)
      Elements.push_back(encodeFloatInitializer(Data->getElementAsConstant(I)));
  } else if (isa<ConstantArray, ConstantStruct, ConstantVector>(C)) {
    for (Value *Operand : C->operands())
      Elements.push_back(encodeFloatInitializer(cast<Constant>(Operand)));
  } else {
    return C;
  }

  if (auto *ArrayTy = dyn_cast<ArrayType>(Ty))
    return ConstantArray::get(ArrayTy, Elements);
  if (auto *StructTy = dyn_cast<StructType>(Ty))
    return ConstantStruct::get(StructTy, Elements);
  if (Ty->isVectorTy())
    return ConstantVector::get(Elements);
  llvm_unreachable("unexpected C166 aggregate initializer type");
}

static Value *reverseFloatWords(IRBuilder<> &Builder, Value *Bits,
                                const Twine &Name) {
  auto *Ty = cast<IntegerType>(Bits->getType());
  unsigned Width = Ty->getBitWidth();
  assert((Width == 32 || Width == 64) && "expected binary32 or binary64");

  Value *Result = ConstantInt::get(Ty, 0);
  unsigned Words = Width / 16;
  for (unsigned I = 0; I != Words; ++I) {
    Value *Word = Bits;
    if (I)
      Word = Builder.CreateLShr(Word, I * 16, Name + ".word");
    Word = Builder.CreateAnd(Word, 0xffffu, Name + ".masked");
    unsigned DestinationShift = (Words - 1 - I) * 16;
    if (DestinationShift)
      Word = Builder.CreateShl(Word, DestinationShift, Name + ".shifted");
    Result = Builder.CreateOr(Result, Word, Name + ".partial");
  }
  return Result;
}

class C166FloatMemoryLowering : public ModulePass {
public:
  static char ID;

  C166FloatMemoryLowering() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;
  StringRef getPassName() const override {
    return "C166 float memory lowering";
  }
};

} // namespace

char C166FloatMemoryLowering::ID = 0;

INITIALIZE_PASS(C166FloatMemoryLowering, "c166-float-memory-lowering",
                "C166 float memory lowering", false, false)

Pass *llvm::createC166FloatMemoryLoweringPass() {
  return new C166FloatMemoryLowering();
}

bool C166FloatMemoryLowering::runOnModule(Module &M) {
  return lowerC166FloatMemory(M);
}

bool llvm::lowerC166FloatMemory(Module &M) {
  bool Changed = false;
  for (GlobalVariable &Global : M.globals()) {
    if (!Global.hasInitializer() ||
        !containsFloatStorage(Global.getValueType()) ||
        Global.getMetadata(C166FloatStorageMetadata))
      continue;
    Constant *Initializer = Global.getInitializer();
    Constant *Encoded = encodeFloatInitializer(Initializer);
    if (Encoded != Initializer)
      Global.setInitializer(Encoded);
    // The early new-PM hook and the late CodeGen safety pass share this
    // implementation.  Mark each physical initializer so an LTO or legacy
    // pipeline cannot reverse its words a second time.
    Global.setMetadata(C166FloatStorageMetadata,
                       MDNode::get(M.getContext(), {}));
    Changed = true;
  }

  SmallVector<LoadInst *, 16> Loads;
  SmallVector<StoreInst *, 16> Stores;
  SmallVector<VAArgInst *, 4> VAArgs;
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;
    for (Instruction &I : instructions(F)) {
      if (auto *VAArg = dyn_cast<VAArgInst>(&I);
          VAArg &&
          (VAArg->getType()->isFloatTy() || VAArg->getType()->isDoubleTy()))
        VAArgs.push_back(VAArg);
      else if (auto *Load = dyn_cast<LoadInst>(&I);
               Load &&
               (Load->getType()->isFloatTy() || Load->getType()->isDoubleTy()))
        Loads.push_back(Load);
      else if (auto *Store = dyn_cast<StoreInst>(&I);
               Store && (Store->getValueOperand()->getType()->isFloatTy() ||
                         Store->getValueOperand()->getType()->isDoubleTy()))
        Stores.push_back(Store);
    }
  }

  for (VAArgInst *VAArg : VAArgs) {
    IRBuilder<> Builder(VAArg);
    Type *FloatTy = VAArg->getType();
    unsigned Width = FloatTy->isFloatTy() ? 32 : 64;
    IntegerType *IntTy = Builder.getIntNTy(Width);
    PointerType *DataPtrTy = Builder.getPtrTy();

    // Type legalization softens a floating VAARG to integer VAARG nodes before
    // target DAG lowering, losing the target's different word
    // order for floating objects.  Expand it while the IR type is still known:
    // update the far va_list pointer, load the physical object as an integer,
    // reverse its 16-bit words, and restore the logical floating value.
    LoadInst *Current = Builder.CreateLoad(
        DataPtrTy, VAArg->getPointerOperand(), VAArg->getName() + ".address");
    Current->setAlignment(Align(2));
    Value *Next = Builder.CreateGEP(Builder.getInt8Ty(), Current,
                                    Builder.getInt32(Width / 8),
                                    VAArg->getName() + ".next");
    StoreInst *Advance = Builder.CreateStore(Next, VAArg->getPointerOperand());
    Advance->setAlignment(Align(2));
    LoadInst *Physical =
        Builder.CreateLoad(IntTy, Current, VAArg->getName() + ".physical");
    Physical->setAlignment(Align(2));
    Value *Logical =
        reverseFloatWords(Builder, Physical, VAArg->getName() + ".logical");
    Value *Value =
        Builder.CreateBitCast(Logical, FloatTy, VAArg->getName() + ".value");
    Current->setDebugLoc(VAArg->getDebugLoc());
    Advance->setDebugLoc(VAArg->getDebugLoc());
    Physical->setDebugLoc(VAArg->getDebugLoc());
    if (auto *LogicalI = dyn_cast<Instruction>(Logical))
      LogicalI->setDebugLoc(VAArg->getDebugLoc());
    if (auto *ValueI = dyn_cast<Instruction>(Value))
      ValueI->setDebugLoc(VAArg->getDebugLoc());
    VAArg->replaceAllUsesWith(Value);
    VAArg->eraseFromParent();
    Changed = true;
  }

  for (LoadInst *Load : Loads) {
    IRBuilder<> Builder(Load);
    Type *FloatTy = Load->getType();
    IntegerType *IntTy = Builder.getIntNTy(FloatTy->isFloatTy() ? 32 : 64);
    LoadInst *Physical = Builder.CreateLoad(IntTy, Load->getPointerOperand(),
                                            Load->getName() + ".physical");
    Physical->setVolatile(Load->isVolatile());
    Physical->setAlignment(Load->getAlign());
    if (Load->isAtomic())
      Physical->setAtomic(Load->getOrdering(), Load->getSyncScopeID());
    Physical->copyMetadata(*Load);
    Physical->setDebugLoc(Load->getDebugLoc());
    Value *Logical =
        reverseFloatWords(Builder, Physical, Load->getName() + ".logical");
    Value *Value =
        Builder.CreateBitCast(Logical, FloatTy, Load->getName() + ".value");
    Load->replaceAllUsesWith(Value);
    Load->eraseFromParent();
    Changed = true;
  }

  for (StoreInst *Store : Stores) {
    IRBuilder<> Builder(Store);
    Type *FloatTy = Store->getValueOperand()->getType();
    IntegerType *IntTy = Builder.getIntNTy(FloatTy->isFloatTy() ? 32 : 64);
    Value *Logical =
        Builder.CreateBitCast(Store->getValueOperand(), IntTy, "fp.logical");
    Value *Physical = reverseFloatWords(Builder, Logical, "fp.physical");
    StoreInst *Encoded =
        Builder.CreateStore(Physical, Store->getPointerOperand());
    Encoded->setVolatile(Store->isVolatile());
    Encoded->setAlignment(Store->getAlign());
    if (Store->isAtomic())
      Encoded->setAtomic(Store->getOrdering(), Store->getSyncScopeID());
    Encoded->copyMetadata(*Store);
    Encoded->setDebugLoc(Store->getDebugLoc());
    Store->eraseFromParent();
    Changed = true;
  }

  return Changed;
}
