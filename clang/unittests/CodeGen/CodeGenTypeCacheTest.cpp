//===- CodeGenTypeCacheTest.cpp - CodeGen type cache tests ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../../lib/CodeGen/CodeGenTypeCache.h"
#include "../../lib/CodeGen/CodeGenModule.h"
#include "TestCompiler.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "gtest/gtest.h"

using namespace clang;
using namespace clang::CodeGen;
using namespace llvm;

namespace {

TEST(CodeGenTypeCacheTest, IntegerTypesAreIndependent) {
  LLVMContext Context;
  CodeGenTypeCache Cache;

  Cache.IntPtrTy = llvm::Type::getInt32Ty(Context);
  Cache.SizeTy = llvm::Type::getInt16Ty(Context);
  Cache.PtrDiffTy = llvm::Type::getInt8Ty(Context);

  EXPECT_EQ(32U, Cache.IntPtrTy->getBitWidth());
  EXPECT_EQ(16U, Cache.SizeTy->getBitWidth());
  EXPECT_EQ(8U, Cache.PtrDiffTy->getBitWidth());
}

TEST(CodeGenTypeCacheTest, PointerSizeAndAlignmentAreIndependent) {
  CodeGenTypeCache Cache;

  Cache.PointerSizeInBytes = 2;
  Cache.PointerAlignInBytes = 1;

  EXPECT_EQ(2, Cache.getPointerSize().getQuantity());
  EXPECT_EQ(1, Cache.getPointerAlign().getQuantity());
}

TEST(CodeGenTypeCacheTest, SizeSizeAndAlignmentAreIndependent) {
  CodeGenTypeCache Cache;

  Cache.SizeSizeInBytes = 2;
  Cache.SizeAlignInBytes = 1;

  EXPECT_EQ(2, Cache.getSizeSize().getQuantity());
  EXPECT_EQ(1, Cache.getSizeAlign().getQuantity());
}

TEST(CodeGenTypeCacheTest, X86_64UsesDeclaredTypeAndPointerLayout) {
  LangOptions LangOpts;
  TestCompiler Compiler(LangOpts, CodeGenOptions(), "x86_64-unknown-linux-gnu");
  Compiler.init("void f(void) {}");
  Compiler.compile();

  auto &Generator =
      static_cast<CodeGenerator &>(Compiler.compiler.getASTConsumer());
  CodeGenModule &CGM = Generator.CGM();
  EXPECT_EQ(64U, CGM.IntPtrTy->getBitWidth());
  EXPECT_EQ(64U, CGM.SizeTy->getBitWidth());
  EXPECT_EQ(64U, CGM.PtrDiffTy->getBitWidth());
  EXPECT_EQ(8, CGM.getPointerSize().getQuantity());
  EXPECT_EQ(8, CGM.getPointerAlign().getQuantity());
  EXPECT_EQ(8, CGM.getSizeSize().getQuantity());
  EXPECT_EQ(8, CGM.getSizeAlign().getQuantity());
}

TEST(CodeGenTypeCacheTest, AVRUsesDeclaredTypeAndPointerLayout) {
  LangOptions LangOpts;
  TestCompiler Compiler(LangOpts, CodeGenOptions(), "avr-none-none");
  Compiler.init("void f(void) {}");
  Compiler.compile();

  auto &Generator =
      static_cast<CodeGenerator &>(Compiler.compiler.getASTConsumer());
  CodeGenModule &CGM = Generator.CGM();
  EXPECT_EQ(16U, CGM.IntPtrTy->getBitWidth());
  EXPECT_EQ(16U, CGM.SizeTy->getBitWidth());
  EXPECT_EQ(16U, CGM.PtrDiffTy->getBitWidth());
  EXPECT_EQ(2, CGM.getPointerSize().getQuantity());
  EXPECT_EQ(1, CGM.getPointerAlign().getQuantity());
  EXPECT_EQ(2, CGM.getSizeSize().getQuantity());
  EXPECT_EQ(1, CGM.getSizeAlign().getQuantity());
}

TEST(CodeGenTypeCacheTest, C166LargeIntegerWidthsAndPointerLayout) {
  LangOptions LangOpts;
  TestCompiler Compiler(LangOpts, CodeGenOptions(), "c166-none-elf");
  Compiler.init("void f(void) {}");
  Compiler.compile();

  auto &Generator =
      static_cast<CodeGenerator &>(Compiler.compiler.getASTConsumer());
  CodeGenModule &CGM = Generator.CGM();
  EXPECT_EQ(32U, CGM.IntPtrTy->getBitWidth());
  EXPECT_EQ(16U, CGM.SizeTy->getBitWidth());
  EXPECT_EQ(16U, CGM.PtrDiffTy->getBitWidth());
  EXPECT_EQ(4, CGM.getPointerSize().getQuantity());
  EXPECT_EQ(2, CGM.getPointerAlign().getQuantity());
  EXPECT_EQ(2, CGM.getSizeSize().getQuantity());
  EXPECT_EQ(2, CGM.getSizeAlign().getQuantity());
}

} // namespace
