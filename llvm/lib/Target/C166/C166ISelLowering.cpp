//===-- C166ISelLowering.cpp - C166 DAG lowering implementation ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166ISelLowering.h"
#include "C166.h"
#include "C166MachineFunctionInfo.h"
#include "C166SelectionDAGInfo.h"
#include "C166Subtarget.h"
#include "C166TargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/IntrinsicsC166.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/C166TargetParser.h"

#include <array>

using namespace llvm;

#define GET_CALLING_CONV_IMPL
#include "C166GenCallingConv.inc"

C166TargetLowering::C166TargetLowering(const TargetMachine &TM,
                                       const C166Subtarget &STI)
    : TargetLowering(TM, STI) {
  addRegisterClass(MVT::i16, &C166::GR16RegClass);
  addRegisterClass(MVT::i32, &C166::GR32RegClass);
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(C166::R0);
  setBooleanContents(ZeroOrOneBooleanContent);
  for (MVT VT : {MVT::i16, MVT::i32}) {
    setOperationAction(ISD::UADDO, VT, Legal);
    setOperationAction(ISD::USUBO, VT, Legal);
    setOperationAction(ISD::UADDO_CARRY, VT, Legal);
    setOperationAction(ISD::USUBO_CARRY, VT, Legal);
  }
  setOperationAction(ISD::SETCC, MVT::i16, Expand);
  setOperationAction(ISD::SETCC, MVT::i32, Expand);
  setOperationAction(ISD::SETCC, MVT::i64, Custom);
  setOperationAction(ISD::BR_CC, MVT::i64, Custom);
  setOperationAction(ISD::SELECT, MVT::i16, Expand);
  setOperationAction(ISD::SELECT, MVT::i32, Expand);
  for (MVT VT : {MVT::i16, MVT::i32})
    for (unsigned Opcode : {ISD::ROTL, ISD::ROTR})
      setOperationAction(Opcode, VT, Expand);
  // Constant 32-bit rotates map efficiently to native word shifts. Variable
  // rotates still need the general shift expansion.
  setOperationAction(ISD::ROTL, MVT::i32, Custom);
  // i32 is kept legal to model C166 register pairs, but operations which
  // require more than the native 16-bit ALU use the ordinary LLVM runtime
  // helper ABI.  The helpers therefore receive long operands through the
  // public R12-R15 convention and return through R4:R5.
  setOperationAction(ISD::MUL, MVT::i32, Custom);
  for (unsigned Opcode : {ISD::SDIV, ISD::SREM})
    setOperationAction(Opcode, MVT::i32, LibCall);
  for (unsigned Opcode : {ISD::UDIV, ISD::UREM})
    setOperationAction(Opcode, MVT::i32, Custom);
  for (unsigned Opcode :
       {ISD::MULHS, ISD::MULHU, ISD::SMUL_LOHI, ISD::UMUL_LOHI})
    setOperationAction(Opcode, MVT::i32, Expand);
  for (MVT VT : {MVT::i16, MVT::i32})
    for (unsigned Opcode : {ISD::SDIVREM, ISD::UDIVREM})
      setOperationAction(Opcode, VT, Expand);
  for (unsigned Opcode : {ISD::SHL, ISD::SRL, ISD::SRA})
    setOperationAction(Opcode, MVT::i32, Custom);
  for (unsigned Opcode : {ISD::SHL_PARTS, ISD::SRL_PARTS, ISD::SRA_PARTS})
    setOperationAction(Opcode, MVT::i32, Expand);
  // PRIOR implements the zero-poison i16 operation directly.  The defined
  // form expands the zero case around it.
  setOperationAction(ISD::CTLZ, MVT::i16, Expand);
  setOperationAction(ISD::CTLZ_ZERO_POISON, MVT::i16, Legal);
  // The standard i32 bit-counting libcall returns C int (i16 on C166), which
  // SelectionDAG extends back to i32.
  setOperationAction(ISD::CTLZ, MVT::i32, Expand);
  setOperationAction(ISD::CTLZ_ZERO_POISON, MVT::i32, LibCall);
  setOperationAction(ISD::CTTZ, MVT::i32, Expand);
  setOperationAction(ISD::CTTZ_ZERO_POISON, MVT::i32, Expand);
  setOperationAction(ISD::CTPOP, MVT::i32, LibCall);
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Custom);
  setOperationAction(ISD::BR_JT, MVT::Other, Custom);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Custom);
  for (MVT VT : {MVT::i16, MVT::i32})
    setOperationAction(ISD::DYNAMIC_STACKALLOC, VT, Custom);
  setOperationAction(ISD::STACKSAVE, MVT::Other, Custom);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Custom);
  for (MVT VT : {MVT::i8, MVT::i16})
    setIndexedLoadAction(ISD::POST_INC, VT, Legal);
  setTargetDAGCombine({ISD::INTRINSIC_WO_CHAIN, ISD::OR, ISD::FSHL, ISD::FSHR,
                       ISD::SHL, ISD::SRL, ISD::SRA});
  setMinFunctionAlignment(Align(2));
  setPrefFunctionAlignment(Align(2));
  // Paged table setup breaks even later than direct Small-model access.
  setMinimumJumpTableEntries(
      C166::hasNearData(
          static_cast<const C166TargetMachine &>(TM).getC166MemoryModel())
          ? 7
          : 8);
  setMaxAtomicSizeInBitsSupported(0);
}

bool C166TargetLowering::shouldReduceLoadWidth(
    SDNode *Load, ISD::LoadExtType ExtTy, EVT NewVT,
    std::optional<unsigned> ByteOffset) const {
  const auto *Ld = cast<LoadSDNode>(Load);
  if (Ld->getMemoryVT() == MVT::i16 && NewVT == MVT::i8 && ByteOffset == 0 &&
      SDValue(Load, 0).hasOneUse()) {
    const SDNode *User = nullptr;
    for (const SDUse &Use : Load->uses()) {
      if (Use.getResNo() == 0) {
        User = Use.getUser();
        break;
      }
    }
    assert(User && "missing load value user");
    if (User->getOpcode() == ISD::AND) {
      const auto *Mask = dyn_cast<ConstantSDNode>(User->getOperand(1));
      // A byte load still needs MOVBZ when the mask does not consume every
      // loaded bit. Keep the equally cheap word load and avoid that extension.
      if (Mask && Mask->getAPIntValue().isMask() &&
          Mask->getAPIntValue().countr_one() < NewVT.getSizeInBits())
        return false;
    }
  }
  return TargetLowering::shouldReduceLoadWidth(Load, ExtTy, NewVT, ByteOffset);
}

SDValue C166TargetLowering::PerformDAGCombine(SDNode *N,
                                              DAGCombinerInfo &DCI) const {
  if (N->getOpcode() == ISD::OR || N->getOpcode() == ISD::FSHL) {
    if (SDValue Result = CombineSplitLeftShiftOne(N, DCI))
      return Result;
  }

  if (N->getOpcode() == ISD::OR || N->getOpcode() == ISD::FSHL ||
      N->getOpcode() == ISD::FSHR)
    return CombineSplitRightShiftOne(N, DCI);

  if (N->getOpcode() == ISD::SHL || N->getOpcode() == ISD::SRL ||
      N->getOpcode() == ISD::SRA)
    return CombineI64ConstantShift(N, DCI.DAG);

  if (N->getOpcode() != ISD::INTRINSIC_WO_CHAIN ||
      !isa<ConstantSDNode>(N->getOperand(0)) ||
      cast<ConstantSDNode>(N->getOperand(0))->getZExtValue() !=
          Intrinsic::c166_far_add)
    return SDValue();

  SDValue Base = N->getOperand(1);
  if (!Base->hasNUsesOfValue(2, Base.getResNo()))
    return SDValue();

  LoadSDNode *Load = nullptr;
  for (SDUse &Use : Base->uses()) {
    if (Use.getResNo() != Base.getResNo() || Use.getUser() == N)
      continue;
    Load = dyn_cast<LoadSDNode>(Use.getUser());
  }
  if (!Load)
    return SDValue();

  const auto *PointerValue =
      dyn_cast_if_present<const Value *>(Load->getPointerInfo().V);
  const auto *ByValArgument = dyn_cast_if_present<llvm::Argument>(PointerValue);
  EVT MemoryVT = Load->getMemoryVT();
  bool IsWord = MemoryVT == MVT::i16 &&
                Load->getExtensionType() == ISD::NON_EXTLOAD &&
                Load->getAlign() >= Align(2);
  bool IsByte = MemoryVT == MVT::i8 && Load->getValueType(0) == MVT::i16;
  auto *Increment = dyn_cast<ConstantSDNode>(N->getOperand(2));
  if (Load->isAtomic() || (!IsWord && !IsByte) || !Increment ||
      Increment->getZExtValue() != MemoryVT.getStoreSize() ||
      Load->getAddressSpace() != C166::FarDataAddressSpace ||
      Load->getBasePtr() != Base ||
      Load->getChain()->hasPredecessor(N) ||
      isa<const PseudoSourceValue *>(Load->getPointerInfo().V) ||
      (ByValArgument && ByValArgument->hasByValAttr()))
    return SDValue();

  SelectionDAG &DAG = DCI.DAG;
  SDValue Indexed = DAG.getIndexedLoad(
      SDValue(Load, 0), SDLoc(Load), Base,
      DAG.getConstant(MemoryVT.getStoreSize(), SDLoc(Load), MVT::i32),
      ISD::POST_INC);
  DCI.CombineTo(Load, Indexed.getValue(0), Indexed.getValue(2));
  return Indexed.getValue(1);
}

SDValue
C166TargetLowering::CombineSplitLeftShiftOne(SDNode *N,
                                             DAGCombinerInfo &DCI) const {
  if (!DCI.isBeforeLegalize() || N->getValueType(0) != MVT::i16)
    return SDValue();

  auto IsShiftBy = [](SDValue Value, unsigned Opcode, uint64_t Amount) {
    if (Value.getOpcode() != Opcode || Value->getNumOperands() < 2)
      return false;
    auto *AmountNode = dyn_cast<ConstantSDNode>(Value.getOperand(1));
    return AmountNode && AmountNode->getZExtValue() == Amount;
  };

  SDValue HighShift;
  SDValue Low;
  if (N->getOpcode() == ISD::FSHL) {
    auto *Amount = dyn_cast<ConstantSDNode>(N->getOperand(2));
    if (!Amount || Amount->getZExtValue() != 1)
      return SDValue();
    HighShift = N->getOperand(0);
    Low = N->getOperand(1);
  } else {
    SDValue Carry;
    for (unsigned I = 0; I != 2; ++I) {
      if (IsShiftBy(N->getOperand(I), ISD::SHL, 1) &&
          IsShiftBy(N->getOperand(I ^ 1), ISD::SRL, 15)) {
        HighShift = N->getOperand(I).getOperand(0);
        Carry = N->getOperand(I ^ 1);
        break;
      }
    }
    if (!HighShift)
      return SDValue();
    Low = Carry.getOperand(0);
  }

  SDNode *LowShift = nullptr;
  for (SDUse &Use : Low->uses()) {
    SDNode *User = Use.getUser();
    if (User->getNumOperands() >= 1 && User->getOperand(0) == Low &&
        IsShiftBy(SDValue(User, 0), ISD::SHL, 1)) {
      LowShift = User;
      break;
    }
  }
  if (!LowShift)
    return SDValue();

  SelectionDAG &DAG = DCI.DAG;
  SDLoc DL(N);
  SDVTList AddVTs = DAG.getVTList(MVT::i16, MVT::i1);
  SDValue LowAdd = DAG.getNode(ISD::UADDO, DL, AddVTs, Low, Low);
  SDValue HighAdd = DAG.getNode(ISD::UADDO_CARRY, DL, AddVTs, HighShift,
                                HighShift, LowAdd.getValue(1));
  DCI.CombineTo(LowShift, LowAdd.getValue(0));
  return HighAdd.getValue(0);
}

SDValue
C166TargetLowering::CombineSplitRightShiftOne(SDNode *N,
                                              DAGCombinerInfo &DCI) const {
  if (!DCI.isBeforeLegalize() || N->getValueType(0) != MVT::i16)
    return SDValue();

  auto IsShiftBy = [](SDValue Value, unsigned Opcode, uint64_t Amount) {
    if (Value.getOpcode() != Opcode || Value->getNumOperands() < 2)
      return false;
    auto *AmountNode = dyn_cast<ConstantSDNode>(Value.getOperand(1));
    return AmountNode && AmountNode->getZExtValue() == Amount;
  };

  SDValue Low;
  SDValue High;
  if (N->getOpcode() == ISD::FSHR) {
    auto *Amount = dyn_cast<ConstantSDNode>(N->getOperand(2));
    if (!Amount || Amount->getZExtValue() != 1)
      return SDValue();
    High = N->getOperand(0);
    Low = N->getOperand(1);
  } else if (N->getOpcode() == ISD::FSHL) {
    auto *Amount = dyn_cast<ConstantSDNode>(N->getOperand(2));
    if (!Amount || Amount->getZExtValue() != 15)
      return SDValue();
    High = N->getOperand(0);
    Low = N->getOperand(1);
  } else {
    for (unsigned I = 0; I != 2; ++I) {
      if (IsShiftBy(N->getOperand(I), ISD::SRL, 1) &&
          IsShiftBy(N->getOperand(I ^ 1), ISD::SHL, 15)) {
        Low = N->getOperand(I).getOperand(0);
        High = N->getOperand(I ^ 1).getOperand(0);
        break;
      }
    }
  }
  if (!Low || !High || Low == High)
    return SDValue();

  auto IsFunnelBy = [](SDNode *User, unsigned Opcode, uint64_t Amount) {
    if (User->getOpcode() != Opcode || User->getNumOperands() < 3)
      return false;
    auto *AmountNode = dyn_cast<ConstantSDNode>(User->getOperand(2));
    return AmountNode && AmountNode->getZExtValue() == Amount;
  };
  auto IsOtherCrossWordUse = [&](SDValue Value) {
    for (SDUse &Use : Value->uses()) {
      SDNode *User = Use.getUser();
      if (User == N)
        continue;
      if (IsFunnelBy(User, ISD::FSHL, 15) || IsFunnelBy(User, ISD::FSHR, 1))
        return true;
      if (!IsShiftBy(SDValue(User, 0), ISD::SRL, 1) &&
          !IsShiftBy(SDValue(User, 0), ISD::SHL, 15))
        continue;
      for (SDUse &ShiftUse : User->uses()) {
        SDNode *ShiftUser = ShiftUse.getUser();
        if (ShiftUser != N && ShiftUser->getOpcode() == ISD::OR)
          return true;
      }
    }
    return false;
  };
  if (IsOtherCrossWordUse(Low) || IsOtherCrossWordUse(High))
    return SDValue();

  SDNode *HighShift = nullptr;
  for (SDUse &Use : High->uses()) {
    SDNode *User = Use.getUser();
    if (User->getNumOperands() < 2 || User->getOperand(0) != High)
      continue;
    SDValue Value(User, 0);
    if (IsShiftBy(Value, ISD::SRL, 1) || IsShiftBy(Value, ISD::SRA, 1)) {
      HighShift = User;
      break;
    }
  }
  if (!HighShift)
    return SDValue();

  SelectionDAG &DAG = DCI.DAG;
  SDLoc DL(N);
  unsigned Opcode = HighShift->getOpcode() == ISD::SRL ? C166ISD::SRLPAIR1
                                                       : C166ISD::SRAPAIR1;
  SDValue Pair =
      DAG.getNode(Opcode, DL, DAG.getVTList(MVT::i16, MVT::i16), Low, High);
  DCI.CombineTo(HighShift, Pair.getValue(1));
  return Pair.getValue(0);
}

