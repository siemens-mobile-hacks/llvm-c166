//===-- C166F64Lowering.cpp - Lower binary64 operations -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Keep binary64 expression values in physical stack blocks and pass their
// addresses directly to the target runtime.  This avoids reconstructing the
// public stack-only C boundary around every compiler-generated operation.
// Ordinary C calls and all externally visible floating-point storage retain
// their ABI representation.
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsC166.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"
#include <optional>

using namespace llvm;

namespace {

static bool isStackObject(Value *Pointer) {
  Value *Object = getUnderlyingObject(Pointer);
  if (isa<AllocaInst>(Object))
    return true;
  const auto *Arg = dyn_cast<Argument>(Object);
  return Arg && (Arg->hasByValAttr() || Arg->hasStructRetAttr());
}

static bool isSRetObject(Value *Pointer) {
  const auto *Arg = dyn_cast<Argument>(getUnderlyingObject(Pointer));
  return Arg && Arg->hasStructRetAttr() && Arg->hasNoAliasAttr();
}

// Re-reading memory at a use is valid only while the loaded value is intact.
// Stay within one block; a stack object may escape or be overwritten too.
static bool hasNoWritesBetween(Instruction *Definition, Instruction *Use,
                               Instruction *IgnoredStore = nullptr) {
  if (Definition->getParent() != Use->getParent() ||
      !Definition->comesBefore(Use))
    return false;
  for (Instruction *I = Definition->getNextNode(); I != Use;
       I = I->getNextNode())
    if (I != IgnoredStore && I->mayWriteToMemory())
      return false;
  return true;
}

static Value *getF64LoadPointer(Value *V) {
  if (auto *Load = dyn_cast<LoadInst>(V))
    return Load->isSimple() && Load->getAlign() >= Align(2)
               ? Load->getPointerOperand()
               : nullptr;
  const auto *II = dyn_cast<IntrinsicInst>(V);
  if (II && II->getIntrinsicID() == Intrinsic::c166_float_load &&
      II->getType()->isDoubleTy() &&
      cast<ConstantInt>(II->getArgOperand(1))->getZExtValue() >= 2)
    return II->getArgOperand(0);
  return nullptr;
}

static Value *getF64StoreValue(User *U, Value *Slot) {
  if (auto *Store = dyn_cast<StoreInst>(U))
    return Store->isSimple() && Store->getPointerOperand() == Slot
               ? Store->getValueOperand()
               : nullptr;
  const auto *II = dyn_cast<IntrinsicInst>(U);
  if (II && II->getIntrinsicID() == Intrinsic::c166_float_store &&
      II->getArgOperand(1) == Slot &&
      cast<ConstantInt>(II->getArgOperand(2))->getZExtValue() >= 2)
    return II->getArgOperand(0);
  return nullptr;
}

static bool isF64LoadFrom(User *U, Value *Slot) {
  return getF64LoadPointer(cast<Value>(U)) == Slot;
}

static Value *getByValCopySource(Instruction &Load, Instruction &Use,
                               Value *Pointer, Instruction *&DefinitionOut) {
  auto *Slot = dyn_cast<AllocaInst>(Pointer);
  if (!Slot)
    return nullptr;

  Instruction *Definition = nullptr;
  Value *StoredValue = nullptr;
  for (User *User : Slot->users()) {
    if (isF64LoadFrom(User, Slot))
      continue;
    if (Value *Value = getF64StoreValue(User, Slot)) {
      if (Definition)
        return nullptr;
      Definition = cast<Instruction>(User);
      StoredValue = Value;
      continue;
    }
    auto *II = dyn_cast<IntrinsicInst>(User);
    if (!II || (!II->isLifetimeStartOrEnd() &&
                II->getIntrinsicID() != Intrinsic::dbg_declare))
      return nullptr;
  }

  if (!Definition || Definition->getParent() != Load.getParent() ||
      !Definition->comesBefore(&Load))
    return nullptr;

  Value *SourcePointer = getF64LoadPointer(StoredValue);
  if (!SourcePointer)
    return nullptr;
  const auto *Argument =
      dyn_cast<llvm::Argument>(getUnderlyingObject(SourcePointer));
  if (!Argument || !Argument->hasByValAttr())
    return nullptr;
  // Definition writes only the private, non-escaping copy described above.
  // Other writes, including calls, can invalidate the original byval value.
  if (!hasNoWritesBetween(cast<Instruction>(StoredValue), &Use, Definition))
    return nullptr;
  DefinitionOut = Definition;
  return SourcePointer;
}

static Value *getF64StorePointer(User *U, Value *StoredValue) {
  if (auto *Store = dyn_cast<StoreInst>(U))
    return Store->isSimple() && Store->getAlign() >= Align(2) &&
                   Store->getValueOperand() == StoredValue
               ? Store->getPointerOperand()
               : nullptr;
  const auto *II = dyn_cast<IntrinsicInst>(U);
  if (II && II->getIntrinsicID() == Intrinsic::c166_float_store &&
      II->getArgOperand(0) == StoredValue &&
      cast<ConstantInt>(II->getArgOperand(2))->getZExtValue() >= 2)
    return II->getArgOperand(1);
  return nullptr;
}

static bool isF64Arithmetic(Instruction &I) {
  if (I.getType()->isDoubleTy())
    if (unsigned Opcode = I.getOpcode();
        Opcode == Instruction::FAdd || Opcode == Instruction::FSub ||
        Opcode == Instruction::FMul || Opcode == Instruction::FDiv)
      return true;

  const auto *Intrinsic = dyn_cast<IntrinsicInst>(&I);
  return Intrinsic && Intrinsic->getType()->isDoubleTy() &&
         Intrinsic->getIntrinsicID() == Intrinsic::fmuladd;
}

static bool isI32ToF64Conversion(Instruction &I) {
  auto *Cast = dyn_cast<CastInst>(&I);
  if (!Cast || !Cast->getType()->isDoubleTy())
    return false;

  unsigned Opcode = Cast->getOpcode();
  if (Opcode != Instruction::SIToFP && Opcode != Instruction::UIToFP)
    return false;

  auto *SourceType = dyn_cast<IntegerType>(Cast->getOperand(0)->getType());
  return SourceType && SourceType->getBitWidth() <= 32;
}

static StringRef getRuntimeName(unsigned Opcode) {
  switch (Opcode) {
  case Instruction::FAdd:
    return "__c166_adddf3";
  case Instruction::FSub:
    return "__c166_subdf3";
  case Instruction::FMul:
    return "__c166_muldf3";
  case Instruction::FDiv:
    return "__c166_divdf3";
  default:
    llvm_unreachable("unexpected C166 binary64 operation");
  }
}

static AllocaInst *createF64Slot(Function &F, const Twine &Name) {
  IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());
  AllocaInst *Slot = Builder.CreateAlloca(
      Builder.getDoubleTy(), F.getDataLayout().getAllocaAddrSpace(), nullptr,
      Name);
  Slot->setAlignment(Align(2));
  return Slot;
}

