//===-- C166AtomicLowering.cpp - C166 atomic lowering -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Lower C166 atomic IR to the target runtime before the generic
// AtomicExpandPass.  The generic pass deliberately uses intptr_t for the
// unsized helper's size argument and i32 for its memory-order arguments.
// C166 uses 16-bit size_t and int, while pointer width depends on the memory
// model, so those generic assumptions do not describe this ABI.
//
// The public load/store/exchange/compare-exchange entry points use the GCC
// generic atomic ABI, expressed with the actual C166 C types. Atomic RMW
// and fences use private C166 helpers because the generic ABI has no unsized
// RMW entry points.
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

#include <cstdint>
#include <optional>

using namespace llvm;

namespace {

enum C166AtomicRMWOp : unsigned {
  C166AtomicAdd,
  C166AtomicSub,
  C166AtomicAnd,
  C166AtomicOr,
  C166AtomicXor,
  C166AtomicNand,
  C166AtomicMax,
  C166AtomicMin,
  C166AtomicUMax,
  C166AtomicUMin,
};

static std::optional<C166AtomicRMWOp>
getAtomicRMWOpcode(AtomicRMWInst::BinOp Op) {
  switch (Op) {
  case AtomicRMWInst::Add:
    return C166AtomicAdd;
  case AtomicRMWInst::Sub:
    return C166AtomicSub;
  case AtomicRMWInst::And:
    return C166AtomicAnd;
  case AtomicRMWInst::Or:
    return C166AtomicOr;
  case AtomicRMWInst::Xor:
    return C166AtomicXor;
  case AtomicRMWInst::Nand:
    return C166AtomicNand;
  case AtomicRMWInst::Max:
    return C166AtomicMax;
  case AtomicRMWInst::Min:
    return C166AtomicMin;
  case AtomicRMWInst::UMax:
    return C166AtomicUMax;
  case AtomicRMWInst::UMin:
    return C166AtomicUMin;
  default:
    return std::nullopt;
  }
}

static unsigned getCAtomicOrder(AtomicOrdering Ordering) {
  switch (Ordering) {
  case AtomicOrdering::NotAtomic:
  case AtomicOrdering::Unordered:
  case AtomicOrdering::Monotonic:
    return 0; // __ATOMIC_RELAXED
  case AtomicOrdering::Acquire:
    return 2; // __ATOMIC_ACQUIRE
  case AtomicOrdering::Release:
    return 3; // __ATOMIC_RELEASE
  case AtomicOrdering::AcquireRelease:
    return 4; // __ATOMIC_ACQ_REL
  case AtomicOrdering::SequentiallyConsistent:
    return 5; // __ATOMIC_SEQ_CST
  }
  llvm_unreachable("unknown atomic ordering");
}

class C166AtomicLowering : public ModulePass {
public:
  static char ID;

  C166AtomicLowering() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;
  StringRef getPassName() const override {
    return "C166 atomic runtime lowering";
  }
};

class C166AtomicBuilder {
  Module &M;
  const DataLayout &DL;
  LLVMContext &Ctx;
  IntegerType *CIntTy;
  PointerType *DataPtrTy;

  Value *getSize(IRBuilder<> &Builder, Type *Ty) const {
    TypeSize Size = DL.getTypeStoreSize(Ty);
    if (Size.isScalable() || Size.getFixedValue() > UINT16_MAX) {
      Ctx.emitError("C166 atomic object size is not representable by 16-bit "
                    "size_t");
      return PoisonValue::get(CIntTy);
    }
    return Builder.getInt16(Size.getFixedValue());
  }

  Value *getOrder(IRBuilder<> &Builder, AtomicOrdering Ordering) const {
    return Builder.getInt16(getCAtomicOrder(Ordering));
  }

  Value *asDataPointer(IRBuilder<> &Builder, Value *Pointer,
                       const Twine &Name) const {
    if (Pointer->getType() == DataPtrTy)
      return Pointer;
    return Builder.CreateAddrSpaceCast(Pointer, DataPtrTy, Name);
  }

  AllocaInst *createTemporary(Instruction *At, Type *Ty,
                              const Twine &Name) const {
    Function &F = *At->getFunction();
    IRBuilder<> AllocaBuilder(&*F.getEntryBlock().getFirstInsertionPt());
    AllocaInst *Temporary = AllocaBuilder.CreateAlloca(Ty, nullptr, Name);
    Temporary->setAlignment(DL.getABITypeAlign(Ty));
    return Temporary;
  }