SDValue C166TargetLowering::CombineI64ConstantShift(SDNode *N,
                                                    SelectionDAG &DAG) const {
  if (N->getValueType(0) != MVT::i64)
    return SDValue();

  auto *AmountNode = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!AmountNode)
    return SDValue();

  uint64_t Amount = AmountNode->getZExtValue();
  // Type legalization already reduces word-aligned shifts to register-pair
  // moves.  Expanding those here would hide profitable word permutations.
  if (Amount == 0 || Amount >= 64 || Amount % 16 == 0)
    return SDValue();

  SDLoc DL(N);
  SDValue HalfIndex0 = DAG.getConstant(0, DL, MVT::i16);
  SDValue HalfIndex1 = DAG.getConstant(1, DL, MVT::i16);
  SDValue Low = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32,
                            N->getOperand(0), HalfIndex0);
  SDValue High = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32,
                             N->getOperand(0), HalfIndex1);
  unsigned WordAmount = static_cast<unsigned>(Amount / 16);
  unsigned BitAmount = static_cast<unsigned>(Amount % 16);
  if (N->getOpcode() == ISD::SHL && WordAmount == 0 && BitAmount >= 2 &&
      BitAmount <= 3) {
    SDVTList ShiftVTs = DAG.getVTList(MVT::i32, MVT::i32);
    SDValue Shift = DAG.getNode(C166ISD::SHL64, DL, ShiftVTs, Low, High,
                                DAG.getConstant(BitAmount, DL, MVT::i16));
    return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i64, Shift.getValue(0),
                       Shift.getValue(1));
  }
  if ((N->getOpcode() == ISD::SRL || N->getOpcode() == ISD::SRA) &&
      WordAmount == 0 && BitAmount == 1) {
    unsigned Opcode =
        N->getOpcode() == ISD::SRL ? C166ISD::SRL64_1 : C166ISD::SRA64_1;
    SDVTList ShiftVTs = DAG.getVTList(MVT::i32, MVT::i32);
    SDValue Shift = DAG.getNode(Opcode, DL, ShiftVTs, Low, High);
    return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i64, Shift.getValue(0),
                       Shift.getValue(1));
  }

  std::array<SDValue, 4> Input = {
      DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, Low),
      DAG.getNode(C166ISD::HIWORD, DL, MVT::i16, Low),
      DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, High),
      DAG.getNode(C166ISD::HIWORD, DL, MVT::i16, High),
  };
  std::array<SDValue, 4> Output;

  SDValue Zero = DAG.getConstant(0, DL, MVT::i16);
  auto Shift = [&](unsigned Opcode, SDValue Value, unsigned Bits) {
    return DAG.getNode(Opcode, DL, MVT::i16, Value,
                       DAG.getConstant(Bits, DL, MVT::i16));
  };

  if (N->getOpcode() == ISD::SHL && BitAmount == 1) {
    for (unsigned I = 0; I != WordAmount; ++I)
      Output[I] = Zero;

    SDVTList AddVTs = DAG.getVTList(MVT::i16, MVT::i1);
    SDValue Add = DAG.getNode(ISD::UADDO, DL, AddVTs, Input[0], Input[0]);
    Output[WordAmount] = Add;
    SDValue Carry = Add.getValue(1);
    for (unsigned I = WordAmount + 1; I != 4; ++I) {
      unsigned Source = I - WordAmount;
      Add = DAG.getNode(ISD::UADDO_CARRY, DL, AddVTs, Input[Source],
                        Input[Source], Carry);
      Output[I] = Add;
      Carry = Add.getValue(1);
    }
  } else if (N->getOpcode() == ISD::SHL) {
    for (unsigned I = 0; I != 4; ++I) {
      if (I < WordAmount) {
        Output[I] = Zero;
        continue;
      }
      unsigned Source = I - WordAmount;
      Output[I] = Shift(ISD::SHL, Input[Source], BitAmount);
      if (Source != 0) {
        SDValue Carry = Shift(ISD::SRL, Input[Source - 1], 16 - BitAmount);
        Output[I] = DAG.getNode(ISD::OR, DL, MVT::i16, Output[I], Carry);
      }
    }
  } else {
    bool Arithmetic = N->getOpcode() == ISD::SRA;
    SDValue Fill = Arithmetic ? DAG.getNode(ISD::SRA, DL, MVT::i16, Input[3],
                                            DAG.getConstant(15, DL, MVT::i16))
                              : Zero;
    for (unsigned I = 0; I != 4; ++I) {
      unsigned Source = I + WordAmount;
      if (Source >= 4) {
        Output[I] = Fill;
        continue;
      }
      if (Arithmetic && Source == 3) {
        Output[I] = Shift(ISD::SRA, Input[Source], BitAmount);
        continue;
      }
      Output[I] = Shift(ISD::SRL, Input[Source], BitAmount);
      SDValue Upper = Source == 3 ? Fill : Input[Source + 1];
      SDValue Carry = Shift(ISD::SHL, Upper, 16 - BitAmount);
      Output[I] = DAG.getNode(ISD::OR, DL, MVT::i16, Output[I], Carry);
    }
  }

  SDValue ResultLow =
      DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, Output[0], Output[1]);
  SDValue ResultHigh =
      DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, Output[2], Output[3]);
  return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i64, ResultLow, ResultHigh);
}

bool C166TargetLowering::getPostIndexedAddressParts(SDNode *N, SDNode *Op,
                                                    SDValue &Base,
                                                    SDValue &Offset,
                                                    ISD::MemIndexedMode &AM,
                                                    SelectionDAG &DAG) const {
  auto *Load = dyn_cast<LoadSDNode>(N);
  if (!Load || Load->isAtomic() ||
      (Load->getAddressSpace() != C166::NearAddressSpace &&
       Load->getAddressSpace() != C166::XNearDataAddressSpace) ||
      isa<const PseudoSourceValue *>(Load->getPointerInfo().V) ||
      Op->getOpcode() != ISD::ADD)
    return false;

  const auto *PointerValue =
      dyn_cast_if_present<const Value *>(Load->getPointerInfo().V);
  const auto *ByValArgument = dyn_cast_if_present<Argument>(PointerValue);
  if (ByValArgument && ByValArgument->hasByValAttr())
    return false;

  EVT MemoryVT = Load->getMemoryVT();
  bool IsWord = MemoryVT == MVT::i16 &&
                Load->getExtensionType() == ISD::NON_EXTLOAD &&
                Load->getAlign() >= Align(2);
  bool IsByte = MemoryVT == MVT::i8 && Load->getValueType(0) == MVT::i16;
  auto *Increment = dyn_cast<ConstantSDNode>(Op->getOperand(1));
  if ((!IsWord && !IsByte) || !Increment ||
      Increment->getZExtValue() != MemoryVT.getStoreSize() ||
      Load->getBasePtr() != Op->getOperand(0))
    return false;

  Base = Op->getOperand(0);
  Offset =
      DAG.getConstant(MemoryVT.getStoreSize(), SDLoc(N), Base.getValueType());
  AM = ISD::POST_INC;
  return true;
}

EVT C166TargetLowering::getSetCCResultType(const DataLayout &DL,
                                           LLVMContext &Context, EVT VT) const {
  assert(!VT.isVector() && "C166 vector comparisons are not supported");
  return MVT::i16;
}

TargetLowering::ConstraintType
C166TargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint == "r")
    return C_RegisterClass;
  if (Constraint == "I")
    return C_Immediate;
  return TargetLowering::getConstraintType(Constraint);
}

Register C166TargetLowering::getRegisterByName(const char *RegName, LLT,
                                               const MachineFunction &) const {
  StringRef Name(RegName);
  if (Name == "mdl")
    return C166::MDL;
  if (Name == "mdh")
    return C166::MDH;
  report_fatal_error(Twine("invalid register name '") + Name + "'");
}

std::pair<unsigned, const TargetRegisterClass *>
C166TargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                 StringRef Constraint,
                                                 MVT VT) const {
  if (Constraint == "r") {
    if (VT == MVT::i8)
      return {0U, &C166::GR8RegClass};
    if (VT == MVT::i16)
      return {0U, &C166::GR16RegClass};
    if (VT == MVT::i32)
      return {0U, &C166::GR32RegClass};
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

void C166TargetLowering::LowerAsmOperandForConstraint(SDValue Op,
                                                      StringRef Constraint,
                                                      std::vector<SDValue> &Ops,
                                                      SelectionDAG &DAG) const {
  if (Constraint == "I") {
    if (const auto *Constant = dyn_cast<ConstantSDNode>(Op)) {
      uint64_t Value = Constant->getZExtValue();
      if (Value <= 15) {
        Ops.push_back(
            DAG.getTargetConstant(Value, SDLoc(Op), Op.getValueType()));
        return;
      }
    }
    return;
  }
  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

const char *C166TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case C166ISD::CALL:
    return "C166ISD::CALL";
  case C166ISD::NEARCALL:
    return "C166ISD::NEARCALL";
  case C166ISD::TAILCALL:
    return "C166ISD::TAILCALL";
  case C166ISD::NEARTAILCALL:
    return "C166ISD::NEARTAILCALL";
  case C166ISD::RET:
    return "C166ISD::RET";
  case C166ISD::NEARRET:
    return "C166ISD::NEARRET";
  case C166ISD::INTERRUPTRET:
    return "C166ISD::INTERRUPTRET";
  case C166ISD::PUSHARG:
    return "C166ISD::PUSHARG";
  case C166ISD::ADJSP:
    return "C166ISD::ADJSP";
  case C166ISD::ALLOCSP:
    return "C166ISD::ALLOCSP";
  case C166ISD::NEARLOAD:
    return "C166ISD::NEARLOAD";
  case C166ISD::FARADD:
    return "C166ISD::FARADD";
  case C166ISD::FRAMEADDR:
    return "C166ISD::FRAMEADDR";
  case C166ISD::LOWORD:
    return "C166ISD::LOWORD";
  case C166ISD::HIWORD:
    return "C166ISD::HIWORD";
  case C166ISD::SHL64:
    return "C166ISD::SHL64";
  case C166ISD::SRL64_1:
    return "C166ISD::SRL64_1";
  case C166ISD::SRA64_1:
    return "C166ISD::SRA64_1";
  case C166ISD::SRL32_1:
    return "C166ISD::SRL32_1";
  case C166ISD::SRA32_1:
    return "C166ISD::SRA32_1";
  case C166ISD::SRLPAIR1:
    return "C166ISD::SRLPAIR1";
  case C166ISD::SRAPAIR1:
    return "C166ISD::SRAPAIR1";
  case C166ISD::SMUL16:
    return "C166ISD::SMUL16";
  case C166ISD::UMUL16:
    return "C166ISD::UMUL16";
  case C166ISD::UDIVREM32BY16:
    return "C166ISD::UDIVREM32BY16";
  default:
    return nullptr;
  }
}

SDValue C166TargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::VASTART:
    return LowerVASTART(Op, DAG);
  case ISD::VAARG:
    return LowerVAARG(Op, DAG);
  case ISD::VACOPY:
    return LowerVACOPY(Op, DAG);
  case ISD::DYNAMIC_STACKALLOC:
    return LowerDYNAMIC_STACKALLOC(Op, DAG);
  case ISD::STACKSAVE:
    return LowerSTACKSAVE(Op, DAG);
  case ISD::STACKRESTORE:
    return LowerSTACKRESTORE(Op, DAG);
  case ISD::BR_JT:
    return LowerBRJT(Op, DAG);
  case ISD::BR_CC:
    return LowerI64BRCC(Op, DAG);
  case ISD::SETCC:
    return LowerI64SetCC(Op, DAG);
  case ISD::ROTL:
    if (isa<ConstantSDNode>(Op.getOperand(1)))
      return Op;
    return expandROT(Op.getNode(), true, DAG);
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:
    return LowerI32Shift(Op, DAG);
  case ISD::MUL: {
    SDLoc DL(Op);
    auto ExtendedWord = [&](SDValue Value, unsigned Extension) {
      if (Value.getOpcode() == Extension &&
          Value.getOperand(0).getValueType() == MVT::i16)
        return Value.getOperand(0);
      if (Extension == ISD::ZERO_EXTEND && Value.getOpcode() == ISD::AND) {
        for (unsigned ConstantOperand : {0u, 1u}) {
          auto *Mask =
              dyn_cast<ConstantSDNode>(Value.getOperand(ConstantOperand));
          if (Mask && Mask->getZExtValue() == 0xffff)
            return DAG.getNode(C166ISD::LOWORD, DL, MVT::i16,
                               Value.getOperand(ConstantOperand ^ 1));
        }
      }
      if (Extension == ISD::ZERO_EXTEND &&
          DAG.computeKnownBits(Value).countMaxActiveBits() <= 16)
        return DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, Value);
      if (Extension == ISD::SIGN_EXTEND && DAG.ComputeNumSignBits(Value) >= 17)
        return DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, Value);
      return SDValue();
    };
    for (auto [Extension, Opcode] :
         {std::pair{ISD::ZERO_EXTEND, C166ISD::UMUL16},
          std::pair{ISD::SIGN_EXTEND, C166ISD::SMUL16}}) {
      SDValue Lhs = ExtendedWord(Op.getOperand(0), Extension);
      SDValue Rhs = ExtendedWord(Op.getOperand(1), Extension);
      if (Lhs && Rhs)
        return DAG.getNode(Opcode, DL, MVT::i32, Lhs, Rhs);
    }
    MakeLibCallOptions Options;
    SmallVector<SDValue, 2> Args = {Op.getOperand(0), Op.getOperand(1)};
    return makeLibCall(DAG, RTLIB::MUL_I32, MVT::i32, Args, Options, DL).first;
  }
  case ISD::UDIV:
  case ISD::UREM: {
    SDLoc DL(Op);
    SDValue Divisor = Op.getOperand(1);
    SDValue NarrowDivisor;
    if (Divisor.getOpcode() == ISD::ZERO_EXTEND &&
        Divisor.getOperand(0).getValueType() == MVT::i16) {
      NarrowDivisor = Divisor.getOperand(0);
    } else if (DAG.computeKnownBits(Divisor).countMaxActiveBits() <= 16) {
      NarrowDivisor = DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, Divisor);
    }

    if (!NarrowDivisor) {
      MakeLibCallOptions Options;
      RTLIB::Libcall Libcall =
          Op.getOpcode() == ISD::UDIV ? RTLIB::UDIV_I32 : RTLIB::UREM_I32;
      SmallVector<SDValue, 2> Args = {Op.getOperand(0), Divisor};
      return makeLibCall(DAG, Libcall, MVT::i32, Args, Options, DL).first;
    }

    SDValue Dividend = Op.getOperand(0);
    // DIVLU traps when the quotient does not fit in one word.  Divide the
    // high word first, then divide the resulting remainder and low word.  The
    // second dividend is safe because its high half is smaller than divisor.
    SDVTList DivRemVTs = DAG.getVTList(MVT::i32, MVT::i16);
    SDValue Result = DAG.getNode(C166ISD::UDIVREM32BY16, DL, DivRemVTs,
                                 Dividend, NarrowDivisor);
    if (Op.getOpcode() == ISD::UREM) {
      SDValue Zero = DAG.getConstant(0, DL, MVT::i16);
      return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, Result.getValue(1),
                         Zero);
    }
    return Result.getValue(0);
  }
  default:
    llvm_unreachable("unexpected C166 custom-lowered operation");
  }
}

