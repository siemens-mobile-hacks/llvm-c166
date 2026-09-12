//===-- C166VAArgLowering.cpp - C166 variadic argument lowering ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A C166 va_list is a pointer into the user-stack argument stream.  Expand the
// generic variadic intrinsics before scalar optimization so the pointer can be
// promoted to SSA and advanced in registers.
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsC166.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Alignment.h"

using namespace llvm;

namespace {

AllocaInst *getVAListAlloca(Value *Address) {
  return dyn_cast<AllocaInst>(Address->stripPointerCasts());
}

bool isLocalVAList(AllocaInst &Cursor) {
  if (!Cursor.isStaticAlloca() || Cursor.isArrayAllocation())
    return false;

  for (User *User : Cursor.users()) {
    auto *I = dyn_cast<Instruction>(User);
    if (!I)
      return false;
    if (I->isDebugOrPseudoInst() || I->isLifetimeStartOrEnd())
      continue;
    if (auto *Load = dyn_cast<LoadInst>(I)) {
      if (Load->isSimple() && Load->getType() == Cursor.getAllocatedType() &&
          getVAListAlloca(Load->getPointerOperand()) == &Cursor)
        continue;
      return false;
    }
    if (auto *Store = dyn_cast<StoreInst>(I)) {
      if (Store->isSimple() &&
          Store->getValueOperand()->getType() == Cursor.getAllocatedType() &&
          getVAListAlloca(Store->getPointerOperand()) == &Cursor)
        continue;
      return false;
    }
    if (auto *VAArg = dyn_cast<VAArgInst>(I)) {
      if (getVAListAlloca(VAArg->getPointerOperand()) == &Cursor)
        continue;
      return false;
    }
    auto *II = dyn_cast<IntrinsicInst>(I);
    if (II && (II->getIntrinsicID() == Intrinsic::vastart ||
               II->getIntrinsicID() == Intrinsic::vaend ||
               II->getIntrinsicID() == Intrinsic::vacopy))
      continue;
    return false;
  }
  return true;
}

bool canLowerVALists(Function &F) {
  if (F.hasFnAttribute(Attribute::OptimizeNone))
    return false;

  SmallPtrSet<AllocaInst *, 4> Cursors;
  for (Instruction &I : instructions(F)) {
    auto *II = dyn_cast<IntrinsicInst>(&I);
    if (!II || II->getIntrinsicID() != Intrinsic::vastart)
      continue;
    AllocaInst *Cursor = getVAListAlloca(II->getArgOperand(0));
    if (!Cursor || !isLocalVAList(*Cursor))
      return false;
    Cursors.insert(Cursor);
  }

  if (Cursors.empty())
    return false;

  auto IsCursor = [&](Value *Address) {
    return Cursors.contains(getVAListAlloca(Address));
  };
  for (Instruction &I : instructions(F)) {
    if (auto *VAArg = dyn_cast<VAArgInst>(&I)) {
      if (!IsCursor(VAArg->getPointerOperand()))
        return false;
      continue;
    }
    auto *II = dyn_cast<IntrinsicInst>(&I);
    if (!II)
      continue;
    switch (II->getIntrinsicID()) {
    case Intrinsic::vastart:
    case Intrinsic::vaend:
      if (!IsCursor(II->getArgOperand(0)))
        return false;
      break;
    case Intrinsic::vacopy:
      if (!IsCursor(II->getArgOperand(0)) || !IsCursor(II->getArgOperand(1)))
        return false;
      break;
    default:
      break;
    }
  }
  return true;
}

} // namespace

