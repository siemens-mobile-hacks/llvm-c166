//===-- C166ByValTailForwarding.cpp - Forward tail-call byval slots -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Prepare aggregate-return forwarding calls before floating-point memory
// legalization obscures their copy chains.  The DAG lowering independently
// verifies that every incoming stack slot and the hidden result block already
// occupy the callee's exact ABI offsets before emitting a sibling jump.
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"

using namespace llvm;

namespace {

StoreInst *getSingleStore(AllocaInst &Object, CallBase &Call) {
  StoreInst *Result = nullptr;
  for (User *U : Object.users()) {
    auto *I = dyn_cast<Instruction>(U);
    if (!I || I->getParent() != Call.getParent())
      return nullptr;
    if (I == &Call)
      continue;
    if (I->isDebugOrPseudoInst() || I->isLifetimeStartOrEnd())
      continue;
    if (auto *Load = dyn_cast<LoadInst>(I)) {
      if (Load->isSimple() && Load->getPointerOperand() == &Object)
        continue;
      return nullptr;
    }
    auto *Store = dyn_cast<StoreInst>(I);
    if (!Store || !Store->isSimple() || Store->getPointerOperand() != &Object ||
        Result)
      return nullptr;
    Result = Store;
  }
  return Result && Result->comesBefore(&Call) ? Result : nullptr;
}

Argument *findForwardedArgument(Value *Pointer, CallBase &Call) {
  SmallPtrSet<AllocaInst *, 4> Visited;
  while (auto *Object = dyn_cast<AllocaInst>(Pointer->stripPointerCasts())) {
    if (!Visited.insert(Object).second)
      return nullptr;
    StoreInst *Store = getSingleStore(*Object, Call);
    if (!Store)
      return nullptr;
    auto *Load = dyn_cast<LoadInst>(Store->getValueOperand());
    if (!Load || !Load->isSimple() || Load->getParent() != Call.getParent() ||
        !Load->comesBefore(Store))
      return nullptr;
    Pointer = Load->getPointerOperand();
  }

  auto *Arg = dyn_cast<Argument>(Pointer->stripPointerCasts());
  if (!Arg || !Arg->hasByValAttr())
    return nullptr;
  for (User *U : Arg->users()) {
    auto *I = dyn_cast<Instruction>(U);
    if (!I || I->getParent() != Call.getParent())
      return nullptr;
    if (I->isDebugOrPseudoInst())
      continue;
    auto *Load = dyn_cast<LoadInst>(I);
    if (!Load || !Load->isSimple() || Load->getPointerOperand() != Arg ||
        !Load->comesBefore(&Call))
      return nullptr;
  }
  return Arg;
}

} // namespace

bool llvm::forwardC166ByValTailArguments(Function &F) {
  if (F.isDeclaration() || F.hasFnAttribute(Attribute::OptimizeNone))
    return false;

  bool Changed = false;
  SmallVector<CallBase *, 4> Calls;
  for (Instruction &I : instructions(F))
    if (auto *Call = dyn_cast<CallBase>(&I))
      if (!isa<IntrinsicInst>(Call))
        Calls.push_back(Call);

  for (CallBase *Call : Calls) {
    if (Call->isInlineAsm() || !Call->getCalledFunction() ||
        !Call->getType()->isVoidTy() || Call->arg_empty() ||
        !Call->paramHasAttr(0, Attribute::StructRet))
      continue;

    for (unsigned I = 0; I != Call->arg_size(); ++I) {
      if (!Call->paramHasAttr(I, Attribute::ByVal))
        continue;
      Argument *Arg = findForwardedArgument(Call->getArgOperand(I), *Call);
      if (!Arg || Arg->getParamByValType() != Call->getParamByValType(I) ||
          Arg->getParamAlign().valueOrOne() <
              Call->getParamAlign(I).valueOrOne())
        continue;
      Call->setArgOperand(I, Arg);
      Changed = true;
    }
  }
  return Changed;
}
