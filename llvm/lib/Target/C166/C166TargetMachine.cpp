//===-- C166TargetMachine.cpp - C166 target initialization ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166TargetMachine.h"
#include "C166.h"
#include "C166MachineFunctionInfo.h"
#include "C166TargetObjectFile.h"
#include "TargetInfo/C166TargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/PassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/C166TargetParser.h"

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeC166Target() {
  RegisterTargetMachine<C166TargetMachine> X(getTheC166Target());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeC166AsmPrinterPass(PR);
  initializeC166AtomicLoweringPass(PR);
  initializeC166FarPointerLoweringPass(PR);
  initializeC166F64LoweringPass(PR);
  initializeC166FloatMemoryLoweringPass(PR);
  initializeC166SFRBitfieldLoweringPass(PR);
  initializeC166UnsupportedFeaturesPass(PR);
  initializeC166ArgumentLoadSinkingPass(PR);
  initializeC166PostISelPass(PR);
  initializeC166PostRAPass(PR);
  initializeC166LongBranchOptPass(PR);
  initializeC166CallFrameExpansionPass(PR);
  initializeC166FrameAddressRematerializationPass(PR);
  initializeC166PHIEdgeSplittingPass(PR);
  initializeC166DAGToDAGISelLegacyPass(PR);
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

static CodeModel::Model
getEffectiveC166CodeModel(std::optional<CodeModel::Model> CM) {
  return getEffectiveCodeModel(CM, CodeModel::Large);
}

static StringRef computeC166DataLayout(std::optional<CodeModel::Model> CM) {
  switch (getEffectiveC166CodeModel(CM)) {
  case CodeModel::Small:
    return C166::getDataLayout(C166::MemoryModel::Small);
  case CodeModel::Medium:
    return C166::getDataLayout(C166::MemoryModel::Medium);
  default:
    return C166::getDataLayout(C166::MemoryModel::Large);
  }
}

C166TargetMachine::C166TargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS,
                                     const TargetOptions &Options,
                                     std::optional<Reloc::Model> RM,
                                     std::optional<CodeModel::Model> CM,
                                     CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, computeC166DataLayout(CM), TT, CPU, FS,
                               Options, getEffectiveRelocModel(RM),
                               getEffectiveC166CodeModel(CM), OL),
      TLOF(std::make_unique<C166TargetObjectFile>()),
      Subtarget(TT, CPU, FS, *this) {
  initAsmInfo();
}

C166TargetMachine::~C166TargetMachine() = default;

MachineFunctionInfo *C166TargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return C166MachineFunctionInfo::create<C166MachineFunctionInfo>(Allocator, F,
                                                                  STI);
}

namespace {
class C166FarPointerEarlyPass
    : public OptionalPassInfoMixin<C166FarPointerEarlyPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    return lowerC166PointerCasts(F) ? PreservedAnalyses::none()
                                    : PreservedAnalyses::all();
  }
};

class C166FloatMemoryEarlyPass
    : public OptionalPassInfoMixin<C166FloatMemoryEarlyPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    return prepareC166FloatMemory(M) ? PreservedAnalyses::none()
                                     : PreservedAnalyses::all();
  }
};

class C166ByValTailForwardingEarlyPass
    : public OptionalPassInfoMixin<C166ByValTailForwardingEarlyPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    return forwardC166ByValTailArguments(F) ? PreservedAnalyses::none()
                                            : PreservedAnalyses::all();
  }
};

class C166VAArgEarlyPass : public OptionalPassInfoMixin<C166VAArgEarlyPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    return lowerC166VAArgs(M) ? PreservedAnalyses::none()
                              : PreservedAnalyses::all();
  }
};

class C166SFRBitfieldEarlyPass
    : public OptionalPassInfoMixin<C166SFRBitfieldEarlyPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    return lowerC166SFRBitfields(F) ? PreservedAnalyses::none()
                                    : PreservedAnalyses::all();
  }
};
} // namespace

void C166TargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {
  PB.registerPipelineStartEPCallback(
      [](ModulePassManager &MPM, OptimizationLevel Level) {
        // At optimization levels which run scalar promotion, expose local
        // va_list cursors early enough for them to remain in SSA.  Escaped and
        // unoptimized cursors retain the canonical variadic DAG lowering.
        if (Level != OptimizationLevel::O0)
          MPM.addPass(C166VAArgEarlyPass());

        if (Level != OptimizationLevel::O0) {
          FunctionPassManager FPM;
          FPM.addPass(C166ByValTailForwardingEarlyPass());
          MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
        }

        // LLVM's DataLayout cannot express the ABI's type-dependent word
        // order.  Wrap floating accesses before SROA can fold a float/word
        // union through ordinary little-endian bitcast semantics.  The late
        // target pass expands the wrappers and handles accesses introduced by
        // later optimization passes.
        MPM.addPass(C166FloatMemoryEarlyPass());
        FunctionPassManager FPM;
        FPM.addPass(C166SFRBitfieldEarlyPass());
        FPM.addPass(C166FarPointerEarlyPass());
        MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
      });
}

namespace {
class C166PassConfig : public TargetPassConfig {
public:
  C166PassConfig(C166TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  void addIRPasses() override {
    TargetPassConfig::addIRPasses();
    addPass(createC166UnsupportedFeaturesPass());
    // C166 int and size_t are 16-bit while pointers are 32-bit. Lower through
    // the target runtime before generic AtomicExpand can apply its
    // intptr_t-as-size_t and i32-as-int helper assumptions.  This must also
    // precede float memory lowering: the latter converts the helper temporaries
    // between LLVM's logical float values and MSW-first storage.
    addPass(createC166AtomicLoweringPass());
    addPass(createC166F64LoweringPass());
    addPass(createC166FloatMemoryLoweringPass());
    addPass(createC166SFRBitfieldLoweringPass());
    // Keep the generic pass as a structural safety net for any future atomic
    // IR operation which the target-local lowering does not recognize.
    addPass(createAtomicExpandLegacyPass());
    // CodeGenPrepare and the float lowering above may introduce GEPs and
    // llvm.mem* intrinsics in the far-data address space.  Lower them only
    // after every generic IR pass has run so none can be reintroduced before
    // instruction selection.
    addPass(createC166FarPointerLoweringPass());
  }

  bool addInstSelector() override {
    addPass(createC166ISelDag(getTM<C166TargetMachine>(), getOptLevel()));
    return false;
  }

  void addMachineSSAOptimization() override {
    if (getOptLevel() != CodeGenOptLevel::None) {
      addPass(createC166PostISelPass());
      addPass(createC166ArgumentLoadSinkingPass());
    }
    TargetPassConfig::addMachineSSAOptimization();
  }

  void addPreRegAlloc() override {
    if (getOptLevel() != CodeGenOptLevel::None) {
      addPass(createC166FrameAddressRematerializationPass());
      addPass(createC166PHIEdgeSplittingPass());
    }
  }

  void addPreEmitPass() override {
    if (getOptLevel() != CodeGenOptLevel::None) {
      addPass(createC166PostRAPass());
      addPass(createMachineCopyPropagationPass(/*UseCopyInstr=*/true));
    }
    addPass(createC166CallFrameExpansionPass());
    addPass(&BranchRelaxationPassID);
  }

  void addPreEmitPass2() override { addPass(createC166LongBranchOptPass()); }
};
} // namespace

TargetPassConfig *C166TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new C166PassConfig(*this, PM);
}