bool llvm::lowerC166VAArgs(Module &M) {
  SmallVector<IntrinsicInst *, 8> VAStarts;
  SmallVector<IntrinsicInst *, 8> VAEnds;
  SmallVector<IntrinsicInst *, 4> VACopies;
  SmallVector<VAArgInst *, 16> VAArgs;

  for (Function &F : M) {
    // Escaped cursors require observable memory updates and use the canonical
    // DAG lowering.  The early form is only useful when scalar promotion can
    // keep the complete cursor in SSA.
    if (F.isDeclaration() || !canLowerVALists(F))
      continue;
    for (Instruction &I : instructions(F)) {
      if (auto *VAArg = dyn_cast<VAArgInst>(&I)) {
        VAArgs.push_back(VAArg);
        continue;
      }
      auto *II = dyn_cast<IntrinsicInst>(&I);
      if (!II)
        continue;
      switch (II->getIntrinsicID()) {
      case Intrinsic::vastart:
        VAStarts.push_back(II);
        break;
      case Intrinsic::vaend:
        VAEnds.push_back(II);
        break;
      case Intrinsic::vacopy:
        VACopies.push_back(II);
        break;
      default:
        break;
      }
    }
  }

  if (VAStarts.empty() && VAEnds.empty() && VACopies.empty() && VAArgs.empty())
    return false;

  const DataLayout &DL = M.getDataLayout();
  PointerType *DataPtrTy =
      PointerType::get(M.getContext(), DL.getDefaultGlobalsAddressSpace());
  Function *Start = VAStarts.empty()
                        ? nullptr
                        : Intrinsic::getOrInsertDeclaration(
                              &M, Intrinsic::c166_va_start, {DataPtrTy});

  for (IntrinsicInst *VAStart : VAStarts) {
    // Generic inlining rejects functions containing llvm.va_start.  Preserve
    // that rule after replacing the generic intrinsic: the target intrinsic
    // denotes a frame-local fixed object and must not move into its caller.
    Function *F = VAStart->getFunction();
    F->removeFnAttr(Attribute::AlwaysInline);
    F->addFnAttr(Attribute::NoInline);
    // The replacement intrinsic refers to this function's incoming varargs
    // frame.  Prevent interprocedural passes from changing its signature after
    // llvm.va_start is no longer present for them to recognize.
    F->addFnAttr(Attribute::NoIPA);

    IRBuilder<> Builder(VAStart);
    CallInst *Address = Builder.CreateCall(Start, {}, "va.start");
    Address->setDebugLoc(VAStart->getDebugLoc());
    StoreInst *Store = Builder.CreateStore(Address, VAStart->getArgOperand(0));
    Store->setAlignment(Align(2));
    Store->setDebugLoc(VAStart->getDebugLoc());
    VAStart->eraseFromParent();
  }

  for (IntrinsicInst *VACopy : VACopies) {
    IRBuilder<> Builder(VACopy);
    LoadInst *Value =
        Builder.CreateLoad(DataPtrTy, VACopy->getArgOperand(1), "va.copy");
    Value->setAlignment(Align(2));
    Value->setDebugLoc(VACopy->getDebugLoc());
    StoreInst *Store = Builder.CreateStore(Value, VACopy->getArgOperand(0));
    Store->setAlignment(Align(2));
    Store->setDebugLoc(VACopy->getDebugLoc());
    VACopy->eraseFromParent();
  }

  for (VAArgInst *VAArg : VAArgs) {
    TypeSize StoreSize = DL.getTypeStoreSize(VAArg->getType());
    if (StoreSize.isScalable()) {
      M.getContext().emitError("C166 does not support scalable va_arg types");
      continue;
    }

    uint64_t SlotSize = alignTo(StoreSize.getFixedValue(), 2u);
    IRBuilder<> Builder(VAArg);
    LoadInst *Current = Builder.CreateLoad(
        DataPtrTy, VAArg->getPointerOperand(), VAArg->getName() + ".address");
    Current->setAlignment(Align(2));
    Current->setDebugLoc(VAArg->getDebugLoc());
    Value *Next = Builder.CreateGEP(Builder.getInt8Ty(), Current,
                                    Builder.getInt16(SlotSize),
                                    VAArg->getName() + ".next");
    StoreInst *Advance = Builder.CreateStore(Next, VAArg->getPointerOperand());
    Advance->setAlignment(Align(2));
    Advance->setDebugLoc(VAArg->getDebugLoc());
    LoadInst *Value = Builder.CreateLoad(VAArg->getType(), Current,
                                         VAArg->getName() + ".value");
    Value->setAlignment(Align(2));
    Value->setDebugLoc(VAArg->getDebugLoc());
    VAArg->replaceAllUsesWith(Value);
    VAArg->eraseFromParent();
  }

  for (IntrinsicInst *VAEnd : VAEnds)
    VAEnd->eraseFromParent();

  return true;
}
