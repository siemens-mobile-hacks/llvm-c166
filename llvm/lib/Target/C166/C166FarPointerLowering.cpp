//===-- C166FarPointerLowering.cpp - C166 far GEP lowering ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/Utils/Local.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsC166.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/TargetParser/C166TargetParser.h"
#include "llvm/Transforms/Utils/LowerMemIntrinsics.h"

using namespace llvm;

namespace {

bool isSegmentedAddressSpace(unsigned AddressSpace) {
  return AddressSpace == C166::HugeDataAddressSpace ||
         AddressSpace == C166::SHugeDataAddressSpace;
}

bool isNearAddressSpace(unsigned AddressSpace) {
  return AddressSpace == C166::NearAddressSpace ||
         AddressSpace == C166::XNearDataAddressSpace;
}

bool isStackAddress(Value *Value) {
  const auto *II = dyn_cast<IntrinsicInst>(Value);
  return II && II->getIntrinsicID() == Intrinsic::c166_stack_address;
}

unsigned getNearDPP(unsigned AddressSpace) {
  assert(isNearAddressSpace(AddressSpace) && "not a C166 near address space");
  return AddressSpace == C166::NearAddressSpace ? 2 : 1;
}

Value *readNearDPP(IRBuilder<> &Builder, Module &M, unsigned AddressSpace) {
  Function *ReadDPP =
      Intrinsic::getOrInsertDeclaration(&M, Intrinsic::c166_read_dpp);
  return Builder.CreateCall(
      ReadDPP, {Builder.getInt16(getNearDPP(AddressSpace))}, "near.cast.dpp");
}

Value *readDPP(IRBuilder<> &Builder, Module &M, unsigned Number) {
  Function *ReadDPP =
      Intrinsic::getOrInsertDeclaration(&M, Intrinsic::c166_read_dpp);
  return Builder.CreateCall(ReadDPP, {Builder.getInt16(Number)},
                            "direct.cast.dpp");
}

Value *readDirectPointerDPP(IRBuilder<> &Builder, Module &M, Value *Raw) {
  Value *Selector =
      Builder.CreateLShr(Raw, Builder.getInt16(14), "direct.cast.selector");
  Value *Page = readDPP(Builder, M, 0);
  for (unsigned Number = 1; Number != 4; ++Number) {
    Value *Matches = Builder.CreateICmpEQ(Selector, Builder.getInt16(Number),
                                          "direct.cast.selector.match");
    Page = Builder.CreateSelect(Matches, readDPP(Builder, M, Number), Page,
                                "direct.cast.page");
  }
  return Page;
}

Value *buildI32Words(IRBuilder<> &Builder, Value *Low, Value *High,
                     const Twine &Name) {
  Type *I32 = Builder.getInt32Ty();
  Value *Low32 = Builder.CreateZExt(Low, I32, Name + ".low");
  Value *High32 = Builder.CreateZExt(High, I32, Name + ".high");
  High32 = Builder.CreateShl(High32, Builder.getInt32(16), Name + ".highword");
  return Builder.CreateOr(Low32, High32, Name);
}

Value *convertDirectToLinear(IRBuilder<> &Builder, Module &M, Value *Raw) {
  Value *Offset =
      Builder.CreateAnd(Raw, Builder.getInt16(0x3fff), "direct.cast.offset");
  Value *Page = readDirectPointerDPP(Builder, M, Raw);
  Value *LowPage =
      Builder.CreateShl(Page, Builder.getInt16(14), "direct.cast.page.low");
  Value *Low = Builder.CreateOr(Offset, LowPage, "direct.cast.linear.low");
  Value *High =
      Builder.CreateLShr(Page, Builder.getInt16(2), "direct.cast.linear.high");
  return buildI32Words(Builder, Low, High, "direct.cast.linear");
}

Value *convertDirectToFar(IRBuilder<> &Builder, Module &M, Value *Raw,
                          Type *DestinationType) {
  Value *Linear = convertDirectToLinear(Builder, M, Raw);
  Value *IsNull =
      Builder.CreateICmpEQ(Raw, Builder.getInt16(0), "direct.cast.null");
  Linear = Builder.CreateSelect(IsNull, Builder.getInt32(0), Linear,
                                "direct.cast.linear.nonnull");
  Function *ToFar = Intrinsic::getOrInsertDeclaration(
      &M, Intrinsic::c166_linear_to_far, {DestinationType});
  return Builder.CreateCall(ToFar, {Linear}, "direct.cast.pointer");
}

Value *forceNearSelector(IRBuilder<> &Builder, Value *Raw,
                         unsigned AddressSpace) {
  assert(isNearAddressSpace(AddressSpace) && "not a C166 near address space");
  uint16_t ClearMask = AddressSpace == C166::NearAddressSpace ? 0xbfff : 0x7fff;
  uint16_t Selector = AddressSpace == C166::NearAddressSpace ? 0x8000 : 0x4000;
  Raw = Builder.CreateAnd(Raw, Builder.getInt16(ClearMask),
                          "near.cast.selector.clear");
  return Builder.CreateOr(Raw, Builder.getInt16(Selector),
                          "near.cast.selector");
}

Value *narrowFarOffset(IRBuilder<> &Builder, Value *Input) {
  Type *I16 = Builder.getInt16Ty();
  if (Input->getType() == I16)
    return Input;

  if (auto *Constant = dyn_cast<ConstantInt>(Input))
    return ConstantInt::get(I16, Constant->getValue().trunc(16));

  if (auto *Cast = dyn_cast<CastInst>(Input)) {
    Value *Operand = Cast->getOperand(0);
    switch (Cast->getOpcode()) {
    case Instruction::ZExt:
      return Builder.CreateZExtOrTrunc(Operand, I16, "far.offset.zext");
    case Instruction::SExt:
      return Builder.CreateSExtOrTrunc(Operand, I16, "far.offset.sext");
    case Instruction::Trunc:
      return narrowFarOffset(Builder, Operand);
    default:
      break;
    }
  }

  if (auto *Binary = dyn_cast<BinaryOperator>(Input)) {
    unsigned Opcode = Binary->getOpcode();
    if (Opcode == Instruction::Add || Opcode == Instruction::Sub ||
        Opcode == Instruction::Mul || Opcode == Instruction::Shl) {
      Value *LHS = narrowFarOffset(Builder, Binary->getOperand(0));
      Value *RHS = narrowFarOffset(Builder, Binary->getOperand(1));
      if (Opcode == Instruction::Mul) {
        if (auto *Scale = dyn_cast<ConstantInt>(RHS)) {
          int Log = Scale->getValue().exactLogBase2();
          if (Log >= 0)
            return Builder.CreateShl(LHS, Log, "far.offset.scale");
        }
      }
      return Builder.CreateBinOp(static_cast<Instruction::BinaryOps>(Opcode),
                                 LHS, RHS, "far.offset.op");
    }
  }

  return Builder.CreateTruncOrBitCast(Input, I16, "far.offset");
}

class C166FarPointerLowering : public FunctionPass {
public:
  static char ID;