static unsigned getStackAddressSpace(const DataLayout &DL) {
  return DL.getAllocaAddrSpace() == C166::NearAddressSpace
             ? C166::NearAddressSpace
             : C166::XNearDataAddressSpace;
}

static Value *createStackAddress(IRBuilder<> &Builder, Module &M,
                                 Value *Pointer, unsigned AddressSpace) {
  Function *Intrinsic = Intrinsic::getOrInsertDeclaration(
      &M, Intrinsic::c166_stack_address, {Pointer->getType()});
  Value *Raw =
      Builder.CreateCall(Intrinsic, Pointer, Pointer->getName() + ".direct");
  return Builder.CreateIntToPtr(Raw,
                                PointerType::get(M.getContext(), AddressSpace),
                                Pointer->getName() + ".stack");
}

static CallInst *createRuntimeCall(IRBuilder<> &Builder, Module &M,
                                   unsigned Opcode, Value *Destination,
                                   Value *Left, Value *Right) {
  unsigned AddressSpace = getStackAddressSpace(M.getDataLayout());
  PointerType *StackPointer = PointerType::get(M.getContext(), AddressSpace);
  FunctionType *FunctionTy = FunctionType::get(
      Builder.getVoidTy(), {StackPointer, StackPointer, StackPointer}, false);
  FunctionCallee Callee =
      M.getOrInsertFunction(getRuntimeName(Opcode), FunctionTy);
  if (auto *Function = dyn_cast<llvm::Function>(Callee.getCallee())) {
    Function->setDoesNotThrow();
    Function->addFnAttr(Attribute::NoFree);
    Function->addFnAttr(Attribute::WillReturn);
  }

  Value *Arguments[] = {
      createStackAddress(Builder, M, Destination, AddressSpace),
      createStackAddress(Builder, M, Left, AddressSpace),
      createStackAddress(Builder, M, Right, AddressSpace),
  };
  return Builder.CreateCall(Callee, Arguments);
}

