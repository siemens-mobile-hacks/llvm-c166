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
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/C166TargetParser.h"

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
  setOperationAction(ISD::SETCC, MVT::i16, Expand);
  setOperationAction(ISD::SETCC, MVT::i32, Expand);
  setOperationAction(ISD::SELECT, MVT::i16, Expand);
  setOperationAction(ISD::SELECT, MVT::i32, Expand);
  for (MVT VT : {MVT::i16, MVT::i32})
    for (unsigned Opcode : {ISD::ROTL, ISD::ROTR})
      setOperationAction(Opcode, VT, Expand);
  // i32 is kept legal to model C166 register pairs, but operations which
  // require more than the native 16-bit ALU use the ordinary LLVM runtime
  // helper ABI.  The helpers therefore receive long operands through the
  // public R12-R15 convention and return through R4:R5.
  for (unsigned Opcode : {ISD::MUL, ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM})
    setOperationAction(Opcode, MVT::i32, LibCall);
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
  // compiler-rt's single-precision routines use __builtin_clz on their
  // 32-bit representation type.  The standard bit-counting libcall returns
  // C int (i16 on C166), which SelectionDAG extends back to i32.
  setOperationAction(ISD::CTLZ, MVT::i32, Expand);
  setOperationAction(ISD::CTLZ_ZERO_POISON, MVT::i32, LibCall);
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Custom);
  setOperationAction(ISD::BR_JT, MVT::Other, Custom);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setMinFunctionAlignment(Align(2));
  setPrefFunctionAlignment(Align(2));
  setMaxAtomicSizeInBitsSupported(0);
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
  case C166ISD::RET:
    return "C166ISD::RET";
  case C166ISD::NEARRET:
    return "C166ISD::NEARRET";
  case C166ISD::INTERRUPTRET:
    return "C166ISD::INTERRUPTRET";
  case C166ISD::STOREARG:
    return "C166ISD::STOREARG";
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
  case ISD::BR_JT:
    return LowerBRJT(Op, DAG);
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:
    return LowerI32Shift(Op, DAG);
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
    if (Amount->getZExtValue() == 16) {
      SDValue Value = Op.getOperand(0);
      SDValue Low = DAG.getNode(C166ISD::LOWORD, DL, MVT::i16, Value);
      SDValue High = DAG.getNode(C166ISD::HIWORD, DL, MVT::i16, Value);
      SDValue Zero = DAG.getConstant(0, DL, MVT::i16);
      switch (Op.getOpcode()) {
      case ISD::SHL:
        return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, Zero, Low);
      case ISD::SRL:
        return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, High, Zero);
      case ISD::SRA: {
        SDValue Sign = DAG.getNode(ISD::SRA, DL, MVT::i16, High,
                                   DAG.getConstant(15, DL, MVT::i16));
        return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, High, Sign);
      }
      default:
        llvm_unreachable("unexpected C166 i32 shift");
      }
    }
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
  auto EmitI32CompareBranch = [&](Register Lhs, Register Rhs, unsigned CC,
                                  MachineBasicBlock *TrueMBB,
                                  MachineBasicBlock *FalseMBB) {
    MachineFunction *MF = MBB->getParent();
    const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
    const BasicBlock *IRBB = MBB->getBasicBlock();
    MachineFunction::iterator InsertAt = std::next(MBB->getIterator());
    unsigned CallFrameSize = TII->getCallFrameSizeAt(MI);
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

    TrueMBB->replacePhiUsesWith(MBB, TrueGate);
    FalseMBB->replacePhiUsesWith(MBB, FalseGate);
    while (!MBB->succ_empty())
      MBB->removeSuccessor(MBB->succ_begin());

    auto EmitBranch = [&](MachineBasicBlock *Block, unsigned SubReg,
                          unsigned WordCC, MachineBasicBlock *BranchMBB,
                          MachineBasicBlock *FallthroughMBB) {
      MachineInstrBuilder Compare =
          Block == MBB
              ? BuildMI(*Block, MI, MI.getDebugLoc(), TII->get(C166::CMPBR))
              : BuildMI(Block, MI.getDebugLoc(), TII->get(C166::CMPBR));
      Compare.addReg(Lhs, {}, SubReg)
          .addReg(Rhs, {}, SubReg)
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

    // A C166 MOV updates N/Z/E.  Keeping a 32-bit comparison as SUB/SUBC
    // followed by a separate flags branch therefore lets register-allocation
    // spills destroy the signed condition.  Compare the high words first and
    // the low words only when they are equal.  CMPBR remains atomic until
    // post-RA expansion, so every hardware CMP is adjacent to its JMPR.
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
    EmitI32CompareBranch(MI.getOperand(0).getReg(), MI.getOperand(1).getReg(),
                         MI.getOperand(2).getImm(), TrueMBB, FalseMBB);
    MI.eraseFromParent();
    return MBB;
  }

  if (MI.getOpcode() != C166::SELECT16 &&
      MI.getOpcode() != C166::SELECT16_32CMP &&
      MI.getOpcode() != C166::SELECT32_16CMP &&
      MI.getOpcode() != C166::SELECT32_32CMP)
    llvm_unreachable("unexpected C166 custom inserter instruction");

  MachineFunction *MF = MBB->getParent();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  const BasicBlock *IRBB = MBB->getBasicBlock();
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
  FalseMBB->setCallFrameSize(CallFrameSize);
  SinkMBB->setCallFrameSize(CallFrameSize);

  SinkMBB->splice(SinkMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(MBB);

  MBB->addSuccessor(FalseMBB);
  MBB->addSuccessor(SinkMBB);
  FalseMBB->addSuccessor(SinkMBB);

  MachineBasicBlock *TrueValueMBB = MBB;
  if (MI.getOpcode() == C166::SELECT16 ||
      MI.getOpcode() == C166::SELECT32_16CMP) {
    BuildMI(*MBB, MI, MI.getDebugLoc(), TII->get(C166::CMPBR))
        .addReg(MI.getOperand(1).getReg())
        .addReg(MI.getOperand(2).getReg())
        .addImm(MI.getOperand(5).getImm())
        .addMBB(SinkMBB);
  } else {
    TrueValueMBB = EmitI32CompareBranch(
        MI.getOperand(1).getReg(), MI.getOperand(2).getReg(),
        MI.getOperand(5).getImm(), SinkMBB, FalseMBB);
  }

  BuildMI(*SinkMBB, SinkMBB->begin(), MI.getDebugLoc(),
          TII->get(TargetOpcode::PHI), MI.getOperand(0).getReg())
      .addReg(MI.getOperand(3).getReg())
      .addMBB(TrueValueMBB)
      .addReg(MI.getOperand(4).getReg())
      .addMBB(FalseMBB);

  MI.eraseFromParent();
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
  unsigned NextWord = 0;
  // Every banked function has a hidden word at [R0]. __banksw
  // consumes it when switching banks, and even a same-bank caller reserves
  // it so the function's public stack-argument layout does not depend on the
  // dynamic call path.
  unsigned StackOffset =
      C166::getCodeBank(MF.getFunction().getAddressSpace()) ? 2 : 0;
  bool UsedStack = CallConv == CallingConv::C166_StackParm;
  bool HasSRet = false;

  for (const ISD::InputArg &Arg : Ins) {
    if (Arg.Flags.isSRet()) {
      assert(!HasSRet && InVals.empty() && "sret must be the first argument");
      HasSRet = true;
      continue;
    }

    if (Arg.Flags.isByVal()) {
      UsedStack = true;
      unsigned Size = alignTo(Arg.Flags.getByValSize(), 2u);
      MachineFrameInfo &MFI = MF.getFrameInfo();
      int PublicFI = MFI.CreateFixedObject(Size, StackOffset, true);
      SDValue PublicAddress =
          DAG.getFrameIndex(PublicFI, getPointerTy(DAG.getDataLayout()));
      InVals.push_back(PublicAddress);
      StackOffset += Size;
      continue;
    }

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
      MachineFrameInfo &MFI = MF.getFrameInfo();
      auto LoadStackWord = [&](unsigned Offset, SDValue LoadChain) {
        int FI = MFI.CreateFixedObject(2, Offset, true);
        SDValue FrameIndex =
            DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
        SDValue Word = DAG.getLoad(MVT::i16, DL, LoadChain, FrameIndex,
                                   MachinePointerInfo::getFixedStack(MF, FI));
        return Word;
      };

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
    SDValue PublicAddress =
        DAG.getFrameIndex(PublicFI, getPointerTy(DAG.getDataLayout()));
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
  if (CLI.IsTailCall) {
    CLI.IsTailCall = false;
    if (CLI.CB && CLI.CB->isMustTailCall())
      report_fatal_error("C166 tail calls are not implemented yet");
  }

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
  SDValue Glue;
  unsigned NextWord = 0;
  bool UsedStack = CLI.CallConv == CallingConv::C166_StackParm;
  unsigned FixedArgCount = 0;
  if (CLI.IsVarArg) {
    if (!CLI.CB)
      report_fatal_error("C166 variadic call requires call-site type info");
    FixedArgCount = CLI.CB->getFunctionType()->getNumParams();
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
        IsDoubleSRet = SRetBytes == 8;
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
      SDValue Base = CLI.OutVals[I];
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
            MachinePointerInfo().getWithOffset(Offset), MVT::i8, Align(1));
      };

      for (unsigned Offset = 0; Offset != SlotSize; Offset += 2) {
        SDValue Word;
        if (Offset + 2 <= ObjectSize && ObjectAlign >= Align(2)) {
          // A word-aligned object remains aligned at every even ABI slot.
          Word = DAG.getLoad(MVT::i16, DL, Chain, AddressAt(Offset),
                             MachinePointerInfo().getWithOffset(Offset),
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

  // R0 is both the C user-stack pointer and the base of fixed incoming
  // arguments.  Complete every load used to form call arguments before the
  // first predecrement store changes R0.
  if (!StackWords.empty()) {
    auto CollectLoads = [](ArrayRef<SDValue> Values,
                           SmallPtrSetImpl<SDNode *> &Loads) {
      SmallPtrSet<SDNode *, 16> Visited;
      SmallVector<SDValue, 16> Worklist(Values.begin(), Values.end());
      while (!Worklist.empty()) {
        SDNode *Node = Worklist.pop_back_val().getNode();
        if (!Node || !Visited.insert(Node).second)
          continue;
        if (isa<LoadSDNode>(Node))
          Loads.insert(Node);
        for (SDValue Operand : Node->ops())
          if (Operand.getValueType() != MVT::Other &&
              Operand.getValueType() != MVT::Glue)
            Worklist.push_back(Operand);
      }
    };

    // A value dependency already orders the loads used by the first pushed
    // word.  Do not also add their chains: duplicate data/chain edges confuse
    // the register-pressure scheduler.
    SmallPtrSet<SDNode *, 4> FirstPushLoads;
    CollectLoads(ArrayRef(StackWords.back()), FirstPushLoads);
    while (Chain.getResNo() == 1 && FirstPushLoads.contains(Chain.getNode()))
      Chain = cast<LoadSDNode>(Chain.getNode())->getChain();

    SmallVector<SDValue, 8> ArgChains = {Chain};
    SmallPtrSet<SDNode *, 16> ArgLoads;
    CollectLoads(StackWords, ArgLoads);
    for (SDNode *Node : ArgLoads) {
      if (FirstPushLoads.contains(Node))
        continue;
      auto *Load = cast<LoadSDNode>(Node);
      SDValue LoadChain(Load, 1);
      if (LoadChain != Chain)
        ArgChains.push_back(LoadChain);
    }
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, ArgChains);
  }

  unsigned StackBytes = StackWords.size() * 2;
  unsigned CallFrameBytes = BankSlotBytes + SRetBytes + StackBytes;
  if (CallFrameBytes)
    Chain = DAG.getCALLSEQ_START(Chain, CallFrameBytes, 0, DL);

  // Allocate the complete outgoing area before storing any argument.  The
  // external layout is the same as reverse-order predecrement pushes, but the
  // physical R0 displacement now agrees with CALLSEQ_START at every point.
  // Keeping the adjustment in one pseudo also prevents post-isel block splits
  // from observing a partially allocated call frame.
  if (CallFrameBytes)
    Chain = DAG.getNode(C166ISD::ALLOCSP, DL, MVT::Other, Chain,
                        DAG.getConstant(CallFrameBytes, DL, MVT::i16));

  if (!StackWords.empty()) {
    // A banked function reserves [R0] for the bank word.  Public stack
    // arguments follow it in source order, and the caller-owned aggregate
    // result block follows those arguments.
    for (auto [Index, Word] : llvm::enumerate(StackWords))
      Chain = DAG.getNode(
          C166ISD::STOREARG, DL, MVT::Other, Chain,
          DAG.getConstant(BankSlotBytes + Index * 2, DL, MVT::i16), Word);
  }

  SDValue BankWord;
  if (NeedsBankSwitch) {
    BankWord = DAG.getConstant((CallerBank << 8) | CalleeBank, DL, MVT::i16);
    Chain = DAG.getNode(C166ISD::STOREARG, DL, MVT::Other, Chain,
                        DAG.getConstant(0, DL, MVT::i16), BankWord);
  }

  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  // Small has 16-bit ordinary data pointers but 32-bit default function
  // pointers.  SelectionDAG's synthesized libcalls arrive as untyped external
  // symbols using the generic pointer VT, so retain a distinct code-pointer
  // VT for far calls instead of inheriting Small's data representation.
  EVT FarCodePtrVT = DAG.getTarget().getCodeModel() == CodeModel::Small
                         ? EVT(MVT::i32)
                         : PtrVT;
  bool IsNearCall = CLI.Callee.getValueType() == MVT::i16;
  if (!IsNearCall && DAG.getTarget().getCodeModel() == CodeModel::Medium) {
    // Untyped runtime symbols and target-generated default-address-space
    // functions use Medium's near code class. Explicit huge functions and
    // 32-bit indirect function pointers retain their own class.
    if (isa<ExternalSymbolSDNode>(CLI.Callee))
      IsNearCall = true;
    else if (const auto *GA = dyn_cast<GlobalAddressSDNode>(CLI.Callee))
      IsNearCall = GA->getGlobal()->getAddressSpace() == 0;
  }
  if (DAG.getTarget().getCodeModel() == CodeModel::Small) {
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
    EmitNearCall = DAG.getTarget().getCodeModel() == CodeModel::Medium;
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

SDValue C166TargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  const C166MachineFunctionInfo *FuncInfo =
      MF.getInfo<C166MachineFunctionInfo>();
  if (!FuncInfo->hasVarArgsFrameIndex())
    report_fatal_error("C166 va_start used outside a variadic function");
  int FI = FuncInfo->getVarArgsFrameIndex();

  SDLoc DL(Op);
  SDValue FrameAddress =
      DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  if (FrameAddress.getValueType() == MVT::i16)
    return DAG.getStore(Op.getOperand(0), DL, FrameAddress, Op.getOperand(1),
                        MachinePointerInfo(SV));

  SDValue Offset = DAG.getNode(ISD::TRUNCATE, DL, MVT::i16, FrameAddress);
  Offset = DAG.getNode(ISD::AND, DL, MVT::i16, Offset,
                       DAG.getConstant(0x3fff, DL, MVT::i16));

  // R0 is a 16-bit user-stack offset within the page selected by DPP1.  A
  // A far data pointer stores the 14-bit page offset in its low word and
  // the DPP page number in its high word.
  SDValue Page = DAG.getCopyFromReg(Op.getOperand(0), DL, C166::DPP1, MVT::i16);
  SDValue Address = DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i32, Offset, Page);
  return DAG.getStore(Page.getValue(1), DL, Address, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

SDValue C166TargetLowering::LowerVAARG(SDValue Op, SelectionDAG &DAG) const {
  SDNode *Node = Op.getNode();
  EVT VT = Node->getValueType(0);
  SDValue Chain = Node->getOperand(0);
  SDValue VAListPtr = Node->getOperand(1);
  const Value *SV = cast<SrcValueSDNode>(Node->getOperand(2))->getValue();
  SDLoc DL(Node);

  MVT PointerVT = getPointerTy(DAG.getDataLayout());
  SDValue VAList = DAG.getLoad(PointerVT, DL, Chain, VAListPtr,
                               MachinePointerInfo(SV), Align(2));
  unsigned ArgBytes = alignTo(VT.getStoreSize().getFixedValue(), 2u);
  SDValue Next = PointerVT == MVT::i16
                     ? DAG.getNode(ISD::ADD, DL, MVT::i16, VAList,
                                   DAG.getConstant(ArgBytes, DL, MVT::i16))
                     : DAG.getNode(C166ISD::FARADD, DL, MVT::i32, VAList,
                                   DAG.getConstant(ArgBytes, DL, MVT::i16));
  Chain = DAG.getStore(VAList.getValue(1), DL, Next, VAListPtr,
                       MachinePointerInfo(SV), Align(2));

  // Stack arguments are only word-aligned in the C166 ABI, including
  // four-byte long values and far pointers.
  return DAG.getLoad(VT, DL, Chain, VAList, MachinePointerInfo(), Align(2));
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