  C166FarPointerLowering() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetTransformInfoWrapperPass>();
  }
  StringRef getPassName() const override { return "C166 far pointer lowering"; }
};

} // namespace

char C166FarPointerLowering::ID = 0;

INITIALIZE_PASS_BEGIN(C166FarPointerLowering, "c166-far-pointer-lowering",
                      "C166 far pointer lowering", false, false)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_END(C166FarPointerLowering, "c166-far-pointer-lowering",
                    "C166 far pointer lowering", false, false)

Pass *llvm::createC166FarPointerLoweringPass() {
  return new C166FarPointerLowering();
}

bool llvm::lowerC166PointerCasts(Function &F) {
  const bool IsSmallModel = F.getDataLayout().getDefaultGlobalsAddressSpace() ==
                            C166::NearAddressSpace;
  SmallVector<GetElementPtrInst *, 8> SHugeGEPs;
  SmallVector<PtrToIntInst *, 8> PtrToInts;
  SmallVector<IntToPtrInst *, 8> IntToPtrs;
  SmallVector<AddrSpaceCastInst *, 8> AddressSpaceCasts;
  SmallVector<ICmpInst *, 8> PointerComparisons;
  for (Instruction &I : instructions(F)) {
    if (auto *GEP = dyn_cast<GetElementPtrInst>(&I);
        GEP && GEP->getPointerAddressSpace() == C166::SHugeDataAddressSpace)
      SHugeGEPs.push_back(GEP);
    if (auto *Cast = dyn_cast<PtrToIntInst>(&I);
        Cast && (Cast->getPointerAddressSpace() == C166::FarDataAddressSpace ||
                 isNearAddressSpace(Cast->getPointerAddressSpace())))
      PtrToInts.push_back(Cast);
    if (auto *Cast = dyn_cast<IntToPtrInst>(&I);
        Cast && !isStackAddress(Cast->getOperand(0)) &&
        (Cast->getAddressSpace() == C166::FarDataAddressSpace ||
         isNearAddressSpace(Cast->getAddressSpace())))
      IntToPtrs.push_back(Cast);
    if (auto *Cast = dyn_cast<AddrSpaceCastInst>(&I)) {
      unsigned SourceAddressSpace = Cast->getSrcAddressSpace();
      unsigned DestinationAddressSpace = Cast->getDestAddressSpace();
      bool Supported = (isNearAddressSpace(SourceAddressSpace) &&
                        (DestinationAddressSpace == C166::FarDataAddressSpace ||
                         isNearAddressSpace(DestinationAddressSpace) ||
                         isSegmentedAddressSpace(DestinationAddressSpace))) ||
                       (SourceAddressSpace == C166::FarDataAddressSpace &&
                        (isNearAddressSpace(DestinationAddressSpace) ||
                         isSegmentedAddressSpace(DestinationAddressSpace))) ||
                       (isSegmentedAddressSpace(SourceAddressSpace) &&
                        (DestinationAddressSpace == C166::FarDataAddressSpace ||
                         isNearAddressSpace(DestinationAddressSpace) ||
                         isSegmentedAddressSpace(DestinationAddressSpace)));
      if (Supported)
        AddressSpaceCasts.push_back(Cast);
    }
    if (auto *Compare = dyn_cast<ICmpInst>(&I);
        Compare && Compare->getOperand(0)->getType()->isPointerTy() &&
        Compare->getOperand(0)->getType()->getPointerAddressSpace() ==
            C166::FarDataAddressSpace)
      PointerComparisons.push_back(Compare);
  }

  Module *M = F.getParent();
  Type *I32 = Type::getInt32Ty(F.getContext());
  for (PtrToIntInst *Cast : PtrToInts) {
    IRBuilder<> Builder(Cast);
    Value *Linear;
    if (IsSmallModel &&
        Cast->getPointerAddressSpace() == C166::NearAddressSpace) {
      Value *Raw =
          Builder.CreatePtrToAddr(Cast->getPointerOperand(), "c166.cast.raw");
      Linear = Cast->getType()->getIntegerBitWidth() <= 16
                   ? Raw
                   : convertDirectToLinear(Builder, *M, Raw);
    } else {
      Intrinsic::ID Conversion =
          Cast->getPointerAddressSpace() == C166::FarDataAddressSpace
              ? Intrinsic::c166_far_to_linear
              : Intrinsic::c166_near_to_linear;
      Function *PointerToLinear = Intrinsic::getOrInsertDeclaration(
          M, Conversion, {Cast->getPointerOperand()->getType()});
      Linear = Builder.CreateCall(PointerToLinear, {Cast->getPointerOperand()},
                                  "c166.cast.linear");
    }
    Linear = Builder.CreateZExtOrTrunc(Linear, Cast->getType());
    Cast->replaceAllUsesWith(Linear);
    Cast->eraseFromParent();
  }

  for (IntToPtrInst *Cast : IntToPtrs) {
    IRBuilder<> Builder(Cast);
    if (IsSmallModel && Cast->getAddressSpace() == C166::NearAddressSpace) {
      Value *Raw = Builder.CreateTruncOrBitCast(
          Cast->getOperand(0), Builder.getInt16Ty(), "c166.cast.raw");
      Value *Pointer =
          Builder.CreateIntToPtr(Raw, Cast->getType(), "c166.cast.pointer");
      Cast->replaceAllUsesWith(Pointer);
      Cast->eraseFromParent();
      continue;
    }
    Value *Linear =
        Builder.CreateZExtOrTrunc(Cast->getOperand(0), I32, "c166.cast.linear");
    Intrinsic::ID Conversion =
        Cast->getAddressSpace() == C166::FarDataAddressSpace
            ? Intrinsic::c166_linear_to_far
            : Intrinsic::c166_linear_to_near;
    Function *LinearToPointer =
        Intrinsic::getOrInsertDeclaration(M, Conversion, {Cast->getType()});
    Value *Pointer =
        Builder.CreateCall(LinearToPointer, {Linear}, "c166.cast.pointer");
    Cast->replaceAllUsesWith(Pointer);
    Cast->eraseFromParent();
  }

  for (AddrSpaceCastInst *Cast : AddressSpaceCasts) {
    IRBuilder<> Builder(Cast);
    unsigned SourceAddressSpace = Cast->getSrcAddressSpace();
    unsigned DestinationAddressSpace = Cast->getDestAddressSpace();
    Value *Raw =
        isSegmentedAddressSpace(SourceAddressSpace)
            ? Builder.CreatePtrToInt(Cast->getOperand(0), I32, "c166.cast.raw")
            : Builder.CreatePtrToAddr(Cast->getOperand(0), "c166.cast.raw");
    Value *Pointer;

    if (SourceAddressSpace == C166::FarDataAddressSpace &&
        DestinationAddressSpace == C166::XNearDataAddressSpace &&
        isa<AllocaInst>(getUnderlyingObject(Cast->getOperand(0)))) {
      Function *StackAddress = Intrinsic::getOrInsertDeclaration(
          M, Intrinsic::c166_stack_address, {Cast->getOperand(0)->getType()});
      Value *Direct = Builder.CreateCall(StackAddress, Cast->getOperand(0),
                                         "c166.stack.address");
      Pointer =
          Builder.CreateIntToPtr(Direct, Cast->getType(), "c166.stack.pointer");
    } else if (IsSmallModel && SourceAddressSpace == C166::NearAddressSpace &&
               DestinationAddressSpace == C166::FarDataAddressSpace) {
      Pointer = convertDirectToFar(Builder, *M, Raw, Cast->getType());
    } else if (IsSmallModel && SourceAddressSpace == C166::NearAddressSpace &&
               isSegmentedAddressSpace(DestinationAddressSpace)) {
      Value *Linear = convertDirectToLinear(Builder, *M, Raw);
      Pointer = Builder.CreateIntToPtr(Linear, Cast->getType(),
                                       "direct.cast.pointer");
    } else if (IsSmallModel &&
               DestinationAddressSpace == C166::NearAddressSpace &&
               SourceAddressSpace == C166::FarDataAddressSpace) {
      Value *Offset =
          Builder.CreateTrunc(Raw, Builder.getInt16Ty(), "direct.cast.offset");
      Value *Page = Builder.CreateTrunc(
          Builder.CreateLShr(Raw, Builder.getInt32(16), "direct.cast.page"),
          Builder.getInt16Ty(), "direct.cast.page16");
      Value *Selector =
          Builder.CreateShl(Page, Builder.getInt16(14), "direct.cast.selector");
      Value *Direct = Builder.CreateOr(Offset, Selector, "direct.cast.raw");
      Pointer = Builder.CreateIntToPtr(Direct, Cast->getType(),
                                       "direct.cast.pointer");
    } else if (IsSmallModel &&
               DestinationAddressSpace == C166::NearAddressSpace &&
               isSegmentedAddressSpace(SourceAddressSpace)) {
      Value *Direct =
          Builder.CreateTrunc(Raw, Builder.getInt16Ty(), "direct.cast.raw");
      Pointer = Builder.CreateIntToPtr(Direct, Cast->getType(),
                                       "direct.cast.pointer");
    } else if (isNearAddressSpace(SourceAddressSpace) &&
               DestinationAddressSpace == C166::FarDataAddressSpace) {
      Function *NearToFar = Intrinsic::getOrInsertDeclaration(
          M, Intrinsic::c166_near_to_far, {Cast->getType()});
      Pointer = Builder.CreateCall(
          NearToFar, {Raw, Builder.getInt16(getNearDPP(SourceAddressSpace))},
          "c166.cast.pointer");
    } else if ((isNearAddressSpace(SourceAddressSpace) ||
                SourceAddressSpace == C166::FarDataAddressSpace) &&
               isSegmentedAddressSpace(DestinationAddressSpace)) {
      Intrinsic::ID Conversion = isNearAddressSpace(SourceAddressSpace)
                                     ? Intrinsic::c166_near_to_linear
                                     : Intrinsic::c166_far_to_linear;
      Function *ToLinear = Intrinsic::getOrInsertDeclaration(
          M, Conversion, {Cast->getOperand(0)->getType()});
      Value *Linear = Builder.CreateCall(ToLinear, {Cast->getOperand(0)},
                                         "c166.cast.linear");
      Pointer =
          Builder.CreateIntToPtr(Linear, Cast->getType(), "c166.cast.pointer");
    } else if (isSegmentedAddressSpace(SourceAddressSpace) &&
               DestinationAddressSpace == C166::FarDataAddressSpace) {
      Raw = Builder.CreateZExtOrTrunc(Raw, I32, "c166.cast.raw32");
      Function *ToFar = Intrinsic::getOrInsertDeclaration(
          M, Intrinsic::c166_linear_to_far, {Cast->getType()});
      Pointer = Builder.CreateCall(ToFar, {Raw}, "c166.cast.pointer");
    } else if (isSegmentedAddressSpace(SourceAddressSpace) &&
               isSegmentedAddressSpace(DestinationAddressSpace)) {
      Pointer =
          Builder.CreateIntToPtr(Raw, Cast->getType(), "c166.cast.pointer");
    } else {
      Raw = Builder.CreateZExtOrTrunc(Raw, I32, "c166.cast.raw32");
      Function *ToNear = Intrinsic::getOrInsertDeclaration(
          M, Intrinsic::c166_linear_to_near, {Cast->getType()});
      Pointer = Builder.CreateCall(ToNear, {Raw}, "c166.cast.pointer");
    }

    Cast->replaceAllUsesWith(Pointer);
    Cast->eraseFromParent();
  }

  for (ICmpInst *Compare : PointerComparisons) {
    IRBuilder<> Builder(Compare);
    Value *LHS = Compare->getOperand(0);
    Value *RHS = Compare->getOperand(1);
    const bool IsNullComparison =
        isa<ConstantPointerNull>(LHS) || isa<ConstantPointerNull>(RHS);
    Value *LHSBits = Builder.CreatePtrToAddr(LHS, "c166.ptr.lhs");
    Value *RHSBits = Builder.CreatePtrToAddr(RHS, "c166.ptr.rhs");
    Value *Result;

    if (!IsNullComparison) {
      // The default comparison uses the 14-bit page offset
      // and does not compare the stored page word.
      LHSBits = Builder.CreateTrunc(LHSBits, Builder.getInt16Ty(),
                                    "c166.ptr.lhs.offset");
      RHSBits = Builder.CreateTrunc(RHSBits, Builder.getInt16Ty(),
                                    "c166.ptr.rhs.offset");
      Result = Builder.CreateICmp(Compare->getPredicate(), LHSBits, RHSBits,
                                  "c166.ptr.cmp");
    } else if (Compare->isEquality()) {
      // Null comparisons use both stored words.  Collapse them to one native
      // word comparison instead of keeping a zero-valued register pair live.
      Value *Bits = isa<ConstantPointerNull>(RHS) ? LHSBits : RHSBits;
      Function *HighWord =
          Intrinsic::getOrInsertDeclaration(M, Intrinsic::c166_high_word);
      Value *High = Builder.CreateCall(HighWord, {Bits}, "c166.ptr.high");
      Value *Low =
          Builder.CreateTrunc(Bits, Builder.getInt16Ty(), "c166.ptr.low");
      Value *Words = Builder.CreateOr(Low, High, "c166.ptr.words");
      Result = Builder.CreateICmp(Compare->getPredicate(), Words,
                                  Builder.getInt16(0), "c166.ptr.cmp");
    } else {
      Result = Builder.CreateICmp(Compare->getPredicate(), LHSBits, RHSBits,
                                  "c166.ptr.cmp");
    }

    Compare->replaceAllUsesWith(Result);
    Compare->eraseFromParent();
  }

  // _shuge arithmetic wraps the 16-bit offset without carrying into
  // the stored segment word.  Lower it at the start of the optimization
  // pipeline: otherwise target-independent GEP folding can turn
  // `(p + n) - p` into `n` before the late C166 pass sees the operation.  The
  // target intrinsic is deliberately opaque to generic optimizers and is
  // selected as the same low-word-only addition used by default _far.
  const DataLayout &DL = F.getDataLayout();
  Function *FarAdd =
      Intrinsic::getOrInsertDeclaration(F.getParent(), Intrinsic::c166_far_add);
  for (GetElementPtrInst *GEP : SHugeGEPs) {
    IRBuilder<> Builder(GEP);
    Value *Offset = emitGEPOffset(&Builder, DL, GEP);
    Offset = narrowFarOffset(Builder, Offset);
    Value *Base = Builder.CreatePtrToInt(GEP->getPointerOperand(),
                                         Builder.getInt32Ty(), "shuge.base");
    Value *Address =
        Builder.CreateCall(FarAdd, {Base, Offset}, "shuge.address");
    Value *Pointer =
        Builder.CreateIntToPtr(Address, GEP->getType(), "shuge.pointer");
    GEP->replaceAllUsesWith(Pointer);
    GEP->eraseFromParent();
  }

  return !SHugeGEPs.empty() || !PtrToInts.empty() || !IntToPtrs.empty() ||
         !AddressSpaceCasts.empty() || !PointerComparisons.empty();
}

