//===-- C166ISelLowering.h - C166 DAG lowering interface -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_C166ISELLOWERING_H
#define LLVM_LIB_TARGET_C166_C166ISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class C166Subtarget;

class C166TargetLowering : public TargetLowering {
public:
  C166TargetLowering(const TargetMachine &TM, const C166Subtarget &STI);

  MVT getScalarShiftAmountTy(const DataLayout &, EVT) const override {
    return MVT::i16;
  }

  MVT::SimpleValueType getCmpLibcallReturnType() const override {
    return MVT::i16;
  }

  const char *getTargetNodeName(unsigned Opcode) const override;

  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override;

  ConstraintType getConstraintType(StringRef Constraint) const override;

  Register getRegisterByName(const char *RegName, LLT VT,
                             const MachineFunction &MF) const override;

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  void LowerAsmOperandForConstraint(SDValue Op, StringRef Constraint,
                                    std::vector<SDValue> &Ops,
                                    SelectionDAG &DAG) const override;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *MBB) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  bool getPostIndexedAddressParts(SDNode *N, SDNode *Op, SDValue &Base,
                                  SDValue &Offset, ISD::MemIndexedMode &AM,
                                  SelectionDAG &DAG) const override;

  bool shouldReduceLoadWidth(
      SDNode *Load, ISD::LoadExtType ExtTy, EVT NewVT,
      std::optional<unsigned> ByteOffset = std::nullopt) const override;

  unsigned getJumpTableEncoding() const override;

  MVT getJumpTableRegTy(const DataLayout &DL) const override {
    return getPointerTy(DL, DL.getDefaultGlobalsAddressSpace());
  }

private:
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerCall(CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVAARG(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVACOPY(SDValue Op, SelectionDAG &DAG) const;
  SDValue CombineSplitLeftShiftOne(SDNode *N, DAGCombinerInfo &DCI) const;
  SDValue CombineSplitRightShiftOne(SDNode *N, DAGCombinerInfo &DCI) const;
  SDValue CombineI64ConstantShift(SDNode *N, SelectionDAG &DAG) const;
  SDValue LowerI64BRCC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerI64SetCC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerI32Shift(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBRJT(SDValue Op, SelectionDAG &DAG) const;

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context, const Type *RetTy) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;
};

} // namespace llvm

#endif