unsigned C166TargetLowering::getJumpTableEncoding() const {
  // C166 tables contain 16-bit code offsets and are emitted by the target
  // AsmPrinter. Generic block-address entries would use the default data
  // pointer width, which is 32 bits in Large and Medium.
  return MachineJumpTableInfo::EK_Inline;
}

enum class I64CompareOutput { Carry, BorrowValue };

struct I64CompareResult {
  SDValue Borrow;
  bool TrueOnBorrow;
};

static I64CompareResult lowerI64RelationalCC(SDValue LHS, SDValue RHS,
                                             ISD::CondCode CC,
                                             I64CompareOutput Output,
                                             const SDLoc &DL,
                                             SelectionDAG &DAG) {
  SDValue LowIndex = DAG.getConstant(0, DL, MVT::i16);
  SDValue HighIndex = DAG.getConstant(1, DL, MVT::i16);
  SDValue LhsLow =
      DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32, LHS, LowIndex);
  SDValue LhsHigh =
      DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32, LHS, HighIndex);
  SDValue RhsLow =
      DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32, RHS, LowIndex);
  SDValue RhsHigh =
      DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32, RHS, HighIndex);

  if (CC == ISD::SETLT || CC == ISD::SETLE || CC == ISD::SETGE ||
      CC == ISD::SETGT) {
    SDValue SignBit = DAG.getConstant(UINT32_C(0x80000000), DL, MVT::i32);
    LhsHigh = DAG.getNode(ISD::XOR, DL, MVT::i32, LhsHigh, SignBit);
    RhsHigh = DAG.getNode(ISD::XOR, DL, MVT::i32, RhsHigh, SignBit);
    switch (CC) {
    case ISD::SETLT:
      CC = ISD::SETULT;
      break;
    case ISD::SETLE:
      CC = ISD::SETULE;
      break;
    case ISD::SETGE:
      CC = ISD::SETUGE;
      break;
    case ISD::SETGT:
      CC = ISD::SETUGT;
      break;
    default:
      llvm_unreachable("unexpected signed i64 condition");
    }
  }

  bool ReverseOperands = CC == ISD::SETUGT || CC == ISD::SETULE;
  bool TrueOnBorrow = CC == ISD::SETULT || CC == ISD::SETUGT;
  if (ReverseOperands) {
    std::swap(LhsLow, RhsLow);
    std::swap(LhsHigh, RhsHigh);
  }

  SDVTList SubtractVTs = DAG.getVTList(MVT::i16, MVT::i32, MVT::i32);
  unsigned Opcode = Output == I64CompareOutput::BorrowValue
                        ? C166::SUB64Borrowrr
                        : C166::SUB64Carryrr;
  SDNode *Subtract = DAG.getMachineNode(Opcode, DL, SubtractVTs,
                                        {LhsLow, LhsHigh, RhsLow, RhsHigh});
  return {SDValue(Subtract, 0), TrueOnBorrow};
}

SDValue C166TargetLowering::LowerI64BRCC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Destination = Op.getOperand(4);

  SDValue Condition;
  ISD::CondCode BranchCC = ISD::SETNE;
  if (CC == ISD::SETEQ || CC == ISD::SETNE) {
    Condition = DAG.getSetCC(DL, MVT::i16, LHS, RHS, CC);
  } else {
    I64CompareResult Comparison =
        lowerI64RelationalCC(LHS, RHS, CC, I64CompareOutput::Carry, DL, DAG);
    Condition = Comparison.Borrow;
    BranchCC = Comparison.TrueOnBorrow ? ISD::SETNE : ISD::SETEQ;
  }
  return DAG.getNode(ISD::BR_CC, DL, MVT::Other, Chain,
                     DAG.getCondCode(BranchCC), Condition,
                     DAG.getConstant(0, DL, MVT::i16), Destination);
}

SDValue C166TargetLowering::LowerI64SetCC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();

  SDValue LowIndex = DAG.getConstant(0, DL, MVT::i16);
  SDValue HighIndex = DAG.getConstant(1, DL, MVT::i16);
  SDValue LhsLow =
      DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32, LHS, LowIndex);
  SDValue LhsHigh =
      DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32, LHS, HighIndex);
  SDValue RhsLow =
      DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32, RHS, LowIndex);
  SDValue RhsHigh =
      DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32, RHS, HighIndex);

  if (CC == ISD::SETEQ || CC == ISD::SETNE) {
    SDValue LowDifference = DAG.getNode(ISD::XOR, DL, MVT::i32, LhsLow, RhsLow);
    SDValue HighDifference =
        DAG.getNode(ISD::XOR, DL, MVT::i32, LhsHigh, RhsHigh);
    SDValue Difference =
        DAG.getNode(ISD::OR, DL, MVT::i32, LowDifference, HighDifference);
    return DAG.getSetCC(DL, MVT::i16, Difference,
                        DAG.getConstant(0, DL, MVT::i32), CC);
  }

  SDNode *User = Op->hasOneUse() ? Op->use_begin()->getUser() : nullptr;
  // Type legalization masks the promoted i1 before BRCOND.  Keep the borrow
  // in the carry flag when that branch is its only consumer.
  if (User && User->getOpcode() == ISD::AND && User->hasOneUse())
    User = User->use_begin()->getUser();
  bool BranchOnly = User && User->getOpcode() == ISD::BRCOND;
  I64CompareOutput Output =
      BranchOnly ? I64CompareOutput::Carry : I64CompareOutput::BorrowValue;
  I64CompareResult Comparison =
      lowerI64RelationalCC(LHS, RHS, CC, Output, DL, DAG);
  SDValue Result = DAG.getNode(ISD::AssertZext, DL, MVT::i16, Comparison.Borrow,
                               DAG.getValueType(MVT::i1));
  if (!Comparison.TrueOnBorrow)
    Result = DAG.getNode(ISD::XOR, DL, MVT::i16, Result,
                         DAG.getConstant(1, DL, MVT::i16));
  return Result;
}

SDValue C166TargetLowering::LowerBRJT(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue Table = Op.getOperand(1);
  SDValue Index = Op.getOperand(2);
  if (Index.getValueType() == MVT::i32)
    Index = DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, Index);
  else
    assert(Index.getValueType() == MVT::i16 &&
           "unexpected C166 jump-table index type");

  SDValue ByteOffset = DAG.getNode(ISD::SHL, DL, MVT::i16, Index,
                                   DAG.getConstant(1, DL, MVT::i16));
  SDValue Address;
  if (Table.getValueType() == MVT::i32)
    Address = DAG.getNode(C166ISD::FARADD, DL, MVT::i32, Table, ByteOffset);
  else {
    assert(Table.getValueType() == MVT::i16 &&
           "unexpected C166 jump-table pointer type");
    Address = DAG.getNode(ISD::ADD, DL, MVT::i16, Table, ByteOffset);
  }

  MachinePointerInfo PointerInfo =
      MachinePointerInfo::getJumpTable(DAG.getMachineFunction());
  SDValue Entry =
      DAG.getLoad(MVT::i16, DL, Chain, Address, PointerInfo, Align(2));
  return DAG.getNode(ISD::BRIND, DL, MVT::Other, Entry.getValue(1), Entry);
}

SDValue C166TargetLowering::LowerI32Shift(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  if (auto *Amount = dyn_cast<ConstantSDNode>(Op.getOperand(1))) {
    if (Amount->isZero())
      return Op.getOperand(0);
    unsigned ShiftAmount = Amount->getZExtValue();
    if (ShiftAmount == 1 &&
        (Op.getOpcode() == ISD::SRL || Op.getOpcode() == ISD::SRA))
      return DAG.getNode(Op.getOpcode() == ISD::SRL ? C166ISD::SRL32_1
                                                    : C166ISD::SRA32_1,
                         DL, MVT::i32, Op.getOperand(0));
    if (ShiftAmount >= 16 && ShiftAmount < 32) {
      SDValue Value = Op.getOperand(0);
      SDValue Low = DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, Value);
      SDValue High = DAG.getNode(C166ISD::HIWORD, DL, MVT::i16, Value);
      SDValue Zero = DAG.getConstant(0, DL, MVT::i16);
      unsigned WordAmount = ShiftAmount - 16;
      auto ShiftWord = [&](unsigned Opcode, SDValue Word) {
        return WordAmount == 0
                   ? Word
                   : DAG.getNode(Opcode, DL, MVT::i16, Word,
                                 DAG.getConstant(WordAmount, DL, MVT::i16));
      };
      switch (Op.getOpcode()) {
      case ISD::SHL:
        return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, Zero,
                           ShiftWord(ISD::SHL, Low));
      case ISD::SRL:
        return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32,
                           ShiftWord(ISD::SRL, High), Zero);
      case ISD::SRA: {
        SDValue Sign = DAG.getNode(ISD::SRA, DL, MVT::i16, High,
                                   DAG.getConstant(15, DL, MVT::i16));
        return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32,
                           ShiftWord(ISD::SRA, High), Sign);
      }
      default:
        llvm_unreachable("unexpected C166 i32 shift");
      }
    }
    // Constant shifts map directly to word shifts and avoid a call-clobbering
    // helper sequence. Variable shifts still use the general runtime helper.
    if (ShiftAmount < 32)
      return Op;
  }

  RTLIB::Libcall LC;
  switch (Op.getOpcode()) {
  case ISD::SHL:
    LC = RTLIB::SHL_I32;
    break;
  case ISD::SRL:
    LC = RTLIB::SRL_I32;
    break;
  case ISD::SRA:
    LC = RTLIB::SRA_I32;
    break;
  default:
    llvm_unreachable("unexpected C166 i32 shift");
  }

  MakeLibCallOptions Options;
  SmallVector<SDValue, 2> Args = {Op.getOperand(0), Op.getOperand(1)};
  return makeLibCall(DAG, LC, MVT::i32, Args, Options, DL).first;
}