  CallInst *createCall(IRBuilder<> &Builder, StringRef Name, Type *ResultTy,
                       ArrayRef<Value *> Args, const DebugLoc &Loc) const {
    SmallVector<Type *, 7> ArgTypes;
    for (Value *Arg : Args)
      ArgTypes.push_back(Arg->getType());
    FunctionType *FnTy = FunctionType::get(ResultTy, ArgTypes, false);
    FunctionCallee Callee = M.getOrInsertFunction(Name, FnTy);
    CallInst *Call = Builder.CreateCall(Callee, Args);
    Call->setDebugLoc(Loc);
    Call->setDoesNotThrow();
    return Call;
  }

public:
  explicit C166AtomicBuilder(Module &M)
      : M(M), DL(M.getDataLayout()), Ctx(M.getContext()),
        CIntTy(Type::getInt16Ty(Ctx)),
        DataPtrTy(PointerType::get(Ctx, DL.getDefaultGlobalsAddressSpace())) {}

  void lowerLoad(LoadInst *Load) const;
  void lowerStore(StoreInst *Store) const;
  void lowerRMW(AtomicRMWInst *RMW) const;
  void lowerCmpXchg(AtomicCmpXchgInst *CmpXchg) const;
  void lowerFence(FenceInst *Fence) const;
};

} // namespace

char C166AtomicLowering::ID = 0;

INITIALIZE_PASS(C166AtomicLowering, "c166-atomic-lowering",
                "C166 atomic runtime lowering", false, false)

Pass *llvm::createC166AtomicLoweringPass() { return new C166AtomicLowering(); }

void C166AtomicBuilder::lowerLoad(LoadInst *Load) const {
  IRBuilder<> Builder(Load);
  Type *Ty = Load->getType();
  AllocaInst *Result = createTemporary(Load, Ty, Load->getName() + ".atomic");
  Value *Args[] = {
      getSize(Builder, Ty),
      asDataPointer(Builder, Load->getPointerOperand(), "atomic.object"),
      asDataPointer(Builder, Result, "atomic.result"),
      getOrder(Builder, Load->getOrdering()),
  };
  createCall(Builder, "__atomic_load", Builder.getVoidTy(), Args,
             Load->getDebugLoc());
  LoadInst *Value = Builder.CreateLoad(Ty, Result, Load->getName());
  Value->setAlignment(Result->getAlign());
  Value->setDebugLoc(Load->getDebugLoc());
  Load->replaceAllUsesWith(Value);
  Load->eraseFromParent();
}

void C166AtomicBuilder::lowerStore(StoreInst *Store) const {
  IRBuilder<> Builder(Store);
  Value *Stored = Store->getValueOperand();
  Type *Ty = Stored->getType();
  AllocaInst *ValueTmp = createTemporary(Store, Ty, "atomic.value");
  StoreInst *Copy = Builder.CreateStore(Stored, ValueTmp);
  Copy->setAlignment(ValueTmp->getAlign());
  Copy->setDebugLoc(Store->getDebugLoc());
  Value *Args[] = {
      getSize(Builder, Ty),
      asDataPointer(Builder, Store->getPointerOperand(), "atomic.object"),
      asDataPointer(Builder, ValueTmp, "atomic.value.pointer"),
      getOrder(Builder, Store->getOrdering()),
  };
  createCall(Builder, "__atomic_store", Builder.getVoidTy(), Args,
             Store->getDebugLoc());
  Store->eraseFromParent();
}

void C166AtomicBuilder::lowerRMW(AtomicRMWInst *RMW) const {
  bool IsExchange = RMW->getOperation() == AtomicRMWInst::Xchg;
  std::optional<C166AtomicRMWOp> Opcode =
      getAtomicRMWOpcode(RMW->getOperation());
  if (!IsExchange && !Opcode) {
    Ctx.emitError("C166 does not support this atomicrmw operation");
    return;
  }

  IRBuilder<> Builder(RMW);
  Type *Ty = RMW->getType();
  AllocaInst *ValueTmp = createTemporary(RMW, Ty, "atomic.rmw.value");
  AllocaInst *Result = createTemporary(RMW, Ty, "atomic.rmw.result");
  StoreInst *Copy = Builder.CreateStore(RMW->getValOperand(), ValueTmp);
  Copy->setAlignment(ValueTmp->getAlign());
  Copy->setDebugLoc(RMW->getDebugLoc());
  Value *Size = getSize(Builder, Ty);
  Value *Object =
      asDataPointer(Builder, RMW->getPointerOperand(), "atomic.object");
  Value *ValuePointer =
      asDataPointer(Builder, ValueTmp, "atomic.rmw.value.pointer");
  Value *ResultPointer =
      asDataPointer(Builder, Result, "atomic.rmw.result.pointer");
  Value *Order = getOrder(Builder, RMW->getOrdering());
  if (IsExchange) {
    Value *Args[] = {Size, Object, ValuePointer, ResultPointer, Order};
    createCall(Builder, "__atomic_exchange", Builder.getVoidTy(), Args,
               RMW->getDebugLoc());
  } else {
    Value *Args[] = {Size, Object, ValuePointer, ResultPointer,
                     Builder.getInt16(*Opcode)};
    createCall(Builder, "__c166_atomic_rmw", Builder.getVoidTy(), Args,
               RMW->getDebugLoc());
  }
  LoadInst *Old = Builder.CreateLoad(Ty, Result, RMW->getName());
  Old->setAlignment(Result->getAlign());
  Old->setDebugLoc(RMW->getDebugLoc());
  RMW->replaceAllUsesWith(Old);
  RMW->eraseFromParent();
}

