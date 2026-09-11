//===--- C166.cpp - C166 ToolChain ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "clang/Options/Options.h"
#include "llvm/Option/ArgList.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace llvm::opt;

const char *C166ToolChain::getDefaultLinker() const { return "ld.lld"; }

std::string C166ToolChain::getCompilerRT(const ArgList &Args,
                                         StringRef Component, FileType Type,
                                         bool IsFortran) const {
  if (Component == "builtins") {
    StringRef Model = Args.getLastArgValue(options::OPT_mcmodel_EQ, "large");
    if (Model == "tiny")
      Component = "builtins-tiny";
    else if (Model == "medium")
      Component = "builtins-medium";
    else if (Model == "small")
      Component = "builtins-small";
    else if (Model == "huge")
      Component = "builtins-huge";
  }
  return ToolChain::getCompilerRT(Args, Component, Type, IsFortran);
}

void C166ToolChain::addClangTargetOptions(
    const ArgList &Args, ArgStringList &CC1Args, BoundArch BA,
    Action::OffloadKind DeviceOffloadKind) const {
  BareMetal::addClangTargetOptions(Args, CC1Args, BA, DeviceOffloadKind);
  CC1Args.push_back("-target-abi");
  CC1Args.push_back(Args.MakeArgString(
      Args.getLastArgValue(options::OPT_mcmodel_EQ, "large")));
}

void C166ToolChain::addClangCC1ASTargetOptions(const ArgList &Args,
                                               ArgStringList &CC1ASArgs) const {
  CC1ASArgs.push_back("-target-abi");
  CC1ASArgs.push_back(Args.MakeArgString(
      Args.getLastArgValue(options::OPT_mcmodel_EQ, "large")));
}