MachineBasicBlock *
C166TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                MachineBasicBlock *MBB) const {
  auto GetSignBitCondition = [](MachineRegisterInfo &MRI, Register RHS,
                                unsigned CC) -> std::optional<unsigned> {
    MachineInstr *RHSDef = MRI.getUniqueVRegDef(RHS);
    if (!RHSDef || RHSDef->getOpcode() != C166::CONST32 ||
        !RHSDef->getOperand(1).isImm())
      return std::nullopt;

    int64_t Immediate = RHSDef->getOperand(1).getImm();
    if ((CC == C166::CC_SGT && Immediate == -1) ||
        (CC == C166::CC_SGE && Immediate == 0))
      return C166::CC_EQ;
    if ((CC == C166::CC_SLE && Immediate == -1) ||
        (CC == C166::CC_SLT && Immediate == 0))
      return C166::CC_NE;
    return std::nullopt;
  };

  auto EmitI32CompareBranch = [&](Register Lhs, Register Rhs, unsigned CC,
                                  MachineBasicBlock *TrueMBB,
                                  MachineBasicBlock *FalseMBB) {
    MachineFunction *MF = MBB->getParent();
    const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
    MachineRegisterInfo &MRI = MF->getRegInfo();
    if (std::optional<unsigned> BitCC =
            GetSignBitCondition(MF->getRegInfo(), Rhs, CC)) {
      BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::BITBR))
          .addReg(Lhs, {}, sub_hi16)
          .addImm(15)
          .addImm(*BitCC)
          .addMBB(TrueMBB);
      BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::BR)).addMBB(FalseMBB);
      return MBB;
    }
    auto CanUseTiedSubtract = [&] {
      if (!Lhs.isVirtual() || !MRI.hasOneNonDBGUse(Lhs))
        return false;

      MachineInstr *Def = MRI.getUniqueVRegDef(Lhs);
      if (!Def || Def->getOpcode() != TargetOpcode::REG_SEQUENCE)
        return true;

      for (unsigned I = 1, E = Def->getNumOperands(); I < E; I += 2) {
        const MachineOperand &Source = Def->getOperand(I);
        if (!Source.isReg() || !MRI.hasOneNonDBGUse(Source.getReg()))
          return false;
      }
      return true;
    };
    if (CC != C166::CC_EQ && CC != C166::CC_NE && CanUseTiedSubtract()) {
      Register Scratch = MRI.createVirtualRegister(&C166::GR32RegClass);
      BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::SUB32BR), Scratch)
          .addReg(Lhs)
          .addReg(Rhs)
          .addImm(CC)
          .addMBB(TrueMBB);
      BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::BR)).addMBB(FalseMBB);
      return MBB;
    }

    const BasicBlock *IRBB = MBB->getBasicBlock();
    MachineFunction::iterator InsertAt = std::next(MBB->getIterator());
    unsigned CallFrameSize = TII->getCallFrameSizeAt(MI);
    if (CallFrameSize)
      MF->getInfo<C166MachineFunctionInfo>()->setNeedsStableFramePointer();
    auto NewBlock = [&] {
      MachineBasicBlock *Block = MF->CreateMachineBasicBlock(IRBB);
      MF->insert(InsertAt, Block);
      Block->setCallFrameSize(CallFrameSize);
      return Block;
    };
    MachineBasicBlock *TrueGate = NewBlock();
    MachineBasicBlock *FalseGate = NewBlock();
    MachineBasicBlock *LowMBB = NewBlock();
    MachineBasicBlock *HighRemainderMBB = nullptr;

    struct WordRegister {
      Register Reg;
      unsigned SubReg;
    };
    const TargetRegisterInfo &TRI = *MF->getSubtarget().getRegisterInfo();
    auto GetWordRegister = [&](Register Reg, unsigned SubReg) {
      if (Reg.isPhysical() && SubReg)
        return WordRegister{TRI.getSubReg(Reg, SubReg), 0};
      return WordRegister{Reg, SubReg};
    };
    auto ResolveWord = [&](Register Pair, unsigned WordSubReg) {
      Register Value = Pair;
      while (Value.isVirtual()) {
        MachineInstr *Def = MRI.getUniqueVRegDef(Value);
        if (!Def)
          break;
        if (Def->getOpcode() == TargetOpcode::COPY &&
            Def->getOperand(1).isReg() && !Def->getOperand(1).getSubReg()) {
          Register Source = Def->getOperand(1).getReg();
          if (!Source.isVirtual())
            break;
          Value = Source;
          continue;
        }
        if (Def->getOpcode() == TargetOpcode::REG_SEQUENCE) {
          for (unsigned I = 1, E = Def->getNumOperands(); I + 1 < E; I += 2)
            if (Def->getOperand(I).isReg() && Def->getOperand(I + 1).isImm() &&
                Def->getOperand(I + 1).getImm() == WordSubReg)
              return GetWordRegister(Def->getOperand(I).getReg(),
                                     Def->getOperand(I).getSubReg());
        }
        break;
      }
      return GetWordRegister(Value, WordSubReg);
    };
    WordRegister LhsLow = ResolveWord(Lhs, sub_lo16);
    WordRegister LhsHigh = ResolveWord(Lhs, sub_hi16);
    WordRegister RhsLow = ResolveWord(Rhs, sub_lo16);
    WordRegister RhsHigh = ResolveWord(Rhs, sub_hi16);
    for (WordRegister Word : {LhsLow, LhsHigh, RhsLow, RhsHigh})
      if (Word.Reg.isVirtual())
        MRI.clearKillFlags(Word.Reg);

    TrueMBB->replacePhiUsesWith(MBB, TrueGate);
    FalseMBB->replacePhiUsesWith(MBB, FalseGate);
    while (!MBB->succ_empty())
      MBB->removeSuccessor(MBB->succ_begin());

    auto EmitBranch = [&](MachineBasicBlock *Block, unsigned SubReg,
                          unsigned WordCC, MachineBasicBlock *BranchMBB,
                          MachineBasicBlock *FallthroughMBB) {
      WordRegister LhsWord = SubReg == sub_lo16 ? LhsLow : LhsHigh;
      WordRegister RhsWord = SubReg == sub_lo16 ? RhsLow : RhsHigh;
      MachineInstrBuilder Compare =
          Block == MBB
              ? BuildMI(*Block, MI, MI.getDebugLoc(), TII->get(C166::CMPBR))
              : BuildMI(Block, MI.getDebugLoc(), TII->get(C166::CMPBR));
      Compare.addReg(LhsWord.Reg, {}, LhsWord.SubReg)
          .addReg(RhsWord.Reg, {}, RhsWord.SubReg)
          .addImm(WordCC)
          .addMBB(BranchMBB);
      if (Block == MBB)
        BuildMI(*Block, MI, MI.getDebugLoc(), TII->get(C166::BR))
            .addMBB(FallthroughMBB);
      else
        BuildMI(Block, MI.getDebugLoc(), TII->get(C166::BR))
            .addMBB(FallthroughMBB);
      Block->addSuccessor(BranchMBB);
      Block->addSuccessor(FallthroughMBB);
    };
    auto EmitGate = [&](MachineBasicBlock *Gate, MachineBasicBlock *Target) {
      BuildMI(Gate, MI.getDebugLoc(), TII->get(C166::BR)).addMBB(Target);
      Gate->addSuccessor(Target);
    };

    // Compare the high and low words independently when a tied subtract would
    // need another live register pair. CMPBR remains atomic until post-RA
    // expansion, so a spill cannot clobber the flags between the hardware CMP
    // and its JMPR.
    switch (CC) {
    case C166::CC_EQ:
      EmitBranch(MBB, sub_hi16, C166::CC_NE, FalseGate, LowMBB);
      EmitBranch(LowMBB, sub_lo16, C166::CC_EQ, TrueGate, FalseGate);
      break;
    case C166::CC_NE:
      EmitBranch(MBB, sub_hi16, C166::CC_NE, TrueGate, LowMBB);
      EmitBranch(LowMBB, sub_lo16, C166::CC_NE, TrueGate, FalseGate);
      break;
    case C166::CC_ULT:
    case C166::CC_ULE:
      HighRemainderMBB = NewBlock();
      EmitBranch(MBB, sub_hi16, C166::CC_ULT, TrueGate, HighRemainderMBB);
      EmitBranch(HighRemainderMBB, sub_hi16, C166::CC_UGT, FalseGate, LowMBB);
      EmitBranch(LowMBB, sub_lo16, CC, TrueGate, FalseGate);
      break;
    case C166::CC_UGE:
    case C166::CC_UGT:
      HighRemainderMBB = NewBlock();
      EmitBranch(MBB, sub_hi16, C166::CC_UGT, TrueGate, HighRemainderMBB);
      EmitBranch(HighRemainderMBB, sub_hi16, C166::CC_ULT, FalseGate, LowMBB);
      EmitBranch(LowMBB, sub_lo16, CC, TrueGate, FalseGate);
      break;
    case C166::CC_SLT:
    case C166::CC_SLE:
      HighRemainderMBB = NewBlock();
      EmitBranch(MBB, sub_hi16, C166::CC_SLT, TrueGate, HighRemainderMBB);
      EmitBranch(HighRemainderMBB, sub_hi16, C166::CC_SGT, FalseGate, LowMBB);
      EmitBranch(LowMBB, sub_lo16,
                 CC == C166::CC_SLT ? C166::CC_ULT : C166::CC_ULE, TrueGate,
                 FalseGate);
      break;
    case C166::CC_SGE:
    case C166::CC_SGT:
      HighRemainderMBB = NewBlock();
      EmitBranch(MBB, sub_hi16, C166::CC_SGT, TrueGate, HighRemainderMBB);
      EmitBranch(HighRemainderMBB, sub_hi16, C166::CC_SLT, FalseGate, LowMBB);
      EmitBranch(LowMBB, sub_lo16,
                 CC == C166::CC_SGE ? C166::CC_UGE : C166::CC_UGT, TrueGate,
                 FalseGate);
      break;
    default:
      llvm_unreachable("unsupported C166 i32 comparison condition");
    }
    EmitGate(TrueGate, TrueMBB);
    EmitGate(FalseGate, FalseMBB);
    return TrueGate;
  };

  if (MI.getOpcode() == C166::CMP32BR) {
    MachineFunction *MF = MBB->getParent();
    MachineRegisterInfo &MRI = MF->getRegInfo();
    Register LHS = MI.getOperand(0).getReg();
    Register RHS = MI.getOperand(1).getReg();
    unsigned CC = MI.getOperand(2).getImm();
    MachineInstr *RHSDef = MRI.getUniqueVRegDef(RHS);
    if (RHSDef && RHSDef->getOpcode() == C166::CONST32 &&
        RHSDef->getOperand(1).getImm() == 0 &&
        (CC == C166::CC_EQ || CC == C166::CC_NE)) {
      const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
      BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::TEST32BR))
          .addReg(LHS, {}, sub_lo16)
          .addReg(LHS, {}, sub_hi16)
          .addImm(CC)
          .addMBB(MI.getOperand(3).getMBB());
      MI.eraseFromParent();
      if (MRI.use_nodbg_empty(RHS))
        RHSDef->eraseFromParent();
      return MBB;
    }
    MachineBasicBlock *TrueMBB = MI.getOperand(3).getMBB();
    MachineBasicBlock *FalseMBB = nullptr;
    for (MachineBasicBlock *Successor : MBB->successors())
      if (Successor != TrueMBB) {
        FalseMBB = Successor;
        break;
      }
    if (!FalseMBB)
      report_fatal_error("C166 i32 conditional branch has no false successor");
    auto Following = std::next(MachineBasicBlock::iterator(MI));
    if (Following != MBB->end() && Following->getOpcode() == C166::BR)
      Following->eraseFromParent();
    EmitI32CompareBranch(LHS, RHS, CC, TrueMBB, FalseMBB);
    MI.eraseFromParent();
    if (RHSDef && MRI.use_nodbg_empty(RHS))
      RHSDef->eraseFromParent();
    return MBB;
  }

  if (MI.getOpcode() != C166::SELECT16 &&
      MI.getOpcode() != C166::SELECT16_32CMP &&
      MI.getOpcode() != C166::SELECT32_16CMP &&
      MI.getOpcode() != C166::SELECT32_32CMP)
    llvm_unreachable("unexpected C166 custom inserter instruction");

  MachineFunction *MF = MBB->getParent();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  MachineRegisterInfo &MRI = MF->getRegInfo();

  auto GetImmediate = [&](Register Reg) -> std::optional<int64_t> {
    if (!Reg.isVirtual())
      return std::nullopt;
    MachineInstr *Def = MRI.getUniqueVRegDef(Reg);
    if (!Def ||
        (Def->getOpcode() != C166::MOVri4 && Def->getOpcode() != C166::MOVri16))
      return std::nullopt;
    return Def->getOperand(1).getImm();
  };
  auto GetTrueBooleanValue = [&](Register TrueValue,
                                 Register FalseValue) -> std::optional<bool> {
    std::optional<int64_t> TrueImmediate = GetImmediate(TrueValue);
    std::optional<int64_t> FalseImmediate = GetImmediate(FalseValue);
    if (!TrueImmediate || !FalseImmediate ||
        ((*TrueImmediate != 0 || *FalseImmediate != 1) &&
         (*TrueImmediate != 1 || *FalseImmediate != 0)))
      return std::nullopt;
    return *TrueImmediate == 1;
  };
  auto EraseDeadDefs = [&](ArrayRef<Register> Registers) {
    for (Register Reg : Registers) {
      if (!Reg.isVirtual() || !MRI.use_nodbg_empty(Reg))
        continue;
      if (MachineInstr *Def = MRI.getUniqueVRegDef(Reg))
        Def->eraseFromParent();
    }
  };

  if (MI.getOpcode() == C166::SELECT16) {
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();
    Register TrueValue = MI.getOperand(3).getReg();
    Register FalseValue = MI.getOperand(4).getReg();
    unsigned CC = MI.getOperand(5).getImm();

    std::optional<bool> TrueIsOne = GetTrueBooleanValue(TrueValue, FalseValue);
    if (TrueIsOne && (CC == C166::CC_ULT || CC == C166::CC_UGE)) {
      std::optional<int64_t> RHSImmediate = GetImmediate(RHS);
      if (RHSImmediate)
        BuildMI(
            *MBB, MI, MI.getDebugLoc(),
            TII->get(isUInt<3>(*RHSImmediate) ? C166::CMPri3 : C166::CMPri16))
            .addReg(LHS)
            .addImm(*RHSImmediate);
      else
        BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::CMPrr))
            .addReg(LHS)
            .addReg(RHS);

      Register Result = MI.getOperand(0).getReg();
      Register Initial = MRI.createVirtualRegister(&C166::GR16RegClass);
      bool ResultIsCarry = (CC == C166::CC_ULT) == *TrueIsOne;
      // CMP sets C for unsigned lower. MOV preserves C, so ADDC materializes
      // C while SUBC materializes its inverse.
      BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::MOVri4), Initial)
          .addImm(ResultIsCarry ? 0 : 1);
      BuildMI(*MBB, MI, MI.getDebugLoc(),
              TII->get(ResultIsCarry ? C166::ADDCri3 : C166::SUBCri3), Result)
          .addReg(Initial)
          .addImm(0);

      MI.eraseFromParent();
      EraseDeadDefs({RHS, TrueValue, FalseValue});
      return MBB;
    }
  }

  const BasicBlock *IRBB = MBB->getBasicBlock();
  Register TrueValue = MI.getOperand(3).getReg();
  Register FalseValue = MI.getOperand(4).getReg();
  unsigned SelectCC = MI.getOperand(5).getImm();
  Register CompareLHS = MI.getOperand(1).getReg();
  MachineInstr *CompareLHSDef =
      CompareLHS.isVirtual() ? MRI.getUniqueVRegDef(CompareLHS) : nullptr;
  bool CompareLHSIsPhysicalCopy =
      CompareLHSDef && CompareLHSDef->isCopy() &&
      CompareLHSDef->getOperand(1).isReg() &&
      CompareLHSDef->getOperand(1).getReg().isPhysical();
  // Keep a value arriving in a fixed register on the direct edge and put the
  // immediate replacement on the fallthrough edge.  This lets PHI coalescing
  // retain the fixed register instead of copying the value through a
  // temporary.  Restrict this to physical-register copies: reversing a select
  // around a computed value can lengthen its live range and inhibit unrelated
  // folds.
  if (MI.getOpcode() == C166::SELECT16 &&
      (SelectCC == C166::CC_EQ || SelectCC == C166::CC_NE) &&
      FalseValue == CompareLHS && GetImmediate(TrueValue) &&
      CompareLHSIsPhysicalCopy) {
    std::swap(TrueValue, FalseValue);
    SelectCC = SelectCC == C166::CC_EQ ? C166::CC_NE : C166::CC_EQ;
  }
  MachineFunction::iterator InsertAt = std::next(MBB->getIterator());
  MachineBasicBlock *FalseMBB = MF->CreateMachineBasicBlock(IRBB);
  MachineBasicBlock *SinkMBB = MF->CreateMachineBasicBlock(IRBB);
  MF->insert(InsertAt, FalseMBB);
  MF->insert(InsertAt, SinkMBB);

  // A select may be scheduled while an outgoing caller-owned stack area is
  // live.  Preserve the call-frame displacement on the blocks introduced by
  // the custom inserter so frame-index elimination and the machine verifier
  // see the same R0 adjustment along every path.
  unsigned CallFrameSize = TII->getCallFrameSizeAt(MI);
  if (CallFrameSize)
    MF->getInfo<C166MachineFunctionInfo>()->setNeedsStableFramePointer();
  FalseMBB->setCallFrameSize(CallFrameSize);
  SinkMBB->setCallFrameSize(CallFrameSize);

  SinkMBB->splice(SinkMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(MBB);

  MBB->addSuccessor(FalseMBB);
  MBB->addSuccessor(SinkMBB);
  FalseMBB->addSuccessor(SinkMBB);

  MachineBasicBlock *TrueValueMBB = MBB;
  MachineInstr *DeadCompareLHS = nullptr;
  MachineInstr *DeadCompareRHS = nullptr;
  if (MI.getOpcode() == C166::SELECT16 ||
      MI.getOpcode() == C166::SELECT32_16CMP) {
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();
    unsigned CC = SelectCC;
    MachineInstr *RHSDef = MRI.getUniqueVRegDef(RHS);
    MachineInstr *LHSDef = MRI.getUniqueVRegDef(LHS);
    bool IsZero = RHSDef &&
                  (RHSDef->getOpcode() == C166::MOVri4 ||
                   RHSDef->getOpcode() == C166::MOVri16) &&
                  RHSDef->getOperand(1).getImm() == 0;
    if (IsZero && (CC == C166::CC_EQ || CC == C166::CC_NE) && LHSDef &&
        LHSDef->getOpcode() == C166::ORrr) {
      BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::TEST32BR))
          .addReg(LHSDef->getOperand(1).getReg())
          .addReg(LHSDef->getOperand(2).getReg())
          .addImm(CC)
          .addMBB(SinkMBB);
      DeadCompareLHS = LHSDef;
      DeadCompareRHS = RHSDef;
    } else if (RHSDef && (RHSDef->getOpcode() == C166::MOVri4 ||
                          RHSDef->getOpcode() == C166::MOVri16)) {
      BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::CMPBRi))
          .addReg(LHS)
          .addImm(RHSDef->getOperand(1).getImm())
          .addImm(CC)
          .addMBB(SinkMBB);
      DeadCompareRHS = RHSDef;
    } else {
      BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::CMPBR))
          .addReg(LHS)
          .addReg(RHS)
          .addImm(CC)
          .addMBB(SinkMBB);
    }
  } else {
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();
    unsigned CC = SelectCC;
    MachineInstr *RHSDef = MRI.getUniqueVRegDef(RHS);
    if (RHSDef && RHSDef->getOpcode() == C166::CONST32 &&
        RHSDef->getOperand(1).getImm() == 0 &&
        (CC == C166::CC_EQ || CC == C166::CC_NE)) {
      BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::TEST32BR))
          .addReg(LHS, {}, sub_lo16)
          .addReg(LHS, {}, sub_hi16)
          .addImm(CC)
          .addMBB(SinkMBB);
      DeadCompareRHS = RHSDef;
    } else {
      if (GetSignBitCondition(MRI, RHS, CC))
        DeadCompareRHS = RHSDef;
      TrueValueMBB = EmitI32CompareBranch(LHS, RHS, CC, SinkMBB, FalseMBB);
    }
  }

  BuildMI(*SinkMBB, SinkMBB->begin(), MI.getDebugLoc(),
          TII->get(TargetOpcode::PHI), MI.getOperand(0).getReg())
      .addReg(TrueValue)
      .addMBB(TrueValueMBB)
      .addReg(FalseValue)
      .addMBB(FalseMBB);

  MI.eraseFromParent();
  for (MachineInstr *DeadDef : {DeadCompareLHS, DeadCompareRHS})
    if (DeadDef && MRI.use_nodbg_empty(DeadDef->getOperand(0).getReg()))
      DeadDef->eraseFromParent();
  return SinkMBB;
}