static CallInst *createComparisonRuntimeCall(IRBuilder<> &Builder, Module &M,
                                             Value *Left, Value *Right) {
  unsigned AddressSpace = getStackAddressSpace(M.getDataLayout());
  PointerType *StackPointer = PointerType::get(M.getContext(), AddressSpace);
  FunctionType *FunctionTy = FunctionType::get(
      Builder.getInt16Ty(), {StackPointer, StackPointer}, false);
  FunctionCallee Callee = M.getOrInsertFunction("__c166_cmpdf2", FunctionTy);
  if (auto *Function = dyn_cast<llvm::Function>(Callee.getCallee())) {
    Function->setDoesNotThrow();
    Function->addFnAttr(Attribute::NoFree);
    Function->addFnAttr(Attribute::WillReturn);
    Function->setOnlyReadsMemory();
  }

  Value *Arguments[] = {
      createStackAddress(Builder, M, Left, AddressSpace),
      createStackAddress(Builder, M, Right, AddressSpace),
  };
  return Builder.CreateCall(Callee, Arguments, "f64.relation");
}

static CallInst *createConversionRuntimeCall(IRBuilder<> &Builder, Module &M,
                                             unsigned Opcode,
                                             Value *Destination,
                                             Value *Source) {
  unsigned AddressSpace = getStackAddressSpace(M.getDataLayout());
  PointerType *StackPointer = PointerType::get(M.getContext(), AddressSpace);
  FunctionType *FunctionTy = FunctionType::get(
      Builder.getVoidTy(), {StackPointer, Builder.getInt32Ty()}, false);
  StringRef Name =
      Opcode == Instruction::SIToFP ? "__c166_floatsidf" : "__c166_floatunsidf";
  FunctionCallee Callee = M.getOrInsertFunction(Name, FunctionTy);
  if (auto *Function = dyn_cast<llvm::Function>(Callee.getCallee())) {
    Function->setDoesNotThrow();
    Function->addFnAttr(Attribute::NoFree);
    Function->addFnAttr(Attribute::WillReturn);
    Function->addParamAttr(0, Attribute::WriteOnly);
  }

  bool IsSigned = Opcode == Instruction::SIToFP;
  Value *Extended = Builder.CreateIntCast(Source, Builder.getInt32Ty(),
                                          IsSigned, "f64.integer");
  Value *Arguments[] = {
      createStackAddress(Builder, M, Destination, AddressSpace), Extended};
  return Builder.CreateCall(Callee, Arguments);
}

static uint16_t getComparisonMask(FCmpInst::Predicate Predicate) {
  enum : uint16_t {
    Less = 1,
    Equal = 2,
    Greater = 4,
    Unordered = 8,
  };

  switch (Predicate) {
  case FCmpInst::FCMP_FALSE:
    return 0;
  case FCmpInst::FCMP_OEQ:
    return Equal;
  case FCmpInst::FCMP_OGT:
    return Greater;
  case FCmpInst::FCMP_OGE:
    return Greater | Equal;
  case FCmpInst::FCMP_OLT:
    return Less;
  case FCmpInst::FCMP_OLE:
    return Less | Equal;
  case FCmpInst::FCMP_ONE:
    return Less | Greater;
  case FCmpInst::FCMP_ORD:
    return Less | Equal | Greater;
  case FCmpInst::FCMP_UNO:
    return Unordered;
  case FCmpInst::FCMP_UEQ:
    return Equal | Unordered;
  case FCmpInst::FCMP_UGT:
    return Greater | Unordered;
  case FCmpInst::FCMP_UGE:
    return Greater | Equal | Unordered;
  case FCmpInst::FCMP_ULT:
    return Less | Unordered;
  case FCmpInst::FCMP_ULE:
    return Less | Equal | Unordered;
  case FCmpInst::FCMP_UNE:
    return Less | Greater | Unordered;
  case FCmpInst::FCMP_TRUE:
    return Less | Equal | Greater | Unordered;
  default:
    llvm_unreachable("unexpected binary64 comparison predicate");
  }
}

