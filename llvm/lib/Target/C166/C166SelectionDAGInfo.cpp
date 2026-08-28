//===-- C166SelectionDAGInfo.cpp - C166 SelectionDAG info ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166SelectionDAGInfo.h"
#include "llvm/Analysis/LibcallLoweringInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/IR/DataLayout.h"

#define GET_SDNODE_DESC
#include "C166GenSDNodeInfo.inc"

using namespace llvm;

C166SelectionDAGInfo::C166SelectionDAGInfo()
    : SelectionDAGGenTargetInfo(C166GenSDNodeInfo) {}

C166SelectionDAGInfo::~C166SelectionDAGInfo() = default;

static SDValue
emitMemoryLibcall(SelectionDAG &DAG, const SDLoc &DL, SDValue Chain,
                  RTLIB::Libcall Libcall,
                  ArrayRef<std::pair<SDValue, Type *>> Operands) {
  const TargetLowering &TLI = DAG.getTargetLoweringInfo();
  RTLIB::LibcallImpl Implementation = DAG.getLibcalls().getLibcallImpl(Libcall);
  if (Implementation == RTLIB::Unsupported)
    return SDValue();

  TargetLowering::ArgListTy Args;
  for (const auto &[Value, Ty] : Operands)
    Args.emplace_back(Value, Ty);

  const DataLayout &Layout = DAG.getDataLayout();
  CallingConv::ID CC =
      DAG.getLibcalls().getLibcallImplCallingConv(Implementation);
  TargetLowering::CallLoweringInfo CLI(DAG);
  CLI.setDebugLoc(DL)
      .setChain(Chain)
      .setLibCallee(
          CC, Type::getVoidTy(*DAG.getContext()),
          DAG.getExternalSymbol(
              Implementation,
              TLI.getPointerTy(Layout, Layout.getProgramAddressSpace())),
          std::move(Args))
      .setDiscardResult();
  return TLI.LowerCallTo(CLI).second;
}

SDValue C166SelectionDAGInfo::EmitTargetCodeForMemcpy(
    SelectionDAG &DAG, const SDLoc &DL, SDValue Chain, SDValue Dst, SDValue Src,
    SDValue Size, Align DstAlign, Align SrcAlign, bool IsVolatile,
    bool AlwaysInline, MachinePointerInfo DstPtrInfo,
    MachinePointerInfo SrcPtrInfo) const {
  const DataLayout &Layout = DAG.getDataLayout();
  unsigned AddressSpace = Layout.getDefaultGlobalsAddressSpace();
  if (AlwaysInline || AddressSpace == 0 ||
      DstPtrInfo.getAddrSpace() != AddressSpace ||
      SrcPtrInfo.getAddrSpace() != AddressSpace)
    return SDValue();

  LLVMContext &Context = *DAG.getContext();
  Type *PointerTy = PointerType::get(Context, AddressSpace);
  Type *SizeTy = Type::getInt16Ty(Context);
  Size = DAG.getZExtOrTrunc(Size, DL, MVT::i16);
  return emitMemoryLibcall(
      DAG, DL, Chain, RTLIB::MEMCPY,
      {{Dst, PointerTy}, {Src, PointerTy}, {Size, SizeTy}});
}

SDValue C166SelectionDAGInfo::EmitTargetCodeForMemmove(
    SelectionDAG &DAG, const SDLoc &DL, SDValue Chain, SDValue Dst, SDValue Src,
    SDValue Size, Align DstAlign, Align SrcAlign, bool IsVolatile,
    MachinePointerInfo DstPtrInfo, MachinePointerInfo SrcPtrInfo) const {
  const DataLayout &Layout = DAG.getDataLayout();
  unsigned AddressSpace = Layout.getDefaultGlobalsAddressSpace();
  if (AddressSpace == 0 || DstPtrInfo.getAddrSpace() != AddressSpace ||
      SrcPtrInfo.getAddrSpace() != AddressSpace)
    return SDValue();

  LLVMContext &Context = *DAG.getContext();
  Type *PointerTy = PointerType::get(Context, AddressSpace);
  Type *SizeTy = Type::getInt16Ty(Context);
  Size = DAG.getZExtOrTrunc(Size, DL, MVT::i16);
  return emitMemoryLibcall(
      DAG, DL, Chain, RTLIB::MEMMOVE,
      {{Dst, PointerTy}, {Src, PointerTy}, {Size, SizeTy}});
}

SDValue C166SelectionDAGInfo::EmitTargetCodeForMemset(
    SelectionDAG &DAG, const SDLoc &DL, SDValue Chain, SDValue Dst,
    SDValue Value, SDValue Size, Align Alignment, bool IsVolatile,
    bool AlwaysInline, MachinePointerInfo DstPtrInfo) const {
  const DataLayout &Layout = DAG.getDataLayout();
  unsigned AddressSpace = Layout.getDefaultGlobalsAddressSpace();
  if (AlwaysInline || AddressSpace == 0 ||
      DstPtrInfo.getAddrSpace() != AddressSpace)
    return SDValue();

  LLVMContext &Context = *DAG.getContext();
  Type *PointerTy = PointerType::get(Context, AddressSpace);
  Type *IntTy = Type::getInt16Ty(Context);
  Value = DAG.getZExtOrTrunc(Value, DL, MVT::i16);
  Size = DAG.getZExtOrTrunc(Size, DL, MVT::i16);
  return emitMemoryLibcall(DAG, DL, Chain, RTLIB::MEMSET,
                           {{Dst, PointerTy}, {Value, IntTy}, {Size, IntTy}});
}