static MCRegister getArgReg(MVT LocVT, unsigned FirstWord) {
  static constexpr MCPhysReg WordRegs[] = {C166::R12, C166::R13, C166::R14,
                                           C166::R15};
  static constexpr MCPhysReg PairRegs[] = {C166::R13R12, C166::R14R13,
                                           C166::R15R14};
  if (LocVT == MVT::i32)
    return PairRegs[FirstWord];
  return WordRegs[FirstWord];
}

static SDValue swapI32Words(SelectionDAG &DAG, const SDLoc &DL, SDValue Value) {
  SDValue Low = DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, Value);
  SDValue High = DAG.getNode(C166ISD::HIWORD, DL, MVT::i16, Value);
  return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, High, Low);
}

static unsigned getCalleeCodeBank(const TargetLowering::CallLoweringInfo &CLI) {
  if (CLI.CB) {
    const Value *Callee = CLI.CB->getCalledOperand();
    if (const auto *PointerTy = dyn_cast<PointerType>(Callee->getType()))
      return C166::getCodeBank(PointerTy->getAddressSpace());
  }
  if (const auto *GA = dyn_cast<GlobalAddressSDNode>(CLI.Callee))
    return C166::getCodeBank(GA->getGlobal()->getAddressSpace());
  return 0;
}

static bool hasPublicF64ResultLayout(SDValue Callee) {
  const auto *Symbol = dyn_cast<ExternalSymbolSDNode>(Callee);
  // The hand-written extension helper writes the public MSW-first result
  // block directly.  Generic softened calls return the internal i64 carrier
  // layout and must not be reordered here.
  return Symbol && StringRef(Symbol->getSymbol()) == "__extendsfdf2";
}

static std::pair<SDValue, int64_t> decomposeC166Address(SDValue Address) {
  int64_t Offset = 0;
  while (true) {
    if (Address.getOpcode() == ISD::ADD) {
      auto *Amount = dyn_cast<ConstantSDNode>(Address.getOperand(1));
      if (!Amount)
        break;
      Offset += Amount->getSExtValue();
      Address = Address.getOperand(0);
      continue;
    }
    if (Address.getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
        isa<ConstantSDNode>(Address.getOperand(0)) &&
        cast<ConstantSDNode>(Address.getOperand(0))->getZExtValue() ==
            Intrinsic::c166_far_add) {
      auto *Amount = dyn_cast<ConstantSDNode>(Address.getOperand(2));
      if (!Amount)
        break;
      Offset += Amount->getSExtValue();
      Address = Address.getOperand(1);
      continue;
    }
    break;
  }
  return {Address, Offset};
}

static SDValue findStoredByValWord(SDValue Chain, SDValue ObjectAddress,
                                   unsigned Offset) {
  auto [ObjectBase, ObjectOffset] = decomposeC166Address(ObjectAddress);
  while (Chain) {
    if (auto *Store = dyn_cast<StoreSDNode>(Chain)) {
      if (!Store->isSimple())
        return {};
      auto [StoreBase, StoreOffset] = decomposeC166Address(Store->getBasePtr());
      if (StoreBase != ObjectBase)
        return {};
      StoreOffset -= ObjectOffset;
      uint64_t StoreBytes = Store->getMemoryVT().getStoreSize().getFixedValue();
      bool Overlaps = StoreOffset < int64_t(Offset + 2) &&
                      int64_t(StoreOffset + StoreBytes) > Offset;
      if (Overlaps) {
        if (StoreOffset == Offset && StoreBytes == 2 &&
            !Store->isTruncatingStore() &&
            Store->getValue().getValueType() == MVT::i16)
          return Store->getValue();
        return {};
      }
      Chain = Store->getChain();
      continue;
    }
    if (auto *Load = dyn_cast<LoadSDNode>(Chain)) {
      Chain = Load->getChain();
      continue;
    }
    return {};
  }
  return {};
}