class C166F64Lowering : public ModulePass {
public:
  static char ID;

  C166F64Lowering() : ModulePass(ID) {}

  bool runOnModule(Module &M) override { return lowerC166F64Operations(M); }

  StringRef getPassName() const override { return "C166 binary64 lowering"; }
};

} // namespace

char C166F64Lowering::ID = 0;

INITIALIZE_PASS(C166F64Lowering, "c166-f64-lowering", "C166 binary64 lowering",
                false, false)

Pass *llvm::createC166F64LoweringPass() { return new C166F64Lowering(); }

bool llvm::lowerC166F64Operations(Module &M) {
  bool Changed = false;

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    SmallVector<CastInst *, 4> IntegerConversions;
    SmallVector<Instruction *, 8> Operations;
    SmallVector<FCmpInst *, 8> Comparisons;
    for (Instruction &I : instructions(F))
      if (isI32ToF64Conversion(I))
        IntegerConversions.push_back(cast<CastInst>(&I));
      else if (isF64Arithmetic(I))
        Operations.push_back(&I);
      else if (auto *Comparison = dyn_cast<FCmpInst>(&I);
               Comparison && Comparison->getOperand(0)->getType()->isDoubleTy())
        Comparisons.push_back(Comparison);
    if (IntegerConversions.empty() && Operations.empty() && Comparisons.empty())
      continue;

    DenseMap<Value *, Value *> Locations;
    DenseMap<Instruction *, Value *> Destinations;
    DenseMap<Instruction *, Instruction *> DirectStores;
    SmallVector<WeakTrackingVH, 16> PossiblyDead;
    SmallVector<std::pair<AllocaInst *, WeakTrackingVH>, 4> ForwardedCopies;
    SmallPtrSet<Instruction *, 4> SeenForwardedCopies;

    auto GetLocation = [&](Value *V, IRBuilder<> &Builder) -> Value * {
      if (Value *Location = Locations.lookup(V))
        return Location;

      if (Value *Pointer = getF64LoadPointer(V);
          Pointer && isStackObject(Pointer) &&
          hasNoWritesBetween(cast<Instruction>(V), &*Builder.GetInsertPoint())) {
        auto *Load = cast<Instruction>(V);
        Instruction *Definition = nullptr;
        if (Value *Source = getByValCopySource(
                *Load, *Builder.GetInsertPoint(), Pointer, Definition)) {
          Pointer = Source;
          if (SeenForwardedCopies.insert(Definition).second)
            ForwardedCopies.emplace_back(
                cast<AllocaInst>(getUnderlyingObject(getF64LoadPointer(V))),
                Definition);
        }
        // This proof applies to this use only, not later uses of V.
        PossiblyDead.push_back(Load);
        return Pointer;
      }

      AllocaInst *Slot = createF64Slot(F, V->getName() + ".f64");
      IRBuilder<> StoreBuilder(F.getContext());
      if (auto *Definition = dyn_cast<Instruction>(V)) {
        std::optional<BasicBlock::iterator> InsertAt =
            Definition->getInsertionPointAfterDef();
        if (!InsertAt) {
          // A callbr result has no single definition point dominating all of
          // its successors.  Keep a private copy at this use instead.
          StoreInst *Store = Builder.CreateStore(V, Slot);
          Store->setAlignment(Align(2));
          return Slot;
        }
        StoreBuilder.SetInsertPoint(*InsertAt);
      } else {
        StoreBuilder.SetInsertPoint(Slot->getNextNode());
      }
      StoreInst *Store = StoreBuilder.CreateStore(V, Slot);
      Store->setAlignment(Align(2));
      Locations[V] = Slot;
      return Slot;
    };

    auto FindDirectDestination = [&](Instruction *Producer) {
      if (!Producer->hasOneUse())
        return;
      // An adjacent store cannot have intervening observations, and its
      // address already dominates Producer. Otherwise retain the original
      // store and materialize the arithmetic result separately.
      if (Producer->getNextNode() != *Producer->user_begin())
        return;
      Value *Pointer = getF64StorePointer(*Producer->user_begin(), Producer);
      if (!Pointer || !isStackObject(Pointer))
        return;
      Destinations[Producer] = Pointer;
      DirectStores[Producer] = cast<Instruction>(*Producer->user_begin());
    };
    for (CastInst *Conversion : IntegerConversions)
      FindDirectDestination(Conversion);
    for (Instruction *Operation : Operations)
      FindDirectDestination(Operation);

    // A caller-owned result block can hold a one-use intermediate on its way
    // to the final value.  Runtime operations read both inputs before writing
    // the destination, so a parent may safely consume and overwrite that
    // intermediate in place.  Follow one path through each binary expression;
    // siblings must remain distinct because both are live at the parent.
    for (Instruction *Operation : reverse(Operations)) {
      Value *Destination = Destinations.lookup(Operation);
      if (!Destination || !isSRetObject(Destination) ||
          isa<IntrinsicInst>(Operation))
        continue;

      auto *Binary = cast<BinaryOperator>(Operation);
      for (Value *Operand : Binary->operands()) {
        auto *Child = dyn_cast<Instruction>(Operand);
        if (!Child ||
            (!isF64Arithmetic(*Child) && !isI32ToF64Conversion(*Child)) ||
            !Child->hasOneUse() || *Child->user_begin() != Operation ||
            Child->getNextNode() != Operation ||
            Destinations.count(Child))
          continue;
        Destinations[Child] = Destination;
        break;
      }
    }

    for (CastInst *Conversion : IntegerConversions) {
      IRBuilder<> Builder(Conversion);
      Builder.SetCurrentDebugLocation(Conversion->getDebugLoc());

      Instruction *DirectStore = DirectStores.lookup(Conversion);
      Value *Destination = Destinations.lookup(Conversion);
      if (!Destination)
        Destination = createF64Slot(F, Conversion->getName() + ".f64");
      CallInst *RuntimeCall =
          createConversionRuntimeCall(Builder, M, Conversion->getOpcode(),
                                      Destination, Conversion->getOperand(0));
      RuntimeCall->setDebugLoc(Conversion->getDebugLoc());

      LoadInst *Result = Builder.CreateLoad(Builder.getDoubleTy(), Destination,
                                            Conversion->getName() + ".value");
      Result->setAlignment(Align(2));
      Result->setDebugLoc(Conversion->getDebugLoc());
      if (!Destinations.count(Conversion))
        Locations[Result] = Destination;
      Conversion->replaceAllUsesWith(Result);
      PossiblyDead.push_back(Result);

      if (DirectStore)
        DirectStore->eraseFromParent();
      Conversion->eraseFromParent();
      Changed = true;
    }

    for (Instruction *Operation : Operations) {
      IRBuilder<> Builder(Operation);
      Builder.SetCurrentDebugLocation(Operation->getDebugLoc());

      Instruction *DirectStore = DirectStores.lookup(Operation);
      Value *Destination = Destinations.lookup(Operation);
      if (!Destination)
        Destination = createF64Slot(F, Operation->getName() + ".f64");
      CallInst *RuntimeCall;
      if (auto *Fused = dyn_cast<IntrinsicInst>(Operation)) {
        Value *Left = GetLocation(Fused->getArgOperand(0), Builder);
        Value *Right = GetLocation(Fused->getArgOperand(1), Builder);
        Value *Addend = GetLocation(Fused->getArgOperand(2), Builder);
        Value *Product = Destination;
        // The addend may still refer to the destination, including sret.
        // Do not overwrite it with the product before the addition reads it.
        if (Destinations.count(Operation) &&
            (!isSRetObject(Destination) ||
             getUnderlyingObject(Addend) == getUnderlyingObject(Destination)))
          Product = createF64Slot(F, "f64.product");
        createRuntimeCall(Builder, M, Instruction::FMul, Product, Left, Right);
        RuntimeCall = createRuntimeCall(Builder, M, Instruction::FAdd,
                                        Destination, Product, Addend);
      } else {
        auto *Binary = cast<BinaryOperator>(Operation);
        Value *Left = GetLocation(Binary->getOperand(0), Builder);
        Value *Right = GetLocation(Binary->getOperand(1), Builder);
        RuntimeCall = createRuntimeCall(Builder, M, Binary->getOpcode(),
                                        Destination, Left, Right);
      }
      RuntimeCall->setDebugLoc(Operation->getDebugLoc());

      LoadInst *Result = Builder.CreateLoad(Builder.getDoubleTy(), Destination,
                                            Operation->getName() + ".value");
      Result->setAlignment(Align(2));
      Result->setDebugLoc(Operation->getDebugLoc());
      if (!Destinations.count(Operation))
        Locations[Result] = Destination;
      Operation->replaceAllUsesWith(Result);
      PossiblyDead.push_back(Result);

      if (DirectStore)
        DirectStore->eraseFromParent();
      Operation->eraseFromParent();
      Changed = true;
    }

    struct CachedComparison {
      Value *Left;
      Value *Right;
      Value *Relation;
    };
    DenseMap<BasicBlock *, SmallVector<CachedComparison, 4>> CachedComparisons;

    for (FCmpInst *Comparison : Comparisons) {
      IRBuilder<> Builder(Comparison);
      Builder.SetCurrentDebugLocation(Comparison->getDebugLoc());

      uint16_t Mask = getComparisonMask(Comparison->getPredicate());
      if (Mask == 0 || Mask == 15) {
        Comparison->replaceAllUsesWith(Builder.getInt1(Mask != 0));
        Comparison->eraseFromParent();
        Changed = true;
        continue;
      }

      Value *Left = Comparison->getOperand(0);
      Value *Right = Comparison->getOperand(1);
      Value *Relation = nullptr;
      for (const CachedComparison &Cached :
           CachedComparisons[Comparison->getParent()]) {
        if (Cached.Left == Left && Cached.Right == Right) {
          Relation = Cached.Relation;
          break;
        }
      }

      if (!Relation) {
        Value *LeftLocation = GetLocation(Left, Builder);
        Value *RightLocation = GetLocation(Right, Builder);
        CallInst *RuntimeCall = createComparisonRuntimeCall(
            Builder, M, LeftLocation, RightLocation);
        RuntimeCall->setDebugLoc(Comparison->getDebugLoc());
        Relation = RuntimeCall;
        CachedComparisons[Comparison->getParent()].push_back(
            {Left, Right, Relation});
      }

      Value *Selected = Builder.CreateAnd(Relation, Mask, "f64.selected");
      Value *Result = Builder.CreateICmpNE(Selected, Builder.getInt16(0),
                                           Comparison->getName());
      Comparison->replaceAllUsesWith(Result);
      Comparison->eraseFromParent();
      Changed = true;
    }

    RecursivelyDeleteTriviallyDeadInstructionsPermissive(PossiblyDead);

    for (auto &[Slot, Handle] : ForwardedCopies) {
      auto *Definition = dyn_cast_or_null<Instruction>(Handle);
      if (!Definition)
        continue;

      SmallVector<Instruction *, 2> Markers;
      bool HasLiveAccess = false;
      for (User *User : Slot->users()) {
        if (User == Definition)
          continue;
        auto *II = dyn_cast<IntrinsicInst>(User);
        if (II && (II->isLifetimeStartOrEnd() ||
                   II->getIntrinsicID() == Intrinsic::dbg_declare)) {
          Markers.push_back(II);
          continue;
        }
        HasLiveAccess = true;
        break;
      }
      if (HasLiveAccess)
        continue;

      Value *StoredValue = getF64StoreValue(Definition, Slot);
      Definition->eraseFromParent();
      for (Instruction *Marker : Markers)
        Marker->eraseFromParent();
      if (auto *StoredI = dyn_cast_or_null<Instruction>(StoredValue)) {
        SmallVector<WeakTrackingVH, 1> DeadStoredValue = {StoredI};
        RecursivelyDeleteTriviallyDeadInstructionsPermissive(DeadStoredValue);
      }
      if (Slot->use_empty())
        Slot->eraseFromParent();
    }
  }

  return Changed;
}
