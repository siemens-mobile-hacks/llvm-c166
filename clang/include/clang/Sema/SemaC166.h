//===----- SemaC166.h ----- C166 target-specific routines ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_SEMAC166_H
#define LLVM_CLANG_SEMA_SEMAC166_H

#include "clang/AST/ASTFwd.h"
#include "clang/AST/Type.h"
#include "clang/Sema/SemaBase.h"
#include <optional>

namespace clang {
class C166RegisterBankAttr;
class C166SFRBitAttr;
class ParsedAttr;

class SemaC166 : public SemaBase {
public:
  struct AttributedTypeResult {
    Attr *TypeAttr;
    QualType ModifiedType;
    QualType EquivalentType;
  };

  SemaC166(Sema &S);

  std::optional<AttributedTypeResult> handleFunctionAddressAttr(QualType Type,
                                                                ParsedAttr &AL);
  std::optional<AttributedTypeResult> handleBankAttr(QualType Type,
                                                     ParsedAttr &AL);
  std::optional<AttributedTypeResult> handleDataAddressAttr(QualType Type,
                                                            ParsedAttr &AL);
  void handleInterruptAttr(Decl *D, const ParsedAttr &AL);
  void handleRegisterBankAttr(Decl *D, const ParsedAttr &AL);
  void handleSFRBitAttr(Decl *D, const ParsedAttr &AL);
  void checkRegisterBankAttr(Decl *D);
  C166RegisterBankAttr *mergeRegisterBankAttr(Decl *D,
                                              const C166RegisterBankAttr &AL);
  C166SFRBitAttr *mergeSFRBitAttr(Decl *D, const C166SFRBitAttr &AL);
};
} // namespace clang

#endif // LLVM_CLANG_SEMA_SEMAC166_H