SDValue C166TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  if (CallConv != CallingConv::C && CallConv != CallingConv::Fast &&
      CallConv != CallingConv::C166_StackParm &&
      CallConv != CallingConv::C166_Interrupt)
    report_fatal_error("unsupported C166 calling convention");
  if (CallConv == CallingConv::C166_Interrupt && (IsVarArg || !Ins.empty()))
    report_fatal_error(
        "C166 interrupt handler must have void (void) signature");

  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  unsigned NextWord = 0;
  // Every banked function has a hidden word at [R0]. __banksw
  // consumes it when switching banks, and even a same-bank caller reserves
  // it so the function's public stack-argument layout does not depend on the
  // dynamic call path.
  unsigned StackOffset =
      C166::getCodeBank(MF.getFunction().getAddressSpace()) ? 2 : 0;
  bool UsedStack = CallConv == CallingConv::C166_StackParm;
  bool HasSRet = false;

  auto LoadStackWord = [&](unsigned Offset, SDValue LoadChain) {
    int FI = MFI.CreateFixedObject(2, Offset, true);
    const DataLayout &Layout = DAG.getDataLayout();
    SDValue FrameIndex = DAG.getFrameIndex(
        FI, getPointerTy(Layout, Layout.getAllocaAddrSpace()));
    return DAG.getLoad(MVT::i16, DL, LoadChain, FrameIndex,
                       MachinePointerInfo::getFixedStack(MF, FI));
  };

  for (const ISD::InputArg &Arg : Ins) {
    if (Arg.Flags.isSRet()) {
      assert(!HasSRet && InVals.empty() && "sret must be the first argument");
      HasSRet = true;
      continue;
    }

    if (Arg.Flags.isByVal()) {
      UsedStack = true;
      unsigned Size = alignTo(Arg.Flags.getByValSize(), 2u);
      int PublicFI = MFI.CreateFixedObject(Size, StackOffset, true);
      const DataLayout &Layout = DAG.getDataLayout();
      SDValue PublicAddress = DAG.getFrameIndex(
          PublicFI, getPointerTy(Layout, Layout.getAllocaAddrSpace()));
      InVals.push_back(PublicAddress);
      StackOffset += Size;
      continue;
    }

    if (Arg.OrigTy && Arg.OrigTy->isDoubleTy()) {
      // A softened binary64 argument is split into pieces in
      // least-significant-first order.  Its public representation is the
      // reverse: stack-only, with the most-significant word first.
      UsedStack = true;
      unsigned ArgumentBase = StackOffset - Arg.PartOffset;
      SDValue Value;
      if (Arg.VT == MVT::i16) {
        Value = LoadStackWord(ArgumentBase + 6 - Arg.PartOffset, Chain);
      } else if (Arg.VT == MVT::i32) {
        SDValue Low = LoadStackWord(ArgumentBase + 6 - Arg.PartOffset, Chain);
        SDValue High =
            LoadStackWord(ArgumentBase + 4 - Arg.PartOffset, Low.getValue(1));
        Value = DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, Low, High);
      } else {
        report_fatal_error("unsupported C166 softened double part");
      }
      StackOffset += Arg.VT.getStoreSize().getFixedValue();
      InVals.push_back(Value);
      continue;
    }

    // Legalization splits i64 into two i32 parts.  Apply the register stop
    // rule to the original four-word argument, not independently to each
    // legalized part.
    if (!UsedStack && Arg.PartOffset == 0 && Arg.OrigTy &&
        Arg.OrigTy->isIntegerTy(64) && NextWord != 0)
      UsedStack = true;

    bool IsFloat32 = Arg.ArgVT == MVT::f32;
    MVT ValVT = Arg.VT;
    MVT LocVT = ValVT;
    CCValAssign::LocInfo LocInfo = CCValAssign::Full;
    if (ValVT == MVT::i8) {
      LocVT = MVT::i16;
      LocInfo = Arg.Flags.isSExt()   ? CCValAssign::SExt
                : Arg.Flags.isZExt() ? CCValAssign::ZExt
                                     : CCValAssign::AExt;
    }

    unsigned Words = LocVT == MVT::i32 ? 2 : 1;
    if (IsFloat32 || UsedStack || NextWord + Words > 4)
      UsedStack = true;

    SDValue Value;
    if (UsedStack) {
      if (LocVT == MVT::i16) {
        Value = LoadStackWord(StackOffset, Chain);
      } else if (LocVT == MVT::i32) {
        SDValue First = LoadStackWord(StackOffset, Chain);
        SDValue Second = LoadStackWord(StackOffset + 2, First.getValue(1));
        SDValue Low = IsFloat32 ? Second : First;
        SDValue High = IsFloat32 ? First : Second;
        Value = DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, Low, High);
      } else {
        report_fatal_error("unsupported C166 stack argument type");
      }
      StackOffset += Words * 2;
    } else {
      MCRegister PhysReg = getArgReg(LocVT, NextWord);
      NextWord += Words;
      const TargetRegisterClass *RC = getRegClassFor(LocVT);
      Register VReg = MRI.createVirtualRegister(RC);
      MRI.addLiveIn(PhysReg, VReg);
      Value = DAG.getCopyFromReg(Chain, DL, VReg, LocVT);
    }

    if (LocInfo == CCValAssign::SExt)
      Value = DAG.getNode(ISD::AssertSext, DL, LocVT, Value,
                          DAG.getValueType(ValVT));
    else if (LocInfo == CCValAssign::ZExt)
      Value = DAG.getNode(ISD::AssertZext, DL, LocVT, Value,
                          DAG.getValueType(ValVT));
    if (LocVT != ValVT)
      Value = DAG.getNode(ISD::TRUNCATE, DL, ValVT, Value);
    InVals.push_back(Value);
  }

  if (IsVarArg) {
    // Every unnamed argument is placed on the user stack. The
    // first one starts immediately after any fixed arguments which were also
    // forced to the stack by the normal register-allocation stop rule.
    MachineFrameInfo &MFI = MF.getFrameInfo();
    int FI = MFI.CreateFixedObject(2, StackOffset, true);
    MF.getInfo<C166MachineFunctionInfo>()->setVarArgsFrameIndex(FI);
  }

  if (HasSRet) {
    Type *RetTy = MF.getFunction().getParamStructRetType(0);
    // SelectionDAG can demote an unlowerable IR return without rewriting the
    // Function signature.  Then the original return type, rather than an sret
    // parameter attribute, describes the caller-owned result block.
    if (!RetTy && !MF.getFunction().getReturnType()->isVoidTy())
      RetTy = MF.getFunction().getReturnType();
    if (!RetTy)
      report_fatal_error("missing C166 sret type");
    unsigned RetSize = alignTo(
        DAG.getDataLayout().getTypeAllocSize(RetTy).getFixedValue(), 2u);
    MachineFrameInfo &MFI = MF.getFrameInfo();
    int PublicFI = MFI.CreateFixedObject(RetSize, StackOffset, false);
    const DataLayout &Layout = DAG.getDataLayout();
    SDValue PublicAddress = DAG.getFrameIndex(
        PublicFI, getPointerTy(Layout, Layout.getAllocaAddrSpace()));
    InVals.insert(InVals.begin(), PublicAddress);

    C166MachineFunctionInfo *FuncInfo = MF.getInfo<C166MachineFunctionInfo>();
    SDValue RawAddress =
        DAG.getNode(C166ISD::FRAMEADDR, DL, MVT::i16, PublicAddress);
    Register AddressReg = MRI.createVirtualRegister(&C166::GR16RegClass);
    FuncInfo->setSRetAddressReg(AddressReg);
    Chain = DAG.getCopyToReg(Chain, DL, AddressReg, RawAddress);
  }

  return Chain;
}