void C166AtomicBuilder::lowerCmpXchg(AtomicCmpXchgInst *CmpXchg) const {
  IRBuilder<> Builder(CmpXchg);
  Type *Ty = CmpXchg->getCompareOperand()->getType();
  AllocaInst *Expected =
      createTemporary(CmpXchg, Ty, "atomic.cmpxchg.expected");
  AllocaInst *Desired = createTemporary(CmpXchg, Ty, "atomic.cmpxchg.desired");
  StoreInst *ExpectedCopy =
      Builder.CreateStore(CmpXchg->getCompareOperand(), Expected);
  ExpectedCopy->setAlignment(Expected->getAlign());
  ExpectedCopy->setDebugLoc(CmpXchg->getDebugLoc());
  StoreInst *DesiredCopy =
      Builder.CreateStore(CmpXchg->getNewValOperand(), Desired);
  DesiredCopy->setAlignment(Desired->getAlign());
  DesiredCopy->setDebugLoc(CmpXchg->getDebugLoc());

  Value *Args[] = {
      getSize(Builder, Ty),
      asDataPointer(Builder, CmpXchg->getPointerOperand(), "atomic.object"),
      asDataPointer(Builder, Expected, "atomic.expected"),
      asDataPointer(Builder, Desired, "atomic.desired"),
      getOrder(Builder, CmpXchg->getSuccessOrdering()),
      getOrder(Builder, CmpXchg->getFailureOrdering()),
  };
  CallInst *Succeeded =
      createCall(Builder, "__atomic_compare_exchange", Builder.getInt1Ty(),
                 Args, CmpXchg->getDebugLoc());
  Succeeded->addRetAttr(Attribute::ZExt);
  LoadInst *Old = Builder.CreateLoad(Ty, Expected, "atomic.cmpxchg.old");
  Old->setAlignment(Expected->getAlign());
  Old->setDebugLoc(CmpXchg->getDebugLoc());
  Value *Result = PoisonValue::get(CmpXchg->getType());
  Result = Builder.CreateInsertValue(Result, Old, 0, "atomic.cmpxchg.value");
  Result = Builder.CreateInsertValue(Result, Succeeded, 1, CmpXchg->getName());
  CmpXchg->replaceAllUsesWith(Result);
  CmpXchg->eraseFromParent();
}

void C166AtomicBuilder::lowerFence(FenceInst *Fence) const {
  IRBuilder<> Builder(Fence);
  Value *Args[] = {getOrder(Builder, Fence->getOrdering())};
  createCall(Builder, "__c166_atomic_fence", Builder.getVoidTy(), Args,
             Fence->getDebugLoc());
  Fence->eraseFromParent();
}

bool C166AtomicLowering::runOnModule(Module &M) {
  SmallVector<LoadInst *, 16> Loads;
  SmallVector<StoreInst *, 16> Stores;
  SmallVector<AtomicRMWInst *, 8> RMWs;
  SmallVector<AtomicCmpXchgInst *, 8> CmpXchgs;
  SmallVector<FenceInst *, 4> Fences;

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;
    for (Instruction &I : instructions(F)) {
      if (auto *Load = dyn_cast<LoadInst>(&I); Load && Load->isAtomic())
        Loads.push_back(Load);
      else if (auto *Store = dyn_cast<StoreInst>(&I);
               Store && Store->isAtomic())
        Stores.push_back(Store);
      else if (auto *RMW = dyn_cast<AtomicRMWInst>(&I))
        RMWs.push_back(RMW);
      else if (auto *CmpXchg = dyn_cast<AtomicCmpXchgInst>(&I))
        CmpXchgs.push_back(CmpXchg);
      else if (auto *Fence = dyn_cast<FenceInst>(&I))
        Fences.push_back(Fence);
    }
  }

  if (Loads.empty() && Stores.empty() && RMWs.empty() && CmpXchgs.empty() &&
      Fences.empty())
    return false;

  C166AtomicBuilder Builder(M);
  for (LoadInst *Load : Loads)
    Builder.lowerLoad(Load);
  for (StoreInst *Store : Stores)
    Builder.lowerStore(Store);
  for (AtomicRMWInst *RMW : RMWs)
    Builder.lowerRMW(RMW);
  for (AtomicCmpXchgInst *CmpXchg : CmpXchgs)
    Builder.lowerCmpXchg(CmpXchg);
  for (FenceInst *Fence : Fences)
    Builder.lowerFence(Fence);
  return true;
}
