//===------ SemaC166.cpp ----- C166 target-specific routines -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/SemaC166.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/CharInfo.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"
#include "llvm/TargetParser/C166TargetParser.h"

using namespace clang;

SemaC166::SemaC166(Sema &S) : SemaBase(S) {}

static bool hasAttributedType(QualType Type, attr::Kind Kind) {
  for (;;) {
    if (const auto *TT = dyn_cast<TypedefType>(Type)) {
      Type = TT->desugar();
      continue;
    }
    const auto *AT = dyn_cast<AttributedType>(Type);
    if (!AT)
      return false;
    if (AT->getAttrKind() == Kind)
      return true;
    Type = AT->getModifiedType();
  }
}

std::optional<SemaC166::AttributedTypeResult>
SemaC166::handleFunctionAddressAttr(QualType Type, ParsedAttr &AL) {
  if (SemaRef.CheckAttrNoArgs(AL)) {
    AL.setInvalid();
    return std::nullopt;
  }

  const bool IsNear = AL.getKind() == ParsedAttr::AT_C166Near;
  const attr::Kind ThisKind = IsNear ? attr::C166Near : attr::C166Huge;
  const attr::Kind OtherKind = IsNear ? attr::C166Huge : attr::C166Near;
  if (hasAttributedType(Type, ThisKind)) {
    Diag(AL.getLoc(), diag::warn_duplicate_attribute_exact) << AL;
    return std::nullopt;
  }
  if (hasAttributedType(Type, OtherKind)) {
    Diag(AL.getLoc(), diag::err_attributes_are_not_compatible)
        << (IsNear ? "c166_huge" : "c166_near") << AL
        << AL.isRegularKeywordAttribute();
    AL.setInvalid();
    return std::nullopt;
  }

  const LangAS NewAS = getLangASFromTargetAS(
      IsNear ? llvm::C166::NearAddressSpace : llvm::C166::HugeCodeAddressSpace);
  const LangAS OldAS = Type.getAddressSpace();
  const std::optional<LangAS> DefaultAS =
      getASTContext().getTargetInfo().getDefaultFunctionAddressSpace();
  const bool OverridesDefault = DefaultAS && OldAS == *DefaultAS;
  if (OldAS != LangAS::Default && OldAS != NewAS && !OverridesDefault) {
    Diag(AL.getLoc(), diag::err_attributes_are_not_compatible)
        << AL << "function address space" << AL.isRegularKeywordAttribute();
    AL.setInvalid();
    return std::nullopt;
  }

  Attr *TypeAttr =
      IsNear ? static_cast<Attr *>(::new (getASTContext())
                                       C166NearAttr(getASTContext(), AL))
             : static_cast<Attr *>(::new (getASTContext())
                                       C166HugeAttr(getASTContext(), AL));
  QualType ModifiedType = Type;
  if (OverridesDefault)
    ModifiedType = getASTContext().removeAddrSpaceQualType(ModifiedType);
  QualType EquivalentType = ModifiedType;
  if (EquivalentType.getAddressSpace() != NewAS) {
    EquivalentType = getASTContext().removeAddrSpaceQualType(EquivalentType);
    EquivalentType =
        getASTContext().getAddrSpaceQualType(EquivalentType, NewAS);
  }
  return AttributedTypeResult{TypeAttr, ModifiedType, EquivalentType};
}