SDValue C166TargetLowering::LowerCall(CallLoweringInfo &CLI,
                                      SmallVectorImpl<SDValue> &InVals) const {
  if (CLI.CallConv == CallingConv::C166_Interrupt)
    report_fatal_error("C166 interrupt handler cannot be called directly");
  if (CLI.CallConv != CallingConv::C && CLI.CallConv != CallingConv::Fast &&
      CLI.CallConv != CallingConv::C166_StackParm)
    report_fatal_error("unsupported C166 calling convention");
  const bool RequestedTailCall = CLI.IsTailCall;
  CLI.IsTailCall = false;

  SelectionDAG &DAG = CLI.DAG;
  const SDLoc &DL = CLI.DL;
  MachineFunction &MF = DAG.getMachineFunction();
  SDValue Chain = CLI.Chain;

  const unsigned CallerBank =
      C166::getCodeBank(MF.getFunction().getAddressSpace());
  const unsigned CalleeBank = getCalleeCodeBank(CLI);
  const bool IsBankedCall = CalleeBank != 0;
  const bool NeedsBankSwitch = IsBankedCall && CalleeBank != CallerBank;
  const unsigned BankSlotBytes = IsBankedCall ? 2 : 0;

  SmallVector<std::pair<MCRegister, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 4> StackWords;
  SDValue SRetDestination;
  unsigned SRetBytes = 0;
  unsigned SRetObjectBytes = 0;
  Align SRetAlign(2);
  bool IsDoubleSRet = false;
  bool CanForwardStackTail = RequestedTailCall && !CLI.IsVarArg;
  unsigned ForwardedStackBytes = 0;
  SDValue Glue;
  unsigned NextWord = 0;
  bool UsedStack = CLI.CallConv == CallingConv::C166_StackParm;
  unsigned FixedArgCount = 0;
  if (CLI.IsVarArg) {
    if (!CLI.CB)
      report_fatal_error("C166 variadic call requires call-site type info");
    FixedArgCount = CLI.CB->getFunctionType()->getNumParams();
    // SelectionDAG may prepend a lowering-only sret argument for a return type
    // that is still non-void at the IR call site. OrigArgIndex includes that
    // synthetic argument, while FunctionType::getNumParams() does not.
    if (!CLI.Outs.empty() && CLI.Outs.front().Flags.isSRet() &&
        !CLI.CB->getType()->isVoidTy())
      ++FixedArgCount;
  }
  for (unsigned I = 0; I != CLI.Outs.size(); ++I) {
    if (CLI.Outs[I].Flags.isSRet()) {
      assert(I == 0 && !SRetDestination && "sret must be the first argument");
      SRetDestination = CLI.OutVals[I];
      SRetAlign = CLI.Outs[I].Flags.getNonZeroOrigAlign();
      if (auto *FI = dyn_cast<FrameIndexSDNode>(SRetDestination))
        SRetAlign = MF.getFrameInfo().getObjectAlign(FI->getIndex());
      if (CLI.CB) {
        Type *RetTy = CLI.CB->getParamStructRetType(I);
        // Generic call-result demotion inserts the lowering-only sret
        // argument after IR construction, so the original CallBase has no
        // corresponding parameter attribute.  Its result type remains the
        // authoritative result-block type.
        if (!RetTy && !CLI.CB->getType()->isVoidTy())
          RetTy = CLI.CB->getType();
        if (!RetTy)
          report_fatal_error("missing C166 call-site sret type");
        SRetAlign = DAG.getDataLayout().getABITypeAlign(RetTy);
        SRetObjectBytes =
            DAG.getDataLayout().getTypeAllocSize(RetTy).getFixedValue();
        SRetBytes = alignTo(SRetObjectBytes, 2u);
        // Ordinary C sret destinations already use the public physical object
        // representation produced by C166FloatMemoryLowering.  Only synthetic
        // softened libcalls need a conversion back to LLVM's internal order.
        IsDoubleSRet = false;
      } else {
        // Softened f64 libcalls are synthesized after IR construction and
        // therefore have no CallBase from which to recover the demoted return
        // type.  LowerCallTo has already created an exactly sized result slot;
        // use that object as the authoritative size instead.
        auto *FI = dyn_cast<FrameIndexSDNode>(SRetDestination);
        if (!FI)
          report_fatal_error("C166 synthetic sret requires a frame index");
        SRetBytes =
            alignTo(MF.getFrameInfo().getObjectSize(FI->getIndex()), 2u);
        SRetObjectBytes = SRetBytes;
        IsDoubleSRet = hasPublicF64ResultLayout(CLI.Callee);
      }
      continue;
    }

    // Named arguments use the ordinary R12-R15 allocation. In the C ABI, the
    // ellipsis is a hard stop: every unnamed argument is passed by
    // value on the user stack, even when argument registers remain free.
    if (CLI.IsVarArg && CLI.Outs[I].OrigArgIndex >= FixedArgCount)
      UsedStack = true;

    if (CLI.Outs[I].Flags.isByVal()) {
      UsedStack = true;
      unsigned ObjectSize = CLI.Outs[I].Flags.getByValSize();
      unsigned SlotSize = alignTo(ObjectSize, 2u);
      Align ObjectAlign = CLI.Outs[I].Flags.getNonZeroByValAlign();
      MachinePointerInfo PointerInfo(CLI.Outs[I].Flags.getPointerAddrSpace());
      SDValue Base = CLI.OutVals[I];
      if (CanForwardStackTail) {
        auto *FI = dyn_cast<FrameIndexSDNode>(Base);
        MachineFrameInfo &MFI = MF.getFrameInfo();
        if (!FI || !MFI.isFixedObjectIndex(FI->getIndex()) ||
            MFI.getObjectOffset(FI->getIndex()) != ForwardedStackBytes ||
            MFI.getObjectSize(FI->getIndex()) < ObjectSize)
          CanForwardStackTail = false;
        else
          ForwardedStackBytes += SlotSize;
      }
      auto AddressAt = [&](unsigned Offset) {
        SDValue Address = Base;
        if (Offset)
          Address =
              DAG.getNode(ISD::ADD, DL, Base.getValueType(), Base,
                          DAG.getConstant(Offset, DL, Base.getValueType()));
        return Address;
      };
      auto LoadByte = [&](unsigned Offset) {
        return DAG.getExtLoad(
            ISD::ZEXTLOAD, DL, MVT::i16, Chain, AddressAt(Offset),
            PointerInfo.getWithOffset(Offset), MVT::i8, Align(1));
      };

      for (unsigned Offset = 0; Offset != SlotSize; Offset += 2) {
        SDValue Word;
        if (Offset + 2 <= ObjectSize && ObjectAlign >= Align(2)) {
          // A word-aligned object remains aligned at every even ABI slot.
          // LowerCall creates these outgoing loads after ordinary store
          // forwarding. Reuse an exact preceding word store when no memory
          // write on its chain can change that word.
          Word = findStoredByValWord(Chain, Base, Offset);
          if (!Word)
            Word = DAG.getLoad(MVT::i16, DL, Chain, AddressAt(Offset),
                               PointerInfo.getWithOffset(Offset),
                               commonAlignment(ObjectAlign, Offset));
        } else {
          // Packed aggregates may begin at an odd address, and an odd-sized
          // aggregate has no source byte corresponding to the high byte of
          // its final ABI word.  C166 word memory operations trap on odd
          // addresses, so form each such word from the bytes that actually
          // belong to the object and zero the ABI padding byte.
          Word = LoadByte(Offset);
          if (Offset + 1 < ObjectSize) {
            SDValue High = LoadByte(Offset + 1);
            High = DAG.getNode(ISD::SHL, DL, MVT::i16, High,
                               DAG.getConstant(8, DL, MVT::i16));
            Word = DAG.getNode(ISD::OR, DL, MVT::i16, Word, High);
          }
        }
        StackWords.push_back(Word);
      }
      continue;
    }

    CanForwardStackTail = false;

    if (CLI.Outs[I].OrigTy->isDoubleTy()) {
      // Type legalization exposes a softened binary64 operand as four i16
      // pieces in least-significant-word-first order.  Its runtime entry point
      // is an ordinary C function, so reconstruct the public double
      // boundary: stack-only and most-significant-word first.
      UsedStack = true;
      const unsigned ArgIndex = CLI.Outs[I].OrigArgIndex;
      SmallVector<SDValue, 4> Words;
      while (I != CLI.Outs.size() && CLI.Outs[I].OrigArgIndex == ArgIndex &&
             CLI.Outs[I].OrigTy->isDoubleTy()) {
        if (CLI.Outs[I].VT == MVT::i16) {
          Words.push_back(CLI.OutVals[I]);
        } else if (CLI.Outs[I].VT == MVT::i32) {
          Words.push_back(
              DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, CLI.OutVals[I]));
          Words.push_back(
              DAG.getNode(C166ISD::HIWORD, DL, MVT::i16, CLI.OutVals[I]));
        } else {
          report_fatal_error("unsupported C166 softened double part");
        }
        ++I;
      }
      if (Words.size() != 4)
        report_fatal_error("C166 softened double must contain four words");
      llvm::append_range(StackWords, llvm::reverse(Words));
      --I;
      continue;
    }

    // Legalization splits i64 into two i32 parts.  Apply the register stop
    // rule to the original four-word argument, not independently to each
    // legalized part.
    if (!UsedStack && CLI.Outs[I].PartOffset == 0 && CLI.Outs[I].OrigTy &&
        CLI.Outs[I].OrigTy->isIntegerTy(64) && NextWord != 0)
      UsedStack = true;

    bool IsFloat32 = CLI.Outs[I].ArgVT == MVT::f32;
    MVT ValVT = CLI.Outs[I].VT;
    MVT LocVT = ValVT;
    SDValue Value = CLI.OutVals[I];
    if (ValVT == MVT::i8) {
      LocVT = MVT::i16;
      if (CLI.Outs[I].Flags.isSExt())
        Value = DAG.getNode(ISD::SIGN_EXTEND, DL, LocVT, Value);
      else if (CLI.Outs[I].Flags.isZExt())
        Value = DAG.getNode(ISD::ZERO_EXTEND, DL, LocVT, Value);
      else
        Value = DAG.getNode(ISD::ANY_EXTEND, DL, LocVT, Value);
    }

    if (LocVT != MVT::i16 && LocVT != MVT::i32)
      report_fatal_error("unsupported C166 call argument type");
    unsigned Words = LocVT == MVT::i32 ? 2 : 1;
    if (IsFloat32 || UsedStack || NextWord + Words > 4)
      UsedStack = true;

    if (UsedStack) {
      if (LocVT == MVT::i16) {
        StackWords.push_back(Value);
      } else {
        // This is a register-pair split, not an arithmetic shift.  Keeping it
        // explicit avoids routing the high word through the i32 shift libcall.
        SDValue Low = DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, Value);
        SDValue High = DAG.getNode(C166ISD::HIWORD, DL, MVT::i16, Value);
        StackWords.push_back(IsFloat32 ? High : Low);
        StackWords.push_back(IsFloat32 ? Low : High);
      }
    } else {
      RegsToPass.emplace_back(getArgReg(LocVT, NextWord), Value);
      NextWord += Words;
    }
  }

  unsigned StackBytes = StackWords.size() * 2;
  bool ForwardStackTail = CanForwardStackTail && SRetDestination && SRetBytes &&
                          ForwardedStackBytes == StackBytes &&
                          BankSlotBytes == 0 && RegsToPass.empty();
  if (ForwardStackTail) {
    auto *FI = dyn_cast<FrameIndexSDNode>(SRetDestination);
    MachineFrameInfo &MFI = MF.getFrameInfo();
    ForwardStackTail = FI && MFI.isFixedObjectIndex(FI->getIndex()) &&
                       MFI.getObjectOffset(FI->getIndex()) == StackBytes &&
                       MFI.getObjectSize(FI->getIndex()) >= SRetObjectBytes;
  }
  unsigned CallFrameBytes =
      ForwardStackTail ? 0 : BankSlotBytes + SRetBytes + StackBytes;
  if (CallFrameBytes)
    Chain = DAG.getCALLSEQ_START(Chain, CallFrameBytes, 0, DL);

  unsigned DynamicOffset = 0;
  if (SRetBytes && !ForwardStackTail) {
    DynamicOffset = SRetBytes;
    Chain = DAG.getNode(C166ISD::ALLOCSP, DL, MVT::Other, Chain,
                        DAG.getConstant(SRetBytes, DL, MVT::i16),
                        DAG.getConstant(DynamicOffset, DL, MVT::i16));
  }

  // Build the final layout from high addresses to low addresses.  Keeping
  // each predecrement store in the call chain lets the scheduler form one
  // argument at a time instead of keeping every stack argument live at once.
  if (!ForwardStackTail)
    for (SDValue Word : llvm::reverse(StackWords)) {
      DynamicOffset += 2;
      Chain = DAG.getNode(C166ISD::PUSHARG, DL, MVT::Other, Chain,
                          DAG.getConstant(DynamicOffset, DL, MVT::i16), Word);
    }

  SDValue BankWord;
  if (NeedsBankSwitch) {
    BankWord = DAG.getConstant((CallerBank << 8) | CalleeBank, DL, MVT::i16);
    DynamicOffset += 2;
    Chain = DAG.getNode(C166ISD::PUSHARG, DL, MVT::Other, Chain,
                        DAG.getConstant(DynamicOffset, DL, MVT::i16), BankWord);
  } else if (BankSlotBytes) {
    DynamicOffset += BankSlotBytes;
    Chain = DAG.getNode(C166ISD::ALLOCSP, DL, MVT::Other, Chain,
                        DAG.getConstant(BankSlotBytes, DL, MVT::i16),
                        DAG.getConstant(DynamicOffset, DL, MVT::i16));
  }
  assert(DynamicOffset == CallFrameBytes &&
         "C166 outgoing call frame was not fully allocated");

  EVT FarCodePtrVT =
      getPointerTy(DAG.getDataLayout(), C166::HugeCodeAddressSpace);
  bool IsNearCall = CLI.Callee.getValueType() == MVT::i16;
  C166::MemoryModel MemoryModel =
      static_cast<const C166TargetMachine &>(DAG.getTarget())
          .getC166MemoryModel();
  if (!IsNearCall && C166::hasNearCode(MemoryModel)) {
    // Untyped runtime symbols and target-generated default-address-space
    // functions use Medium's near code class. Explicit huge functions and
    // 32-bit indirect function pointers retain their own class.
    if (isa<ExternalSymbolSDNode>(CLI.Callee))
      IsNearCall = true;
    else if (const auto *GA = dyn_cast<GlobalAddressSDNode>(CLI.Callee))
      IsNearCall = GA->getGlobal()->getAddressSpace() == 0;
  }
  if (MemoryModel == C166::MemoryModel::Small) {
    // Untyped runtime symbols and target-generated default-address-space
    // functions use Small's huge code class. Explicit near functions retain
    // their own class.
    if (isa<ExternalSymbolSDNode>(CLI.Callee))
      IsNearCall = false;
    else if (const auto *GA = dyn_cast<GlobalAddressSDNode>(CLI.Callee))
      if (GA->getGlobal()->getAddressSpace() == 0)
        IsNearCall = false;
  }
  bool EmitNearCall = IsNearCall;
  SDValue CalleeFirst;
  SDValue CalleeSecond;
  if (NeedsBankSwitch) {
    if (CLI.Callee.getValueType() != MVT::i32)
      report_fatal_error("C166 banked callee must be an inter-segment pointer");

    // __banksw is a non-C runtime entry point.  R4:R5 carries the target
    // segment:offset address, while RH3:RL3 carries current:target bank.  The
    // same bank word is already stored at [R0] for __banksw to restore the
    // caller's bank after the indirect call.
    RegsToPass.emplace_back(C166::R5R4, CLI.Callee);
    RegsToPass.emplace_back(C166::R3, BankWord);
    CalleeFirst = DAG.getTargetExternalSymbol("_banksw", FarCodePtrVT);
    CalleeSecond = DAG.getTargetExternalSymbol("_banksw", FarCodePtrVT);
  } else if (IsNearCall) {
    if (auto *GA = dyn_cast<GlobalAddressSDNode>(CLI.Callee))
      CalleeFirst = DAG.getTargetGlobalAddress(GA->getGlobal(), DL, MVT::i16,
                                               GA->getOffset());
    else if (auto *ES = dyn_cast<ExternalSymbolSDNode>(CLI.Callee))
      CalleeFirst = DAG.getTargetExternalSymbol(ES->getSymbol(), MVT::i16);
    else
      CalleeFirst = CLI.Callee;
  } else if (auto *GA = dyn_cast<GlobalAddressSDNode>(CLI.Callee)) {
    CalleeFirst = DAG.getTargetGlobalAddress(GA->getGlobal(), DL, FarCodePtrVT,
                                             GA->getOffset());
    CalleeSecond = DAG.getTargetGlobalAddress(GA->getGlobal(), DL, FarCodePtrVT,
                                              GA->getOffset());
  } else if (auto *ES = dyn_cast<ExternalSymbolSDNode>(CLI.Callee)) {
    CalleeFirst = DAG.getTargetExternalSymbol(ES->getSymbol(), FarCodePtrVT);
    CalleeSecond = DAG.getTargetExternalSymbol(ES->getSymbol(), FarCodePtrVT);
  } else {
    if (CLI.Callee.getValueType() != MVT::i32)
      report_fatal_error("unsupported C166 indirect callee type");

    // Indirect far and huge calls use __icall.
    // The 32-bit function pointer is passed low/high in R4:R5, while the
    // actual C arguments retain their normal R12-R15/user-stack placement.
    // Medium places this runtime trampoline in its first code segment and
    // reaches it with CALLA; Large reaches the same protocol with CALLS.
    RegsToPass.emplace_back(C166::R5R4, CLI.Callee);
    // External symbols receive the C ABI leading underscore in the asm
    // printer, so the runtime helper's IR-level spelling is `_icall` and its
    // emitted ABI symbol is exactly `__icall`.
    EmitNearCall = C166::hasNearCode(MemoryModel);
    EVT HelperVT = EmitNearCall ? EVT(MVT::i16) : FarCodePtrVT;
    CalleeFirst = DAG.getTargetExternalSymbol("_icall", HelperVT);
    if (!EmitNearCall)
      CalleeSecond = DAG.getTargetExternalSymbol("_icall", HelperVT);
  }

  for (const auto &[Reg, Value] : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg, Value, Glue);
    Glue = Chain.getValue(1);
  }

  SmallVector<SDValue, 10> Ops = {Chain, CalleeFirst};
  if (!EmitNearCall)
    Ops.push_back(CalleeSecond);
  for (const auto &[Reg, Value] : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg, Value.getValueType()));
  const uint32_t *Mask =
      MF.getSubtarget().getRegisterInfo()->getCallPreservedMask(MF,
                                                                CLI.CallConv);
  assert(Mask && "missing C166 call-preserved mask");
  Ops.push_back(DAG.getRegisterMask(Mask));
  if (Glue)
    Ops.push_back(Glue);

  const bool CallerReturnsNear =
      MF.getFunction().getAddressSpace() == C166::NearAddressSpace;
  const bool DirectCallee =
      isa<GlobalAddressSDNode, ExternalSymbolSDNode>(CLI.Callee);
  CLI.IsTailCall = RequestedTailCall && !CLI.IsVarArg && DirectCallee &&
                   CallFrameBytes == 0 &&
                   (!SRetDestination || ForwardStackTail) && !NeedsBankSwitch &&
                   !MF.getFrameInfo().hasVarSizedObjects() && CallerBank == 0 &&
                   CalleeBank == 0 &&
                   CLI.CallConv == MF.getFunction().getCallingConv() &&
                   EmitNearCall == CallerReturnsNear;
  if (!CLI.IsTailCall && CLI.CB && CLI.CB->isMustTailCall())
    report_fatal_error("C166 call is not eligible for tail-call elimination");

  if (CLI.IsTailCall) {
    MF.getFrameInfo().setHasTailCall();
    return DAG.getNode(EmitNearCall ? C166ISD::NEARTAILCALL : C166ISD::TAILCALL,
                       DL, MVT::Other, Ops);
  }

  Chain = DAG.getNode(EmitNearCall ? C166ISD::NEARCALL : C166ISD::CALL, DL,
                      DAG.getVTList(MVT::Other, MVT::Glue), Ops);
  Glue = Chain.getValue(1);

  SmallVector<CCValAssign, 4> RVLocs;
  CCState RetCCInfo(CLI.CallConv, CLI.IsVarArg, MF, RVLocs, *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(CLI.Ins, RetCC_C166);

  SDValue SRetResultAddress;
  if (SRetDestination && SRetBytes) {
    SRetResultAddress = DAG.getCopyFromReg(Chain, DL, C166::R4, MVT::i16, Glue);
    Chain = SRetResultAddress.getValue(1);
    Glue = SRetResultAddress.getValue(2);
  }

  SmallVector<SDValue, 4> ResultWords;
  if (SRetDestination && SRetBytes) {
    for (unsigned Offset = 0; Offset != SRetBytes; Offset += 2) {
      SDValue Word = DAG.getNode(
          C166ISD::NEARLOAD, DL, DAG.getVTList(MVT::i16, MVT::Other), Chain,
          SRetResultAddress, DAG.getConstant(Offset, DL, MVT::i16));
      Chain = Word.getValue(1);
      ResultWords.push_back(Word);
    }
  }

  // Calls use caller cleanup. Keep the complete outgoing
  // area allocated until caller-owned aggregate results have been read.  The
  // enclosing CALLSEQ records the live R0 displacement for frame-index
  // elimination, including spill reloads inserted between CALL and cleanup.
  if (CallFrameBytes) {
    SmallVector<SDValue, 3> AdjustOps = {
        Chain, DAG.getConstant(CallFrameBytes, DL, MVT::i16)};
    // A register result is read after cleanup and needs the call glue to keep
    // its physical definition live.  An sret address/result was already read
    // above; threading that CopyFromReg glue through the dependent result
    // loads would create a scheduler cycle, so its memory chain is sufficient.
    if (!RVLocs.empty() && Glue)
      AdjustOps.push_back(Glue);
    Chain = DAG.getNode(C166ISD::ADJSP, DL,
                        DAG.getVTList(MVT::Other, MVT::Glue), AdjustOps);
    Glue = Chain.getValue(1);
    Chain = DAG.getCALLSEQ_END(Chain, CallFrameBytes, 0, Glue, DL);
    Glue = Chain.getValue(1);
  }

  // Make a scalar return depend on caller cleanup.  Reading the return
  // register before CALLSEQ_END leaves a pure libcall's result independent of
  // the cleanup chain, allowing SelectionDAG to delete ADJSP and leave an
  // unmatched FrameSetup.  Caller cleanup does not clobber R4:R5, whereas an
  // sret block was necessarily copied above while its storage was still live.
  for (const CCValAssign &VA : RVLocs) {
    SDValue Value =
        DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), Glue);
    Chain = Value.getValue(1);
    Glue = Value.getValue(2);

    if (VA.getLocInfo() == CCValAssign::SExt)
      Value = DAG.getNode(ISD::AssertSext, DL, VA.getLocVT(), Value,
                          DAG.getValueType(VA.getValVT()));
    else if (VA.getLocInfo() == CCValAssign::ZExt)
      Value = DAG.getNode(ISD::AssertZext, DL, VA.getLocVT(), Value,
                          DAG.getValueType(VA.getValVT()));
    if (VA.getLocVT() != VA.getValVT())
      Value = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), Value);
    if (CLI.Ins[VA.getValNo()].ArgVT == MVT::f32)
      Value = swapI32Words(DAG, DL, Value);
    InVals.push_back(Value);
  }

  if (SRetDestination && SRetBytes) {
    // Result loads must precede cleanup, but their users must depend on it.
    // Otherwise forwarding the stores below into a pure libcall's users can
    // discard CALLSEQ_END. Virtual copies carry that dependency without
    // keeping the temporary result block live after cleanup.
    for (SDValue &Word : ResultWords) {
      Register Reg = MF.getRegInfo().createVirtualRegister(&C166::GR16RegClass);
      Chain = DAG.getCopyToReg(Chain, DL, Reg, Word);
      Word = DAG.getCopyFromReg(Chain, DL, Reg, MVT::i16);
      Chain = Word.getValue(1);
    }
    for (unsigned I = 0; I != ResultWords.size(); ++I) {
      unsigned Offset = I * 2;
      unsigned DestinationOffset =
          IsDoubleSRet ? SRetBytes - 2 - Offset : Offset;
      SDValue Destination = SRetDestination;
      if (DestinationOffset)
        Destination = DAG.getNode(
            ISD::ADD, DL, SRetDestination.getValueType(), SRetDestination,
            DAG.getConstant(DestinationOffset, DL,
                            SRetDestination.getValueType()));
      if (SRetAlign >= Align(2) && DestinationOffset + 2 <= SRetObjectBytes) {
        Chain = DAG.getStore(Chain, DL, ResultWords[I], Destination,
                             MachinePointerInfo(),
                             commonAlignment(SRetAlign, DestinationOffset));
        continue;
      }

      // A packed aggregate result can have byte alignment even when its size
      // is an even number of bytes.  C166 word stores trap at odd addresses,
      // so preserve the IR sret alignment when copying the caller-owned block
      // instead of silently treating each result word as naturally aligned.
      Chain = DAG.getTruncStore(Chain, DL, ResultWords[I], Destination,
                                MachinePointerInfo(), MVT::i8, Align(1));
      if (DestinationOffset + 1 < SRetObjectBytes) {
        SDValue High = DAG.getNode(ISD::SRL, DL, MVT::i16, ResultWords[I],
                                   DAG.getConstant(8, DL, MVT::i16));
        SDValue HighDestination =
            DAG.getNode(ISD::ADD, DL, Destination.getValueType(), Destination,
                        DAG.getConstant(1, DL, Destination.getValueType()));
        Chain = DAG.getTruncStore(Chain, DL, High, HighDestination,
                                  MachinePointerInfo(), MVT::i8, Align(1));
      }
    }
    Glue = SDValue();
  }
  return Chain;
}