bool C166FarPointerLowering::runOnFunction(Function &F) {
  SmallVector<LoadInst *, 8> HugeLoads;
  SmallVector<StoreInst *, 8> HugeStores;
  SmallVector<MemIntrinsic *, 8> MemoryIntrinsics;
  for (Instruction &I : instructions(F)) {
    if (auto *Load = dyn_cast<LoadInst>(&I);
        Load && Load->getPointerAddressSpace() == C166::HugeDataAddressSpace &&
        Load->getType()->isIntegerTy(32))
      HugeLoads.push_back(Load);
    if (auto *Store = dyn_cast<StoreInst>(&I);
        Store &&
        Store->getPointerAddressSpace() == C166::HugeDataAddressSpace &&
        Store->getValueOperand()->getType()->isIntegerTy(32))
      HugeStores.push_back(Store);

    auto *Memory = dyn_cast<MemIntrinsic>(&I);
    if (!Memory)
      continue;
    bool UsesFarData =
        Memory->getDestAddressSpace() == C166::FarDataAddressSpace ||
        isSegmentedAddressSpace(Memory->getDestAddressSpace());
    if (auto *Transfer = dyn_cast<MemTransferInst>(Memory))
      UsesFarData |=
          Transfer->getSourceAddressSpace() == C166::FarDataAddressSpace ||
          isSegmentedAddressSpace(Transfer->getSourceAddressSpace());
    if (UsesFarData)
      MemoryIntrinsics.push_back(Memory);
  }

  // A naturally aligned 32-bit _huge object may begin at offset 0xfffe.  Split
  // it into two segment-qualified word accesses so the second GEP carries into
  // the next 64 KiB segment.  _shuge objects cannot cross that boundary and
  // retain the compact two-instruction EXTS sequence selected below.
  for (LoadInst *Load : HugeLoads) {
    IRBuilder<> Builder(Load);
    Type *I16 = Builder.getInt16Ty();
    Value *Pointer = Load->getPointerOperand();
    Value *HighPointer = Builder.CreateConstGEP1_32(
        Builder.getInt8Ty(), Pointer, 2, "huge.load.high.ptr");
    LoadInst *Low = Builder.CreateAlignedLoad(I16, Pointer, Load->getAlign(),
                                              "huge.load.low");
    LoadInst *High = Builder.CreateAlignedLoad(
        I16, HighPointer, commonAlignment(Load->getAlign(), 2),
        "huge.load.high");
    Low->setVolatile(Load->isVolatile());
    High->setVolatile(Load->isVolatile());
    Low->copyMetadata(*Load);
    High->copyMetadata(*Load);
    Value *Result = buildI32Words(Builder, Low, High, "huge.load");
    Load->replaceAllUsesWith(Result);
    Load->eraseFromParent();
  }

  for (StoreInst *Store : HugeStores) {
    IRBuilder<> Builder(Store);
    Value *Pointer = Store->getPointerOperand();
    Value *StoredValue = Store->getValueOperand();
    Value *Low = Builder.CreateTrunc(StoredValue, Builder.getInt16Ty(),
                                     "huge.store.low");
    Value *High = Builder.CreateTrunc(Builder.CreateLShr(StoredValue,
                                                         Builder.getInt32(16),
                                                         "huge.store.highword"),
                                      Builder.getInt16Ty(), "huge.store.high");
    Value *HighPointer = Builder.CreateConstGEP1_32(
        Builder.getInt8Ty(), Pointer, 2, "huge.store.high.ptr");
    StoreInst *HighStore = Builder.CreateAlignedStore(
        High, HighPointer, commonAlignment(Store->getAlign(), 2));
    StoreInst *LowStore =
        Builder.CreateAlignedStore(Low, Pointer, Store->getAlign());
    HighStore->setVolatile(Store->isVolatile());
    LowStore->setVolatile(Store->isVolatile());
    HighStore->copyMetadata(*Store);
    LowStore->copyMetadata(*Store);
    Store->eraseFromParent();
  }

  const TargetTransformInfo &TTI =
      getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
  bool Changed = false;
  for (MemIntrinsic *Memory : MemoryIntrinsics) {
    bool Expanded = true;
    if (auto *Copy = dyn_cast<MemCpyInst>(Memory))
      expandMemCpyAsLoop(Copy, TTI);
    else if (auto *Move = dyn_cast<MemMoveInst>(Memory))
      Expanded = expandMemMoveAsLoop(Move, TTI);
    else if (auto *Set = dyn_cast<MemSetInst>(Memory))
      expandMemSetAsLoop(Set, TTI);
    if (Expanded) {
      Memory->eraseFromParent();
      Changed = true;
    }
  }

  Changed |= lowerC166PointerCasts(F);
  SmallVector<GetElementPtrInst *, 8> GEPs;
  SmallVector<IntrinsicInst *, 8> PointerConversions;
  for (Instruction &I : instructions(F)) {
    if (auto *GEP = dyn_cast<GetElementPtrInst>(&I);
        GEP && (GEP->getPointerAddressSpace() == 0 ||
                GEP->getPointerAddressSpace() == C166::FarDataAddressSpace))
      GEPs.push_back(GEP);
    if (auto *II = dyn_cast<IntrinsicInst>(&I);
        II && (II->getIntrinsicID() == Intrinsic::c166_far_to_linear ||
               II->getIntrinsicID() == Intrinsic::c166_linear_to_far ||
               II->getIntrinsicID() == Intrinsic::c166_near_to_linear ||
               II->getIntrinsicID() == Intrinsic::c166_linear_to_near ||
               II->getIntrinsicID() == Intrinsic::c166_near_to_far))
      PointerConversions.push_back(II);
  }

  const DataLayout &DL = F.getDataLayout();
  Function *FarAdd =
      Intrinsic::getOrInsertDeclaration(F.getParent(), Intrinsic::c166_far_add);

  for (GetElementPtrInst *GEP : GEPs) {
    IRBuilder<> Builder(GEP);
    Value *Offset = emitGEPOffset(&Builder, DL, GEP);
    Offset = narrowFarOffset(Builder, Offset);
    Value *Base = Builder.CreatePtrToAddr(GEP->getPointerOperand(), "far.base");
    Base = Builder.CreateZExtOrTrunc(Base, Builder.getInt32Ty(), "far.base32");
    Value *Address = Builder.CreateCall(FarAdd, {Base, Offset}, "far.address");
    Value *Pointer = Builder.CreateIntToPtr(Address, GEP->getType());
    GEP->replaceAllUsesWith(Pointer);
    GEP->eraseFromParent();
  }

  for (IntrinsicInst *Conversion : PointerConversions) {
    IRBuilder<> Builder(Conversion);
    Intrinsic::ID ID = Conversion->getIntrinsicID();
    if (ID == Intrinsic::c166_far_to_linear) {
      Value *Raw =
          Builder.CreatePtrToAddr(Conversion->getArgOperand(0), "far.cast.raw");
      // Far-pointer arithmetic is 16-bit wide.  The low word can therefore
      // contain a non-canonical one-past offset at a 16 KiB page boundary.
      // Preserve that word when converting to the linear huge representation;
      // adding it to the page base carries the boundary offset into the next
      // page, as required for a valid one-past pointer.
      Value *Offset =
          Builder.CreateAnd(Raw, Builder.getInt32(0xffff), "far.cast.offset");
      Value *Page =
          Builder.CreateLShr(Raw, Builder.getInt32(16), "far.cast.page");
      Value *LinearPage =
          Builder.CreateShl(Page, Builder.getInt32(14), "far.cast.linear.page");
      Value *Linear = Builder.CreateAdd(Offset, LinearPage, "far.cast.linear");
      Conversion->replaceAllUsesWith(Linear);
    } else if (ID == Intrinsic::c166_linear_to_far) {
      // Shifting in i32 intentionally discards input bits above the
      // representable 16-bit page.
      Value *Linear = Conversion->getArgOperand(0);
      Value *Offset = Builder.CreateAnd(Linear, Builder.getInt32(0x3fff),
                                        "far.cast.offset");
      Value *Page =
          Builder.CreateLShr(Linear, Builder.getInt32(14), "far.cast.page");
      Value *RawPage =
          Builder.CreateShl(Page, Builder.getInt32(16), "far.cast.raw.page");
      Value *Raw = Builder.CreateOr(Offset, RawPage, "far.cast.raw");
      Value *Pointer = Builder.CreateIntToPtr(Raw, Conversion->getType());
      Conversion->replaceAllUsesWith(Pointer);
    } else if (ID == Intrinsic::c166_near_to_linear) {
      Value *Pointer = Conversion->getArgOperand(0);
      unsigned AddressSpace = Pointer->getType()->getPointerAddressSpace();
      Value *Raw = Builder.CreatePtrToAddr(Pointer, "near.cast.raw");
      Value *Offset =
          Builder.CreateAnd(Raw, Builder.getInt16(0x3fff), "near.cast.offset");
      Value *DPP = readNearDPP(Builder, *F.getParent(), AddressSpace);
      Value *LowPage =
          Builder.CreateShl(DPP, Builder.getInt16(14), "near.cast.page.low");
      Value *Low = Builder.CreateOr(Offset, LowPage, "near.cast.linear.low");
      Value *High =
          Builder.CreateLShr(DPP, Builder.getInt16(2), "near.cast.linear.high");
      Value *Linear = buildI32Words(Builder, Low, High, "near.cast.linear");
      Conversion->replaceAllUsesWith(Linear);
    } else if (ID == Intrinsic::c166_linear_to_near) {
      unsigned AddressSpace = Conversion->getType()->getPointerAddressSpace();
      Value *Raw = Builder.CreateTrunc(Conversion->getArgOperand(0),
                                       Builder.getInt16Ty(), "near.cast.raw");
      Raw = forceNearSelector(Builder, Raw, AddressSpace);
      Value *Pointer = Builder.CreateIntToPtr(Raw, Conversion->getType());
      Conversion->replaceAllUsesWith(Pointer);
    } else {
      assert(ID == Intrinsic::c166_near_to_far &&
             "unexpected C166 pointer conversion");
      Value *RawNear = Conversion->getArgOperand(0);
      unsigned DPPNumber =
          cast<ConstantInt>(Conversion->getArgOperand(1))->getZExtValue();
      unsigned AddressSpace =
          DPPNumber == 2 ? C166::NearAddressSpace : C166::XNearDataAddressSpace;
      assert(getNearDPP(AddressSpace) == DPPNumber &&
             "invalid C166 near DPP selector");
      Value *Offset = Builder.CreateAnd(RawNear, Builder.getInt16(0x3fff),
                                        "near.cast.offset");
      Value *Page = readNearDPP(Builder, *F.getParent(), AddressSpace);
      Value *RawFar = buildI32Words(Builder, Offset, Page, "near.cast.far.raw");
      Value *IsNull =
          Builder.CreateICmpEQ(RawNear, Builder.getInt16(0), "near.cast.null");
      RawFar = Builder.CreateSelect(IsNull, Builder.getInt32(0), RawFar,
                                    "near.cast.far");
      Value *Pointer = Builder.CreateIntToPtr(RawFar, Conversion->getType());
      Conversion->replaceAllUsesWith(Pointer);
    }
    Conversion->eraseFromParent();
  }

  return Changed || !HugeLoads.empty() || !HugeStores.empty() ||
         !GEPs.empty() || !PointerConversions.empty();
}
