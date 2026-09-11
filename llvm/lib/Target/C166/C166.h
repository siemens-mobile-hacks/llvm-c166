//===-- C166.h - Top-level interface for C166 representation ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_C166_H
#define LLVM_LIB_TARGET_C166_C166_H

#include "MCTargetDesc/C166MCTargetDesc.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/TargetParser/C166TargetParser.h"

namespace llvm {

namespace C166 {
enum CondCode {
  CC_EQ,
  CC_NE,
  CC_ULT,
  CC_ULE,
  CC_UGE,
  CC_UGT,
  CC_SLT,
  CC_SLE,
  CC_SGE,
  CC_SGT,
  CC_N,
  CC_NN,
};
} // namespace C166

namespace C166II {
enum OperandTargetFlags {
  MO_None = 0,
  MO_SEG,
  MO_SOF,
  MO_PAG,
  MO_POF,
  MO_DPP1,
  MO_DPP2,
  MO_COF,
};
} // namespace C166II

class C166TargetMachine;
class Function;
class FunctionPass;
class Module;
class Pass;
class PassRegistry;

FunctionPass *createC166ISelDag(C166TargetMachine &TM,
                                CodeGenOptLevel OptLevel);
Pass *createC166FarPointerLoweringPass();
Pass *createC166AtomicLoweringPass();
Pass *createC166F64LoweringPass();
Pass *createC166FloatMemoryLoweringPass();
Pass *createC166SFRBitfieldLoweringPass();
Pass *createC166UnsupportedFeaturesPass();
FunctionPass *createC166ArgumentLoadSinkingPass();
FunctionPass *createC166PostISelPass();
FunctionPass *createC166PostRAPass();
FunctionPass *createC166LongBranchOptPass();
FunctionPass *createC166CallFrameExpansionPass();
FunctionPass *createC166FrameAddressRematerializationPass();
FunctionPass *createC166PHIEdgeSplittingPass();
bool lowerC166FloatMemory(Module &M);
bool lowerC166F64Operations(Module &M);
bool prepareC166FloatMemory(Module &M);
bool lowerC166PointerCasts(Function &F);
bool lowerC166SFRBitfields(Function &F);
bool lowerC166VAArgs(Module &M);
bool forwardC166ByValTailArguments(Function &F);

void initializeC166AsmPrinterPass(PassRegistry &);
void initializeC166AtomicLoweringPass(PassRegistry &);
void initializeC166FarPointerLoweringPass(PassRegistry &);
void initializeC166F64LoweringPass(PassRegistry &);
void initializeC166FloatMemoryLoweringPass(PassRegistry &);
void initializeC166SFRBitfieldLoweringPass(PassRegistry &);
void initializeC166UnsupportedFeaturesPass(PassRegistry &);
void initializeC166ArgumentLoadSinkingPass(PassRegistry &);
void initializeC166PostISelPass(PassRegistry &);
void initializeC166PostRAPass(PassRegistry &);
void initializeC166LongBranchOptPass(PassRegistry &);
void initializeC166CallFrameExpansionPass(PassRegistry &);
void initializeC166FrameAddressRematerializationPass(PassRegistry &);
void initializeC166PHIEdgeSplittingPass(PassRegistry &);
void initializeC166DAGToDAGISelLegacyPass(PassRegistry &);

} // namespace llvm

#endif