static std::pair<SDValue, SDValue>
getUserStackAddress(SelectionDAG &DAG, const SDLoc &DL, SDValue Chain,
                    SDValue StackPointer, EVT AddressVT, unsigned AddressSpace);

SDValue C166TargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  const C166MachineFunctionInfo *FuncInfo =
      MF.getInfo<C166MachineFunctionInfo>();
  if (!FuncInfo->hasVarArgsFrameIndex())
    report_fatal_error("C166 va_start used outside a variadic function");
  int FI = FuncInfo->getVarArgsFrameIndex();

  SDLoc DL(Op);
  const DataLayout &DataLayout = DAG.getDataLayout();
  SDValue FrameAddress = DAG.getFrameIndex(
      FI, getPointerTy(DataLayout, DataLayout.getAllocaAddrSpace()));
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  if (FrameAddress.getValueType() == MVT::i16)
    return DAG.getStore(Op.getOperand(0), DL, FrameAddress, Op.getOperand(1),
                        MachinePointerInfo(SV));

  SDValue StackPointer =
      DAG.getNode(C166ISD::FRAMEADDR, DL, MVT::i16, FrameAddress);
  auto [Address, Chain] = getUserStackAddress(
      DAG, DL, Op.getOperand(0), StackPointer,
      getPointerTy(DataLayout, DataLayout.getAllocaAddrSpace()),
      DataLayout.getAllocaAddrSpace());
  return DAG.getStore(Chain, DL, Address, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

SDValue C166TargetLowering::LowerVACOPY(SDValue Op, SelectionDAG &DAG) const {
  const DataLayout &DL = DAG.getDataLayout();
  // va_list holds a data pointer, not the address-space-zero pointer used by
  // generic VACOPY expansion. These have different widths in Small.
  MVT CursorVT = getPointerTy(DL, DL.getDefaultGlobalsAddressSpace());
  const Value *Dst = cast<SrcValueSDNode>(Op.getOperand(3))->getValue();
  const Value *Src = cast<SrcValueSDNode>(Op.getOperand(4))->getValue();
  SDLoc Loc(Op);
  SDValue Cursor =
      DAG.getLoad(CursorVT, Loc, Op.getOperand(0), Op.getOperand(2),
                  MachinePointerInfo(Src), Align(2));
  return DAG.getStore(Cursor.getValue(1), Loc, Cursor, Op.getOperand(1),
                      MachinePointerInfo(Dst), Align(2));
}

static std::pair<SDValue, SDValue>
getUserStackAddress(SelectionDAG &DAG, const SDLoc &DL, SDValue Chain,
                    SDValue StackPointer, EVT AddressVT,
                    unsigned AddressSpace) {
  if (AddressVT == MVT::i16)
    return {StackPointer, Chain};

  assert(AddressVT == MVT::i32 && "unexpected C166 stack address type");
  SDValue Offset = DAG.getNode(ISD::AND, DL, MVT::i16, StackPointer,
                               DAG.getConstant(0x3fff, DL, MVT::i16));
  SDValue Page = DAG.getCopyFromReg(Chain, DL, C166::DPP1, MVT::i16);
  if (AddressSpace == C166::HugeDataAddressSpace ||
      AddressSpace == C166::SHugeDataAddressSpace) {
    SDValue PageInSegment = DAG.getNode(ISD::AND, DL, MVT::i16, Page,
                                        DAG.getConstant(3, DL, MVT::i16));
    PageInSegment = DAG.getNode(ISD::SHL, DL, MVT::i16, PageInSegment,
                                DAG.getConstant(14, DL, MVT::i16));
    SDValue SegmentOffset =
        DAG.getNode(ISD::OR, DL, MVT::i16, Offset, PageInSegment);
    SDValue Segment = DAG.getNode(ISD::SRL, DL, MVT::i16, Page,
                                  DAG.getConstant(2, DL, MVT::i16));
    SDValue Address =
        DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, SegmentOffset, Segment);
    return {Address, Page.getValue(1)};
  }

  assert(AddressSpace == C166::FarDataAddressSpace &&
         "unexpected C166 32-bit stack address space");
  SDValue Address = DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, Offset, Page);
  return {Address, Page.getValue(1)};
}

SDValue C166TargetLowering::LowerDYNAMIC_STACKALLOC(SDValue Op,
                                                    SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue Size = Op.getOperand(1);
  if (Size.getValueType() == MVT::i32)
    Size = DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, Size);
  assert(Size.getValueType() == MVT::i16 &&
         "unexpected C166 dynamic allocation size");

  SDValue StackPointer = DAG.getCopyFromReg(Chain, DL, C166::R0, MVT::i16);
  SDValue NewStackPointer =
      DAG.getNode(ISD::SUB, DL, MVT::i16, StackPointer, Size);
  uint64_t Alignment = cast<ConstantSDNode>(Op.getOperand(2))->getZExtValue();
  if (Alignment > 2)
    NewStackPointer = DAG.getNode(
        ISD::AND, DL, MVT::i16, NewStackPointer,
        DAG.getSignedConstant(-static_cast<int64_t>(Alignment), DL, MVT::i16));

  Chain =
      DAG.getCopyToReg(StackPointer.getValue(1), DL, C166::R0, NewStackPointer);
  unsigned AllocaAddressSpace = DAG.getDataLayout().getAllocaAddrSpace();
  auto [Address, AddressChain] = getUserStackAddress(
      DAG, DL, Chain, NewStackPointer, Op.getValueType(), AllocaAddressSpace);
  return DAG.getMergeValues({Address, AddressChain}, DL);
}

SDValue C166TargetLowering::LowerSTACKSAVE(SDValue Op,
                                           SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue StackPointer =
      DAG.getCopyFromReg(Op.getOperand(0), DL, C166::R0, MVT::i16);
  unsigned AllocaAddressSpace = DAG.getDataLayout().getAllocaAddrSpace();
  auto [Address, Chain] =
      getUserStackAddress(DAG, DL, StackPointer.getValue(1), StackPointer,
                          Op.getValueType(), AllocaAddressSpace);
  return DAG.getMergeValues({Address, Chain}, DL);
}

SDValue C166TargetLowering::LowerSTACKRESTORE(SDValue Op,
                                              SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue StackPointer = Op.getOperand(1);
  if (StackPointer.getValueType() == MVT::i32) {
    StackPointer = DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, StackPointer);
    StackPointer = DAG.getNode(ISD::OR, DL, MVT::i16, StackPointer,
                               DAG.getConstant(0x4000, DL, MVT::i16));
  }
  assert(StackPointer.getValueType() == MVT::i16 &&
         "unexpected C166 stack restore address");
  return DAG.getCopyToReg(Op.getOperand(0), DL, C166::R0, StackPointer);
}

SDValue C166TargetLowering::LowerVAARG(SDValue Op, SelectionDAG &DAG) const {
  SDNode *Node = Op.getNode();
  EVT VT = Node->getValueType(0);
  SDValue Chain = Node->getOperand(0);
  SDValue VAListPtr = Node->getOperand(1);
  const Value *SV = cast<SrcValueSDNode>(Node->getOperand(2))->getValue();
  SDLoc DL(Node);

  const DataLayout &DataLayout = DAG.getDataLayout();
  unsigned DataAddressSpace = DataLayout.getDefaultGlobalsAddressSpace();
  MVT PointerVT = getPointerTy(DataLayout, DataAddressSpace);
  SDValue VAList = DAG.getLoad(PointerVT, DL, Chain, VAListPtr,
                               MachinePointerInfo(SV), Align(2));
  unsigned ArgBytes = alignTo(VT.getStoreSize().getFixedValue(), 2u);
  SDValue Next;
  if (PointerVT == MVT::i16 || DataAddressSpace == C166::HugeDataAddressSpace)
    Next = DAG.getNode(ISD::ADD, DL, PointerVT, VAList,
                       DAG.getConstant(ArgBytes, DL, PointerVT));
  else
    Next = DAG.getNode(C166ISD::FARADD, DL, MVT::i32, VAList,
                       DAG.getConstant(ArgBytes, DL, MVT::i16));
  Chain = DAG.getStore(VAList.getValue(1), DL, Next, VAListPtr,
                       MachinePointerInfo(SV), Align(2));

  // Stack arguments are only word-aligned in the C166 ABI, including
  // four-byte long values and far pointers.
  return DAG.getLoad(VT, DL, Chain, VAList,
                     MachinePointerInfo(DataAddressSpace), Align(2));
}

bool C166TargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  SmallVector<CCValAssign, 4> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_C166);
}

SDValue
C166TargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                bool IsVarArg,
                                const SmallVectorImpl<ISD::OutputArg> &Outs,
                                const SmallVectorImpl<SDValue> &OutVals,
                                const SDLoc &DL, SelectionDAG &DAG) const {
  SmallVector<CCValAssign, 4> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_C166);

  SDValue Glue;
  SmallVector<SDValue, 5> RetOps(1, Chain);
  for (unsigned I = 0; I != RVLocs.size(); ++I) {
    const CCValAssign &VA = RVLocs[I];
    if (!VA.isRegLoc())
      report_fatal_error("C166 memory return is not implemented yet");

    SDValue Value = OutVals[I];
    if (Outs[I].ArgVT == MVT::f32)
      Value = swapI32Words(DAG, DL, Value);
    switch (VA.getLocInfo()) {
    case CCValAssign::Full:
      break;
    case CCValAssign::SExt:
      Value = DAG.getNode(ISD::SIGN_EXTEND, DL, VA.getLocVT(), Value);
      break;
    case CCValAssign::ZExt:
      Value = DAG.getNode(ISD::ZERO_EXTEND, DL, VA.getLocVT(), Value);
      break;
    case CCValAssign::AExt:
      Value = DAG.getNode(ISD::ANY_EXTEND, DL, VA.getLocVT(), Value);
      break;
    default:
      llvm_unreachable("unsupported C166 return location conversion");
    }

    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Value, Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  MachineFunction &MF = DAG.getMachineFunction();
  C166MachineFunctionInfo *FuncInfo = MF.getInfo<C166MachineFunctionInfo>();
  Register AddressReg = FuncInfo->getSRetAddressReg();
  if (AddressReg) {
    SDValue Address = DAG.getCopyFromReg(Chain, DL, AddressReg, MVT::i16);
    Chain = DAG.getCopyToReg(Chain, DL, C166::R4, Address, Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(C166::R4, MVT::i16));

    // The C166 ABI defines a double result as a user-stack block
    // addressed by R4. The generated double caller immediately feeds
    // the same block to __store8x through R10, so a real double return must
    // expose the address in both registers.  Aggregate returns continue to
    // use R4 alone.
    Type *RetTy = MF.getFunction().getParamStructRetType(0);
    if (!RetTy && !MF.getFunction().getReturnType()->isVoidTy())
      RetTy = MF.getFunction().getReturnType();
    if (RetTy && RetTy->isDoubleTy()) {
      Chain = DAG.getCopyToReg(Chain, DL, C166::R10, Address, Glue);
      Glue = Chain.getValue(1);
      RetOps.push_back(DAG.getRegister(C166::R10, MVT::i16));
    }
  }

  RetOps[0] = Chain;
  if (Glue)
    RetOps.push_back(Glue);
  unsigned ReturnOpcode =
      CallConv == CallingConv::C166_Interrupt ? C166ISD::INTERRUPTRET
      : MF.getFunction().getAddressSpace() == C166::NearAddressSpace
          ? C166ISD::NEARRET
          : C166ISD::RET;
  return DAG.getNode(ReturnOpcode, DL, MVT::Other, RetOps);
}
