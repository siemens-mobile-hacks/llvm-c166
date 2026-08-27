//===--- C166.h - C166 ToolChain -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_C166_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_C166_H

#include "BareMetal.h"

namespace clang::driver::toolchains {

class LLVM_LIBRARY_VISIBILITY C166ToolChain final : public BareMetal {
public:
  using BareMetal::BareMetal;

  const char *getDefaultLinker() const override;
  std::string getCompilerRT(const llvm::opt::ArgList &Args, StringRef Component,
                            FileType Type = ToolChain::FT_Static,
                            bool IsFortran = false) const override;
  void addClangCC1ASTargetOptions(
      const llvm::opt::ArgList &Args,
      llvm::opt::ArgStringList &CC1ASArgs) const override;
};

} // namespace clang::driver::toolchains

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_C166_H