std::optional<SemaC166::AttributedTypeResult>
SemaC166::handleBankAttr(QualType Type, ParsedAttr &AL) {
  uint32_t Bank = 0;
  if (!SemaRef.checkUInt32Argument(AL, AL.getArgAsExpr(0), Bank)) {
    AL.setInvalid();
    return std::nullopt;
  }
  if (Bank == 0 || Bank > 255) {
    Diag(AL.getLoc(), diag::err_c166_code_bank_out_of_range);
    AL.setInvalid();
    return std::nullopt;
  }

  const LangAS OldAS = Type.getAddressSpace();
  const LangAS BankAS =
      getLangASFromTargetAS(llvm::C166::getCodeBankAddressSpace(Bank));
  const std::optional<LangAS> DefaultAS =
      getASTContext().getTargetInfo().getDefaultFunctionAddressSpace();
  const bool OverridesDefault = DefaultAS && OldAS == *DefaultAS &&
                                getASTContext().getTargetAddressSpace(OldAS) !=
                                    llvm::C166::NearAddressSpace &&
                                !hasAttributedType(Type, attr::C166Near) &&
                                !hasAttributedType(Type, attr::C166Huge);
  if (OldAS != LangAS::Default && OldAS != BankAS && !OverridesDefault) {
    if (getASTContext().getTargetAddressSpace(OldAS) ==
        llvm::C166::NearAddressSpace)
      Diag(AL.getLoc(), diag::err_c166_code_bank_near);
    else
      Diag(AL.getLoc(), diag::err_attributes_are_not_compatible)
          << AL << "address_space" << AL.isRegularKeywordAttribute();
    AL.setInvalid();
    return std::nullopt;
  }

  Attr *TypeAttr =
      ::new (getASTContext()) C166BankAttr(getASTContext(), AL, Bank);
  QualType ModifiedType = Type;
  if (OverridesDefault)
    ModifiedType = getASTContext().removeAddrSpaceQualType(ModifiedType);
  QualType EquivalentType = ModifiedType;
  if (EquivalentType.getAddressSpace() != BankAS)
    EquivalentType =
        getASTContext().getAddrSpaceQualType(EquivalentType, BankAS);
  return AttributedTypeResult{TypeAttr, ModifiedType, EquivalentType};
}

std::optional<SemaC166::AttributedTypeResult>
SemaC166::handleDataAddressAttr(QualType Type, ParsedAttr &AL) {
  if (SemaRef.CheckAttrNoArgs(AL)) {
    AL.setInvalid();
    return std::nullopt;
  }

  if (AL.getKind() == ParsedAttr::AT_C166XNear &&
      getASTContext().getTargetInfo().getTargetOpts().CodeModel == "small") {
    Diag(AL.getLoc(), diag::err_c166_xnear_memory_model);
    AL.setInvalid();
    return std::nullopt;
  }

  unsigned TargetAS;
  Attr *TypeAttr;
  switch (AL.getKind()) {
  case ParsedAttr::AT_C166Far:
    TargetAS = llvm::C166::FarDataAddressSpace;
    TypeAttr = ::new (getASTContext()) C166FarAttr(getASTContext(), AL);
    break;
  case ParsedAttr::AT_C166Near:
    TargetAS = llvm::C166::NearAddressSpace;
    TypeAttr = ::new (getASTContext()) C166NearAttr(getASTContext(), AL);
    break;
  case ParsedAttr::AT_C166XNear:
    TargetAS = llvm::C166::XNearDataAddressSpace;
    TypeAttr = ::new (getASTContext()) C166XNearAttr(getASTContext(), AL);
    break;
  case ParsedAttr::AT_C166Huge:
    TargetAS = llvm::C166::HugeDataAddressSpace;
    TypeAttr = ::new (getASTContext()) C166HugeAttr(getASTContext(), AL);
    break;
  case ParsedAttr::AT_C166SHuge:
    TargetAS = llvm::C166::SHugeDataAddressSpace;
    TypeAttr = ::new (getASTContext()) C166SHugeAttr(getASTContext(), AL);
    break;
  case ParsedAttr::AT_C166SFR:
    TargetAS = llvm::C166::SFRAddressSpace;
    TypeAttr = ::new (getASTContext()) C166SFRAttr(getASTContext(), AL);
    break;
  case ParsedAttr::AT_C166ESFR:
    TargetAS = llvm::C166::ESFRAddressSpace;
    TypeAttr = ::new (getASTContext()) C166ESFRAttr(getASTContext(), AL);
    break;
  default:
    llvm_unreachable("not a C166 data address-class attribute");
  }

  const LangAS NewAS = getLangASFromTargetAS(TargetAS);
  if (Type.getAddressSpace() != LangAS::Default) {
    if (Type.getAddressSpace() != NewAS) {
      Diag(AL.getLoc(), diag::err_attribute_address_multiple_qualifiers);
      AL.setInvalid();
      return std::nullopt;
    }
    Diag(AL.getLoc(),
         diag::warn_attribute_address_multiple_identical_qualifiers);
  }

  QualType EquivalentType = getASTContext().getAddrSpaceQualType(Type, NewAS);
  return AttributedTypeResult{TypeAttr, Type, EquivalentType};
}

