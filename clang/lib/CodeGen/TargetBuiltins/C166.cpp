//===------ C166.cpp - Emit LLVM code for C166 builtins -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CGBuiltin.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/IR/IntrinsicsC166.h"

using namespace clang;
using namespace CodeGen;
using namespace llvm;

Value *CodeGenFunction::EmitC166BuiltinExpr(unsigned BuiltinID,
                                            const CallExpr *E) {
  switch (BuiltinID) {
  default:
    return nullptr;
  case clang::C166::BI__builtin_c166_divlu: {
    Value *Dividend = EmitScalarExpr(E->getArg(0));
    Value *Divisor = EmitScalarExpr(E->getArg(1));
    return Builder.CreateCall(CGM.getIntrinsic(Intrinsic::c166_divlu),
                              {Dividend, Divisor});
  }
  }
}
