//===-- C166Subtarget.cpp - C166 subtarget information -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166Subtarget.h"
#include "C166SelectionDAGInfo.h"
#include "llvm/Analysis/LibcallLoweringInfo.h"

#define DEBUG_TYPE "c166-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "C166GenSubtargetInfo.inc"

using namespace llvm;

void C166Subtarget::anchor() {}

C166Subtarget &C166Subtarget::initializeSubtargetDependencies(StringRef CPU,
                                                              StringRef FS) {
  if (CPU.empty())
    CPU = "c166";
  ParseSubtargetFeatures(CPU, CPU, FS);
  return *this;
}

C166Subtarget::C166Subtarget(const Triple &TT, StringRef CPU, StringRef FS,
                             const TargetMachine &TM)
    : C166GenSubtargetInfo(TT, CPU, CPU, FS),
      InstrInfo(initializeSubtargetDependencies(CPU, FS)), TLInfo(TM, *this) {
  TSInfo = std::make_unique<C166SelectionDAGInfo>();
}

C166Subtarget::~C166Subtarget() = default;

void C166Subtarget::initLibcallLoweringInfo(LibcallLoweringInfo &Info) const {
  // C166 has a 16-bit C `int`, so generic runtime selection does not assume
  // the usual `si` helpers.  LLVM's C166 ELF runtime nevertheless defines
  // these names by their fixed IR widths: i32 operands are C `long` values and
  // use the public C166 R12-R15/R4:R5 convention.
  static constexpr std::pair<RTLIB::Libcall, RTLIB::LibcallImpl> Calls[] = {
      {RTLIB::MUL_I32, RTLIB::impl___mulsi3},
      {RTLIB::SDIV_I32, RTLIB::impl___divsi3},
      {RTLIB::UDIV_I32, RTLIB::impl___udivsi3},
      {RTLIB::SREM_I32, RTLIB::impl___modsi3},
      {RTLIB::UREM_I32, RTLIB::impl___umodsi3},
      {RTLIB::SHL_I32, RTLIB::impl___ashlsi3},
      {RTLIB::SRL_I32, RTLIB::impl___lshrsi3},
      {RTLIB::SRA_I32, RTLIB::impl___ashrsi3},
      {RTLIB::CTLZ_I32, RTLIB::impl___clzsi2},
      {RTLIB::ADD_F32, RTLIB::impl___addsf3},
      {RTLIB::SUB_F32, RTLIB::impl___subsf3},
      {RTLIB::MUL_F32, RTLIB::impl___mulsf3},
      {RTLIB::DIV_F32, RTLIB::impl___divsf3},
      {RTLIB::ADD_F64, RTLIB::impl___adddf3},
      {RTLIB::SUB_F64, RTLIB::impl___subdf3},
      {RTLIB::MUL_F64, RTLIB::impl___muldf3},
      {RTLIB::DIV_F64, RTLIB::impl___divdf3},
      {RTLIB::FPTOSINT_F32_I32, RTLIB::impl___fixsfsi},
      {RTLIB::FPTOUINT_F32_I32, RTLIB::impl___fixunssfsi},
      {RTLIB::SINTTOFP_I32_F32, RTLIB::impl___floatsisf},
      {RTLIB::UINTTOFP_I32_F32, RTLIB::impl___floatunsisf},
      {RTLIB::FPTOSINT_F64_I32, RTLIB::impl___fixdfsi},
      {RTLIB::FPTOUINT_F64_I32, RTLIB::impl___fixunsdfsi},
      {RTLIB::SINTTOFP_I32_F64, RTLIB::impl___floatsidf},
      {RTLIB::UINTTOFP_I32_F64, RTLIB::impl___floatunsidf},
      {RTLIB::FPEXT_F32_F64, RTLIB::impl___extendsfdf2},
      {RTLIB::FPROUND_F64_F32, RTLIB::impl___truncdfsf2},
      {RTLIB::FCMP3_PRED_OEQ_F32, RTLIB::impl___eqsf2},
      {RTLIB::FCMP3_PRED_UNE_F32, RTLIB::impl___nesf2},
      {RTLIB::FCMP3_PRED_OGE_F32, RTLIB::impl___gesf2},
      {RTLIB::FCMP3_PRED_OLT_F32, RTLIB::impl___ltsf2},
      {RTLIB::FCMP3_PRED_OLE_F32, RTLIB::impl___lesf2},
      {RTLIB::FCMP3_PRED_OGT_F32, RTLIB::impl___gtsf2},
      {RTLIB::UO_F32, RTLIB::impl___unordsf2},
      {RTLIB::FCMP3_PRED_OEQ_F64, RTLIB::impl___eqdf2},
      {RTLIB::FCMP3_PRED_UNE_F64, RTLIB::impl___nedf2},
      {RTLIB::FCMP3_PRED_OGE_F64, RTLIB::impl___gedf2},
      {RTLIB::FCMP3_PRED_OLT_F64, RTLIB::impl___ltdf2},
      {RTLIB::FCMP3_PRED_OLE_F64, RTLIB::impl___ledf2},
      {RTLIB::FCMP3_PRED_OGT_F64, RTLIB::impl___gtdf2},
      {RTLIB::UO_F64, RTLIB::impl___unorddf2},
      // The C166 bare-metal runtime supplies the ordinary C memory entry
      // points.  Leaving these unsupported makes SelectionDAG construct a
      // nameless external callee when an optimized aggregate operation is
      // no longer profitable to inline.
      {RTLIB::MEMCPY, RTLIB::impl_memcpy},
      {RTLIB::MEMMOVE, RTLIB::impl_memmove},
      {RTLIB::MEMSET, RTLIB::impl_memset},
  };
  for (const auto &[Operation, Implementation] : Calls)
    Info.setLibcallImpl(Operation, Implementation);
}