void SemaC166::handleInterruptAttr(Decl *D, const ParsedAttr &AL) {
  auto *FD = dyn_cast<FunctionDecl>(D);
  if (!FD) {
    Diag(AL.getLoc(), diag::warn_attribute_wrong_decl_type)
        << AL << AL.isRegularKeywordAttribute() << ExpectedFunction;
    return;
  }

  if (!AL.checkExactlyNumArgs(SemaRef, 1) || !AL.isArgExpr(0))
    return;

  Expr *NumberExpr = AL.getArgAsExpr(0);
  std::optional<llvm::APSInt> Number =
      NumberExpr->getIntegerConstantExpr(getASTContext());
  if (!Number) {
    Diag(AL.getLoc(), diag::err_attribute_argument_type)
        << AL << AANT_ArgumentIntegerConstant << NumberExpr->getSourceRange();
    return;
  }

  int64_t Vector = Number->getSExtValue();
  if (Vector < -1 || Vector > 127) {
    Diag(AL.getLoc(), diag::err_attribute_argument_out_of_bounds)
        << AL << Vector << NumberExpr->getSourceRange();
    return;
  }

  const auto *FPT = FD->getType()->getAs<FunctionProtoType>();
  if (!FPT || FPT->getNumParams() != 0 || !FPT->getReturnType()->isVoidType()) {
    Diag(AL.getLoc(), diag::err_c166_interrupt_signature);
    return;
  }

  if (const auto *Existing = FD->getAttr<C166InterruptAttr>()) {
    if (Existing->getNumber() != Vector) {
      Diag(AL.getLoc(), diag::err_attributes_are_not_compatible)
          << AL << Existing
          << (AL.isRegularKeywordAttribute() ||
              Existing->isRegularKeywordAttribute());
      Diag(Existing->getLocation(), diag::note_conflicting_attribute);
    }
    return;
  }

  FD->addAttr(::new (getASTContext()) C166InterruptAttr(
      getASTContext(), AL, static_cast<int>(Vector)));
  FD->addAttr(UsedAttr::CreateImplicit(getASTContext()));
}

void SemaC166::handleRegisterBankAttr(Decl *D, const ParsedAttr &AL) {
  auto *FD = dyn_cast<FunctionDecl>(D);
  if (!FD) {
    Diag(AL.getLoc(), diag::warn_attribute_wrong_decl_type)
        << AL << AL.isRegularKeywordAttribute() << ExpectedFunction;
    return;
  }

  StringRef Name;
  if (!SemaRef.checkStringLiteralArgumentAttr(AL, 0, Name))
    return;
  if (!isValidAsciiIdentifier(Name)) {
    Diag(AL.getLoc(), diag::err_c166_register_bank_name);
    return;
  }

  if (const auto *Existing = FD->getAttr<C166RegisterBankAttr>()) {
    if (Existing->getName() != Name) {
      Diag(AL.getLoc(), diag::err_attributes_are_not_compatible)
          << AL << Existing
          << (AL.isRegularKeywordAttribute() ||
              Existing->isRegularKeywordAttribute());
      Diag(Existing->getLocation(), diag::note_conflicting_attribute);
    }
    return;
  }

  FD->addAttr(::new (getASTContext())
                  C166RegisterBankAttr(getASTContext(), AL, Name));
}

