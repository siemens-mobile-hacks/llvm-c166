//===- C166.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"
#include "clang/AST/Decl.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/TargetParser/C166TargetParser.h"

using namespace clang;
using namespace clang::CodeGen;

namespace {

class C166ABIInfo : public DefaultABIInfo {
  ABIArgInfo classify(QualType Ty, bool IsReturn) const {
    // Only the low byte of a char register slot is defined.
    if (Ty->isIntegralOrEnumerationType() && getContext().getTypeSize(Ty) == 8)
      return ABIArgInfo::getDirect();

    if (Ty->isRealFloatingType()) {
      unsigned Bits = getContext().getTypeSize(Ty);
      if (Bits == 32)
        return ABIArgInfo::getDirect();

      // Double uses an indirect stack argument or caller-owned result slot.
      return ABIArgInfo::getIndirect(CharUnits::fromQuantity(2),
                                     getDataLayout().getAllocaAddrSpace(),
                                     /*ByVal=*/!IsReturn, /*Realign=*/false);
    }
    return IsReturn ? DefaultABIInfo::classifyReturnType(Ty)
                    : DefaultABIInfo::classifyArgumentType(Ty);
  }

public:
  explicit C166ABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  RValue EmitVAArg(CodeGenFunction &CGF, Address VAListAddr, QualType Ty,
                   AggValueSlot Slot) const override {
    // Variadic aggregates occupy inline stack slots.
    if (!isAggregateTypeForABI(Ty))
      return DefaultABIInfo::EmitVAArg(CGF, VAListAddr, Ty, Slot);

    if (isEmptyRecord(getContext(), Ty, /*AllowArrays=*/true))
      return Slot.asRValue();

    TypeInfoChars TypeInfo = getContext().getTypeInfoInChars(Ty);
    return emitVoidPtrVAArg(CGF, VAListAddr, Ty, /*IsIndirect=*/false, TypeInfo,
                            CharUnits::fromQuantity(2),
                            /*AllowHigherAlign=*/false, Slot);
  }

  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classify(FI.getReturnType(), true);
    for (auto &Arg : FI.arguments())
      Arg.info = classify(Arg.type, false);
  }
};

class C166TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  explicit C166TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<C166ABIInfo>(CGT)) {}

  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGenModule &CGM) const override {
    if (GV->isDeclaration())
      return;
    const auto *FD = dyn_cast_or_null<FunctionDecl>(D);
    const auto *Interrupt = FD ? FD->getAttr<C166InterruptAttr>() : nullptr;
    if (!Interrupt)
      return;

    auto *F = cast<llvm::Function>(GV);
    F->setCallingConv(llvm::CallingConv::C166_Interrupt);
    F->addFnAttr(llvm::Attribute::NoInline);
    F->addFnAttr("interrupt", llvm::itostr(Interrupt->getNumber()));

    const auto *Bank = FD->getAttr<C166RegisterBankAttr>();
    if (!Bank)
      return;

    std::string Symbol = ("__c166_register_bank_" + Bank->getName()).str();
    F->addFnAttr("c166-register-bank", Symbol);

    llvm::Module &M = CGM.getModule();
    llvm::GlobalVariable *BankStorage = M.getNamedGlobal(Symbol);
    if (!BankStorage) {
      llvm::Type *BankTy =
          llvm::ArrayType::get(llvm::Type::getInt16Ty(M.getContext()), 16);
      BankStorage = new llvm::GlobalVariable(
          M, BankTy, /*isConstant=*/false, llvm::GlobalValue::WeakAnyLinkage,
          llvm::ConstantAggregateZero::get(BankTy), Symbol,
          /*InsertBefore=*/nullptr, llvm::GlobalVariable::NotThreadLocal,
          /*AddressSpace=*/llvm::C166::NearAddressSpace);
      BankStorage->setAlignment(llvm::Align(2));
      // Banks with the same name coalesce in near memory.
      CGM.addCompilerUsedGlobal(BankStorage);
    }
  }

  unsigned getDwarfCallingConvention(const Decl *D,
                                     unsigned DefaultCC) const override {
    const auto *FD = dyn_cast_or_null<FunctionDecl>(D);
    return FD && FD->hasAttr<C166InterruptAttr>() ? 0x65 : DefaultCC;
  }
};

} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createC166TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<C166TargetCodeGenInfo>(CGM.getTypes());
}