void SemaC166::handleSFRBitAttr(Decl *D, const ParsedAttr &AL) {
  if (!AL.diagnoseAppertainsTo(SemaRef, D) ||
      !AL.checkExactlyNumArgs(SemaRef, 2))
    return;

  auto *VD = cast<VarDecl>(D);
  if (!VD->hasExternalStorage() || VD->hasInit() ||
      VD->getType().getUnqualifiedType() != getASTContext().UnsignedIntTy) {
    Diag(AL.getLoc(), diag::err_c166_sfrbit_type) << AL;
    return;
  }

  uint32_t Address;
  uint32_t Bit;
  if (!SemaRef.checkUInt32Argument(AL, AL.getArgAsExpr(0), Address, 0) ||
      !SemaRef.checkUInt32Argument(AL, AL.getArgAsExpr(1), Bit, 1))
    return;

  const bool IsESFR = AL.getAttrName()->getName() == "c166_esfrbit";
  const uint32_t First = IsESFR ? 0xf100 : 0xff00;
  const uint32_t Last = IsESFR ? 0xf1de : 0xffde;
  if (Address < First || Address > Last || (Address & 1)) {
    Diag(AL.getLoc(), diag::err_c166_sfrbit_address)
        << AL << First << Last;
    return;
  }
  if (Bit >= 16) {
    Diag(AL.getLoc(), diag::err_c166_sfrbit_bit) << AL;
    return;
  }

  if (const auto *Existing = VD->getAttr<C166SFRBitAttr>()) {
    if (Existing->getAddress() != Address || Existing->getBit() != Bit ||
        Existing->isESFR() != IsESFR) {
      Diag(AL.getLoc(), diag::err_attributes_are_not_compatible)
          << AL << Existing
          << (AL.isRegularKeywordAttribute() ||
              Existing->isRegularKeywordAttribute());
      Diag(Existing->getLocation(), diag::note_conflicting_attribute);
    }
    return;
  }

  VD->addAttr(::new (getASTContext())
                  C166SFRBitAttr(getASTContext(), AL, Address, Bit));
}

void SemaC166::checkRegisterBankAttr(Decl *D) {
  const auto *Bank = D->getAttr<C166RegisterBankAttr>();
  if (!Bank || D->hasAttr<C166InterruptAttr>())
    return;
  Diag(Bank->getLocation(), diag::err_c166_register_bank_requires_interrupt);
  D->dropAttr<C166RegisterBankAttr>();
}

C166RegisterBankAttr *
SemaC166::mergeRegisterBankAttr(Decl *D, const C166RegisterBankAttr &AL) {
  if (const auto *Current = D->getAttr<C166RegisterBankAttr>()) {
    if (Current->getName() != AL.getName()) {
      Diag(Current->getLocation(), diag::err_attributes_are_not_compatible)
          << Current << &AL
          << (Current->isRegularKeywordAttribute() ||
              AL.isRegularKeywordAttribute());
      Diag(AL.getLocation(), diag::note_conflicting_attribute);
    }
    return nullptr;
  }
  return AL.clone(getASTContext());
}

C166SFRBitAttr *SemaC166::mergeSFRBitAttr(Decl *D,
                                          const C166SFRBitAttr &AL) {
  if (const auto *Current = D->getAttr<C166SFRBitAttr>()) {
    if (Current->getAddress() != AL.getAddress() ||
        Current->getBit() != AL.getBit() ||
        Current->isESFR() != AL.isESFR()) {
      Diag(Current->getLocation(), diag::err_attributes_are_not_compatible)
          << Current << &AL
          << (Current->isRegularKeywordAttribute() ||
              AL.isRegularKeywordAttribute());
      Diag(AL.getLocation(), diag::note_conflicting_attribute);
    }
    return nullptr;
  }
  return AL.clone(getASTContext());
}
