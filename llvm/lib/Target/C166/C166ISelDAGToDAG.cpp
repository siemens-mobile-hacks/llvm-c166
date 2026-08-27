//===-- C166ISelDAGToDAG.cpp - C166 DAG instruction selector ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166ISelLowering.h"
#include "C166SelectionDAGInfo.h"
#include "C166TargetMachine.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicsC166.h"
#include "llvm/Pass.h"
#include "llvm/TargetParser/C166TargetParser.h"

using namespace llvm;

#define DEBUG_TYPE "c166-isel"
#define PASS_NAME "C166 DAG-to-DAG instruction selection"

namespace {

class C166DAGToDAGISel : public SelectionDAGISel {
  static bool isPagedGlobalAddress(SDValue Address) {
    auto *GA = dyn_cast<GlobalAddressSDNode>(Address);
    return GA &&
           GA->getGlobal()->getAddressSpace() == C166::FarDataAddressSpace;
  }

  static bool isNearGlobalAddress(SDValue Address) {
    auto *GA = dyn_cast<GlobalAddressSDNode>(Address);
    return GA &&
           (GA->getGlobal()->getAddressSpace() == C166::NearAddressSpace ||
            GA->getGlobal()->getAddressSpace() == C166::XNearDataAddressSpace);
  }

  SDValue getTargetNearGlobalAddress(GlobalAddressSDNode *GA, SDLoc DL) {
    return CurDAG->getTargetGlobalAddress(GA->getGlobal(), DL, MVT::i16,
                                          GA->getOffset());
  }

  static unsigned getTargetCC(ISD::CondCode CC) {
    switch (CC) {
    case ISD::SETEQ:
      return C166::CC_EQ;
    case ISD::SETNE:
      return C166::CC_NE;
    case ISD::SETULT:
      return C166::CC_ULT;
    case ISD::SETULE:
      return C166::CC_ULE;
    case ISD::SETUGE:
      return C166::CC_UGE;
    case ISD::SETUGT:
      return C166::CC_UGT;
    case ISD::SETLT:
      return C166::CC_SLT;
    case ISD::SETLE:
      return C166::CC_SLE;
    case ISD::SETGE:
      return C166::CC_SGE;
    case ISD::SETGT:
      return C166::CC_SGT;
    default:
      llvm_unreachable("unsupported C166 condition code");
    }
  }

  bool selectFrameAddress(SDValue Address, SDValue &FrameIndex,
                          SDValue &Offset) {
    auto SelectFI = [&](FrameIndexSDNode *FI, int64_t Disp) {
      if (!isUInt<16>(Disp))
        return false;
      SDLoc DL(Address);
      FrameIndex = CurDAG->getTargetFrameIndex(FI->getIndex(), MVT::i32);
      Offset = CurDAG->getTargetConstant(Disp, DL, MVT::i16);
      return true;
    };

    if (auto *FI = dyn_cast<FrameIndexSDNode>(Address))
      return SelectFI(FI, 0);
    if (CurDAG->isBaseWithConstantOffset(Address))
      if (auto *FI = dyn_cast<FrameIndexSDNode>(Address.getOperand(0)))
        return SelectFI(
            FI, cast<ConstantSDNode>(Address.getOperand(1))->getSExtValue());
    if (Address.getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
        isa<ConstantSDNode>(Address.getOperand(0)) &&
        cast<ConstantSDNode>(Address.getOperand(0))->getZExtValue() ==
            Intrinsic::c166_far_add)
      if (auto *FI = dyn_cast<FrameIndexSDNode>(Address.getOperand(1)))
        if (auto *Offset = dyn_cast<ConstantSDNode>(Address.getOperand(2)))
          return SelectFI(FI, Offset->getSExtValue());
    return false;
  }

  SDValue extendWordToI32(SDLoc DL, SDValue Low, bool IsSigned) {
    return SDValue(
        CurDAG->getMachineNode(IsSigned ? C166::SEXT16_32 : C166::ZEXT16_32, DL,
                               MVT::i32, Low),
        0);
  }

  void replaceExtendingLoad(LoadSDNode *Load, SDNode *Selected, bool IsSigned) {
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                           {Load->getMemOperand()});
    if (Load->getValueType(0) == MVT::i16) {
      ReplaceNode(Load, Selected);
      return;
    }

    assert(Load->getValueType(0) == MVT::i32 &&
           "unexpected C166 extending-load result type");
    SDValue Pair = extendWordToI32(SDLoc(Load), SDValue(Selected, 0), IsSigned);
    CurDAG->ReplaceAllUsesOfValueWith(SDValue(Load, 0), Pair);
    CurDAG->ReplaceAllUsesOfValueWith(SDValue(Load, 1), SDValue(Selected, 1));
    CurDAG->RemoveDeadNode(Load);
  }

public:
  C166DAGToDAGISel(C166TargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISel(TM, OptLevel) {}

  void Select(SDNode *Node) override {
    if (Node->isMachineOpcode()) {
      Node->setNodeId(-1);
      return;
    }

    if (Node->getOpcode() == ISD::CALLSEQ_START ||
        Node->getOpcode() == ISD::CALLSEQ_END) {
      auto GetImmediate = [](SDValue Value) {
        if (auto *Constant = dyn_cast<ConstantSDNode>(Value))
          return Constant->getZExtValue();
        assert(Value.isMachineOpcode() &&
               Value.getMachineOpcode() == C166::CONST32 &&
               "unexpected C166 call-frame amount");
        return cast<ConstantSDNode>(Value.getOperand(0))->getZExtValue();
      };

      SDLoc DL(Node);
      SmallVector<SDValue, 4> Ops = {
          CurDAG->getTargetConstant(GetImmediate(Node->getOperand(1)), DL,
                                    MVT::i16),
          CurDAG->getTargetConstant(GetImmediate(Node->getOperand(2)), DL,
                                    MVT::i16),
          Node->getOperand(0)};
      if (Node->getNumOperands() == 4)
        Ops.push_back(Node->getOperand(3));
      unsigned Opcode = Node->getOpcode() == ISD::CALLSEQ_START
                            ? C166::ADJCALLSTACKDOWN
                            : C166::ADJCALLSTACKUP;
      ReplaceNode(Node,
                  CurDAG->getMachineNode(Opcode, DL, Node->getVTList(), Ops));
      return;
    }

    if (Node->getOpcode() == C166ISD::LOWORD ||
        Node->getOpcode() == C166ISD::HIWORD) {
      unsigned SubReg =
          Node->getOpcode() == C166ISD::LOWORD ? sub_lo16 : sub_hi16;
      ReplaceNode(Node,
                  CurDAG
                      ->getTargetExtractSubreg(SubReg, SDLoc(Node), MVT::i16,
                                               Node->getOperand(0))
                      .getNode());
      return;
    }

    // Generic legalization represents an i1 sign extension as
    // SIGN_EXTEND_INREG after promoting the boolean to a legal integer type.
    // C166 has no i1 registers, so materialize the exact 0/-1 value with
    // native word operations before extending it to a register pair.
    if (Node->getOpcode() == ISD::SIGN_EXTEND_INREG &&
        (Node->getValueType(0) == MVT::i16 ||
         Node->getValueType(0) == MVT::i32) &&
        cast<VTSDNode>(Node->getOperand(1))->getVT() == MVT::i1) {
      SDLoc DL(Node);
      SDValue Value = Node->getOperand(0);
      SDValue Low =
          Value.getValueType() == MVT::i32
              ? CurDAG->getTargetExtractSubreg(sub_lo16, DL, MVT::i16, Value)
              : Value;
      SDValue One(
          CurDAG->getMachineNode(C166::MOVri4, DL, MVT::i16,
                                 CurDAG->getTargetConstant(1, DL, MVT::i16)),
          0);
      SDValue Bit(CurDAG->getMachineNode(C166::ANDrr, DL, MVT::i16, Low, One),
                  0);
      SDValue Zero(
          CurDAG->getMachineNode(C166::MOVri4, DL, MVT::i16,
                                 CurDAG->getTargetConstant(0, DL, MVT::i16)),
          0);
      SDValue Negated(
          CurDAG->getMachineNode(C166::SUBrr, DL, MVT::i16, Zero, Bit), 0);
      if (Node->getValueType(0) == MVT::i16) {
        ReplaceNode(Node, Negated.getNode());
        return;
      }
      ReplaceNode(Node, extendWordToI32(DL, Negated, true).getNode());
      return;
    }

    if (Node->getOpcode() == ISD::SIGN_EXTEND_INREG &&
        Node->getValueType(0) == MVT::i16 &&
        cast<VTSDNode>(Node->getOperand(1))->getVT() == MVT::i8) {
      ReplaceNode(Node, CurDAG->getMachineNode(C166::SEXT8rr, SDLoc(Node),
                                               MVT::i16, Node->getOperand(0)));
      return;
    }

    if (Node->getOpcode() == ISD::ANY_EXTEND &&
        Node->getValueType(0) == MVT::i32 &&
        Node->getOperand(0).getValueType() == MVT::i16) {
      ReplaceNode(
          Node,
          extendWordToI32(SDLoc(Node), Node->getOperand(0), false).getNode());
      return;
    }

    if (Node->getOpcode() == ISD::SIGN_EXTEND_INREG &&
        Node->getValueType(0) == MVT::i32 &&
        cast<VTSDNode>(Node->getOperand(1))->getVT() == MVT::i16) {
      SDLoc DL(Node);
      SDValue Low = CurDAG->getTargetExtractSubreg(sub_lo16, DL, MVT::i16,
                                                   Node->getOperand(0));
      SDValue Extended = extendWordToI32(DL, Low, true);
      ReplaceNode(Node, Extended.getNode());
      return;
    }

    if (Node->getOpcode() == ISD::SIGN_EXTEND_INREG &&
        Node->getValueType(0) == MVT::i32 &&
        cast<VTSDNode>(Node->getOperand(1))->getVT() == MVT::i8) {
      SDLoc DL(Node);
      SDValue Low = CurDAG->getTargetExtractSubreg(sub_lo16, DL, MVT::i16,
                                                   Node->getOperand(0));
      SDValue Byte =
          SDValue(CurDAG->getMachineNode(C166::SEXT8rr, DL, MVT::i16, Low), 0);
      SDValue Extended = extendWordToI32(DL, Byte, true);
      ReplaceNode(Node, Extended.getNode());
      return;
    }

    if (Node->getOpcode() == ISD::BR) {
      SDValue Ops[] = {Node->getOperand(1), Node->getOperand(0)};
      ReplaceNode(
          Node, CurDAG->getMachineNode(C166::BR, SDLoc(Node), MVT::Other, Ops));
      return;
    }

    if (Node->getOpcode() == ISD::BR_CC) {
      auto CC = cast<CondCodeSDNode>(Node->getOperand(1))->get();
      SDValue Ops[] = {
          Node->getOperand(2), Node->getOperand(3),
          CurDAG->getTargetConstant(getTargetCC(CC), SDLoc(Node), MVT::i16),
          Node->getOperand(4), Node->getOperand(0)};
      unsigned Opcode = Node->getOperand(2).getValueType() == MVT::i32
                            ? C166::CMP32BR
                            : C166::CMPBR;
      ReplaceNode(Node,
                  CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::Other, Ops));
      return;
    }

    // Boolean expressions which are themselves formed with SELECT_CC are not
    // folded to BR_CC by generic DAG legalization.  Branch on the legalized
    // i16 boolean explicitly instead of leaving an otherwise unselectable
    // BRCOND in the DAG.
    if (Node->getOpcode() == ISD::BRCOND) {
      SDLoc DL(Node);
      SDValue ZeroImmediate = CurDAG->getTargetConstant(0, DL, MVT::i16);
      SDValue Zero(
          CurDAG->getMachineNode(C166::MOVri4, DL, MVT::i16, ZeroImmediate), 0);
      SDValue Ops[] = {Node->getOperand(1), Zero,
                       CurDAG->getTargetConstant(C166::CC_NE, DL, MVT::i16),
                       Node->getOperand(2), Node->getOperand(0)};
      ReplaceNode(Node,
                  CurDAG->getMachineNode(C166::CMPBR, DL, MVT::Other, Ops));
      return;
    }

    if (Node->getOpcode() == ISD::SELECT_CC &&
        (Node->getValueType(0) == MVT::i16 ||
         Node->getValueType(0) == MVT::i32)) {
      auto CC = cast<CondCodeSDNode>(Node->getOperand(4))->get();
      SDValue Ops[] = {
          Node->getOperand(0), Node->getOperand(1), Node->getOperand(2),
          Node->getOperand(3),
          CurDAG->getTargetConstant(getTargetCC(CC), SDLoc(Node), MVT::i16)};
      bool Compare32 = Node->getOperand(0).getValueType() == MVT::i32;
      bool Result32 = Node->getValueType(0) == MVT::i32;
      unsigned Opcode =
          Result32 ? (Compare32 ? C166::SELECT32_32CMP : C166::SELECT32_16CMP)
                   : (Compare32 ? C166::SELECT16_32CMP : C166::SELECT16);
      ReplaceNode(Node, CurDAG->getMachineNode(Opcode, SDLoc(Node),
                                               Node->getValueType(0), Ops));
      return;
    }

    if (Node->getOpcode() == C166ISD::NEARLOAD) {
      auto *Offset = cast<ConstantSDNode>(Node->getOperand(2));
      SDValue TargetOffset = CurDAG->getTargetConstant(Offset->getZExtValue(),
                                                       SDLoc(Node), MVT::i16);
      SDValue Ops[] = {Node->getOperand(1), TargetOffset, Node->getOperand(0)};
      if (Offset->isZero()) {
        SDValue ZeroOps[] = {Node->getOperand(1), Node->getOperand(0)};
        ReplaceNode(Node,
                    CurDAG->getMachineNode(C166::MOVrm, SDLoc(Node), MVT::i16,
                                           MVT::Other, ZeroOps));
      } else {
        ReplaceNode(Node, CurDAG->getMachineNode(C166::MOVrm16, SDLoc(Node),
                                                 MVT::i16, MVT::Other, Ops));
      }
      return;
    }

    if (Node->getOpcode() == C166ISD::FARADD) {
      ReplaceNode(Node, CurDAG->getMachineNode(C166::FARADD32, SDLoc(Node),
                                               MVT::i32, Node->getOperand(0),
                                               Node->getOperand(1)));
      return;
    }

    if (Node->getOpcode() == C166ISD::FRAMEADDR) {
      auto *FI = cast<FrameIndexSDNode>(Node->getOperand(0));
      SDLoc DL(Node);
      SDValue TargetFI = CurDAG->getTargetFrameIndex(FI->getIndex(), MVT::i32);
      SDValue Offset = CurDAG->getTargetConstant(0, DL, MVT::i16);
      ReplaceNode(Node, CurDAG->getMachineNode(C166::LEAfi, DL, MVT::i16,
                                               TargetFI, Offset));
      return;
    }

    // SelectionDAG canonicalizes small decrements to ADD with a negative
    // constant.  C166 has a compact positive SUB immediate form instead.
    if (Node->getOpcode() == ISD::ADD && Node->getValueType(0) == MVT::i16) {
      if (auto *Constant = dyn_cast<ConstantSDNode>(Node->getOperand(1))) {
        int64_t Value = Constant->getSExtValue();
        if (Value >= -7 && Value <= -1) {
          SDValue Amount = CurDAG->getTargetConstant(
              static_cast<uint64_t>(-Value), SDLoc(Node), MVT::i16);
          ReplaceNode(
              Node, CurDAG->getMachineNode(C166::SUBri3, SDLoc(Node), MVT::i16,
                                           Node->getOperand(0), Amount));
          return;
        }
      }
    }

    if (Node->getOpcode() == ISD::SHL && Node->getValueType(0) == MVT::i32) {
      if (auto *Amount = dyn_cast<ConstantSDNode>(Node->getOperand(1));
          Amount && Amount->getZExtValue() >= 1 &&
          Amount->getZExtValue() <= 15) {
        SDValue TargetAmount = CurDAG->getTargetConstant(Amount->getZExtValue(),
                                                         SDLoc(Node), MVT::i16);
        ReplaceNode(Node, CurDAG->getMachineNode(C166::SHL32ri4, SDLoc(Node),
                                                 MVT::i32, Node->getOperand(0),
                                                 TargetAmount));
        return;
      }
    }

    if (Node->getOpcode() == ISD::Constant &&
        Node->getValueType(0) == MVT::i16) {
      auto *Constant = cast<ConstantSDNode>(Node);
      uint64_t Value = Constant->getZExtValue();
      unsigned Opcode = Value < 16 ? C166::MOVri4 : C166::MOVri16;
      SDValue Immediate =
          CurDAG->getTargetConstant(Value, SDLoc(Node), MVT::i16);
      ReplaceNode(Node, CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16,
                                               Immediate));
      return;
    }

    if (Node->getOpcode() == ISD::Constant &&
        Node->getValueType(0) == MVT::i32) {
      auto *Constant = cast<ConstantSDNode>(Node);
      SDValue Value = CurDAG->getTargetConstant(Constant->getZExtValue(),
                                                SDLoc(Node), MVT::i32);
      ReplaceNode(Node, CurDAG->getMachineNode(C166::CONST32, SDLoc(Node),
                                               MVT::i32, Value));
      return;
    }

    if (Node->getOpcode() == C166ISD::CALL) {
      SmallVector<SDValue, 10> Ops = {Node->getOperand(1), Node->getOperand(2)};
      unsigned Last = Node->getNumOperands();
      SDValue Glue;
      if (Node->getOperand(Last - 1).getValueType() == MVT::Glue)
        Glue = Node->getOperand(--Last);
      for (unsigned I = 3; I != Last; ++I)
        Ops.push_back(Node->getOperand(I));
      Ops.push_back(Node->getOperand(0));
      if (Glue)
        Ops.push_back(Glue);
      ReplaceNode(Node, CurDAG->getMachineNode(C166::CALLS, SDLoc(Node),
                                               MVT::Other, MVT::Glue, Ops));
      return;
    }

    if (Node->getOpcode() == C166ISD::NEARCALL) {
      SDValue Callee = Node->getOperand(1);
      SmallVector<SDValue, 10> Ops = {Callee};
      unsigned Last = Node->getNumOperands();
      SDValue Glue;
      if (Node->getOperand(Last - 1).getValueType() == MVT::Glue)
        Glue = Node->getOperand(--Last);
      for (unsigned I = 2; I != Last; ++I)
        Ops.push_back(Node->getOperand(I));
      Ops.push_back(Node->getOperand(0));
      if (Glue)
        Ops.push_back(Glue);
      unsigned Opcode = isa<GlobalAddressSDNode, ExternalSymbolSDNode>(Callee)
                            ? C166::CALLA
                            : C166::CALLI;
      ReplaceNode(Node, CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::Other,
                                               MVT::Glue, Ops));
      return;
    }

    if (Node->getOpcode() == ISD::BRIND) {
      // A blockaddress always names a label in the current function and
      // therefore in the current code segment.  GNU indirect-goto tables use
      // the 32-bit C166 data-pointer representation in both Large and
      // Medium; jump tables and Small-model label addresses are already
      // 16-bit code offsets. JMPI consumes that offset while CSP supplies the
      // unchanged segment.
      SDValue Offset = Node->getOperand(1);
      if (Offset.getValueType() == MVT::i32)
        Offset = CurDAG->getTargetExtractSubreg(sub_lo16, SDLoc(Node), MVT::i16,
                                                Offset);
      else
        assert(Offset.getValueType() == MVT::i16 &&
               "unexpected C166 indirect-branch address type");
      ReplaceNode(Node,
                  CurDAG->getMachineNode(C166::JMPI, SDLoc(Node), MVT::Other,
                                         Offset, Node->getOperand(0)));
      return;
    }

    if (Node->getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
        isa<ConstantSDNode>(Node->getOperand(0)) &&
        cast<ConstantSDNode>(Node->getOperand(0))->getZExtValue() ==
            Intrinsic::c166_high_word) {
      ReplaceNode(Node,
                  CurDAG
                      ->getTargetExtractSubreg(sub_hi16, SDLoc(Node), MVT::i16,
                                               Node->getOperand(1))
                      .getNode());
      return;
    }

    if (Node->getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
        isa<ConstantSDNode>(Node->getOperand(0)) &&
        cast<ConstantSDNode>(Node->getOperand(0))->getZExtValue() ==
            Intrinsic::c166_far_add) {
      ReplaceNode(Node, CurDAG->getMachineNode(C166::FARADD32, SDLoc(Node),
                                               MVT::i32, Node->getOperand(1),
                                               Node->getOperand(2)));
      return;
    }

    if (Node->getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
        isa<ConstantSDNode>(Node->getOperand(0)) &&
        cast<ConstantSDNode>(Node->getOperand(0))->getZExtValue() ==
            Intrinsic::c166_read_dpp) {
      auto *DPPNumber = cast<ConstantSDNode>(Node->getOperand(1));
      unsigned Number = DPPNumber->getZExtValue();
      assert(Number < 4 && "invalid C166 DPP intrinsic selector");
      static constexpr unsigned DPPRegisters[] = {C166::DPP0, C166::DPP1,
                                                  C166::DPP2, C166::DPP3};
      unsigned Register = DPPRegisters[Number];
      SDValue DPP = CurDAG->getRegister(Register, MVT::i16);
      ReplaceNode(Node, CurDAG->getMachineNode(C166::MOVgsfr, SDLoc(Node),
                                               MVT::i16, DPP));
      return;
    }

    if (auto *JT = dyn_cast<JumpTableSDNode>(Node)) {
      SDLoc DL(Node);
      EVT PointerType = Node->getValueType(0);
      SDValue Table = CurDAG->getTargetJumpTable(JT->getIndex(), PointerType);
      if (PointerType == MVT::i32)
        ReplaceNode(Node, CurDAG->getMachineNode(C166::GLOBALDATAADDR32, DL,
                                                 MVT::i32, Table));
      else {
        assert(PointerType == MVT::i16 &&
               "unexpected C166 jump-table pointer type");
        ReplaceNode(Node,
                    CurDAG->getMachineNode(C166::MOVri16, DL, MVT::i16, Table));
      }
      return;
    }

    if (auto *GA = dyn_cast<GlobalAddressSDNode>(Node);
        GA && Node->getOpcode() == ISD::GlobalAddress &&
        Node->getValueType(0) == MVT::i32) {
      SDValue Address = CurDAG->getTargetGlobalAddress(
          GA->getGlobal(), SDLoc(Node), MVT::i32, GA->getOffset());
      unsigned Opcode;
      if (isa<Function>(GA->getGlobal()))
        Opcode = C166::GLOBALADDR32;
      else if (GA->getGlobal()->getAddressSpace() ==
                   C166::HugeDataAddressSpace ||
               GA->getGlobal()->getAddressSpace() ==
                   C166::SHugeDataAddressSpace)
        Opcode = C166::GLOBALHUGEDATAADDR32;
      else
        Opcode = C166::GLOBALDATAADDR32;
      ReplaceNode(
          Node, CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i32, Address));
      return;
    }

    if (auto *GA = dyn_cast<GlobalAddressSDNode>(Node);
        GA && Node->getOpcode() == ISD::GlobalAddress &&
        Node->getValueType(0) == MVT::i16 &&
        (GA->getGlobal()->getAddressSpace() == C166::NearAddressSpace ||
         GA->getGlobal()->getAddressSpace() == C166::XNearDataAddressSpace)) {
      SDValue Address = getTargetNearGlobalAddress(GA, SDLoc(Node));
      ReplaceNode(Node, CurDAG->getMachineNode(C166::NEARGLOBALADDR16,
                                               SDLoc(Node), MVT::i16, Address));
      return;
    }

    if (Node->getOpcode() == ISD::TRUNCATE &&
        Node->getValueType(0) == MVT::i16) {
      auto *Load = dyn_cast<LoadSDNode>(Node->getOperand(0));
      if (Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
          Load->getMemoryVT() == MVT::i32 &&
          Load->getBasePtr().getOpcode() == ISD::FrameIndex) {
        int FI = cast<FrameIndexSDNode>(Load->getBasePtr())->getIndex();
        SDLoc DL(Node);
        SDValue TargetFI =
            CurDAG->getTargetFrameIndex(FI, Load->getBasePtr().getValueType());
        SDValue Offset = CurDAG->getTargetConstant(0, DL, MVT::i16);
        SDNode *Selected =
            CurDAG->getMachineNode(C166::MOVfi, DL, MVT::i16, MVT::Other,
                                   TargetFI, Offset, Load->getChain());
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Load->getMemOperand()});
        CurDAG->ReplaceAllUsesOfValueWith(SDValue(Node, 0),
                                          SDValue(Selected, 0));
        CurDAG->RemoveDeadNode(Node);
        return;
      }
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i32 &&
        isPagedGlobalAddress(Load->getBasePtr())) {
      auto *GA = cast<GlobalAddressSDNode>(Load->getBasePtr());
      SDValue Address = CurDAG->getTargetGlobalAddress(
          GA->getGlobal(), SDLoc(Node), MVT::i32, GA->getOffset());
      SDNode *Selected =
          CurDAG->getMachineNode(C166::GLOBALLOAD32, SDLoc(Node), MVT::i32,
                                 MVT::Other, Address, Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getMemoryVT() == MVT::i8 &&
        (Load->getValueType(0) == MVT::i16 ||
         Load->getValueType(0) == MVT::i32)) {
      bool IsSigned = Load->getExtensionType() == ISD::SEXTLOAD;
      SDValue FrameIndex;
      SDValue Offset;
      if (selectFrameAddress(Load->getBasePtr(), FrameIndex, Offset)) {
        unsigned Opcode = IsSigned ? C166::FRAMELOAD8S : C166::FRAMELOAD8Z;
        SDNode *Selected =
            CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16, MVT::Other,
                                   FrameIndex, Offset, Load->getChain());
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }

      if (isPagedGlobalAddress(Load->getBasePtr())) {
        auto *GA = cast<GlobalAddressSDNode>(Load->getBasePtr());
        SDValue Address = CurDAG->getTargetGlobalAddress(
            GA->getGlobal(), SDLoc(Node), MVT::i32, GA->getOffset());
        unsigned Opcode = IsSigned ? C166::GLOBALLOAD8S : C166::GLOBALLOAD8Z;
        SDNode *Selected =
            CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16, MVT::Other,
                                   Address, Load->getChain());
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }

      if (isNearGlobalAddress(Load->getBasePtr())) {
        auto *GA = cast<GlobalAddressSDNode>(Load->getBasePtr());
        SDValue Address = getTargetNearGlobalAddress(GA, SDLoc(Node));
        unsigned Opcode =
            IsSigned ? C166::NEARGLOBALLOAD8S : C166::NEARGLOBALLOAD8Z;
        SDNode *Selected =
            CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16, MVT::Other,
                                   Address, Load->getChain());
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }

      if (Load->getBasePtr().getValueType() == MVT::i16) {
        SDValue Offset = CurDAG->getTargetConstant(0, SDLoc(Node), MVT::i16);
        unsigned Opcode = IsSigned ? C166::NEARLOAD8S : C166::NEARLOAD8Z;
        SDNode *Selected = CurDAG->getMachineNode(
            Opcode, SDLoc(Node), MVT::i16, MVT::Other, Load->getBasePtr(),
            Offset, Load->getChain());
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }

      if (Load->getBasePtr().getValueType() == MVT::i32) {
        bool IsSegmented =
            Load->getAddressSpace() == C166::HugeDataAddressSpace ||
            Load->getAddressSpace() == C166::SHugeDataAddressSpace;
        unsigned Opcode = IsSegmented
                              ? (IsSigned ? C166::SEGLOAD8S : C166::SEGLOAD8Z)
                              : (IsSigned ? C166::FARLOAD8S : C166::FARLOAD8Z);
        SDNode *Selected =
            CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16, MVT::Other,
                                   Load->getBasePtr(), Load->getChain());
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i32 &&
        Load->getBasePtr().getOpcode() == ISD::FrameIndex) {
      int FI = cast<FrameIndexSDNode>(Load->getBasePtr())->getIndex();
      SDLoc DL(Node);
      SDValue TargetFI =
          CurDAG->getTargetFrameIndex(FI, Load->getBasePtr().getValueType());
      SDValue LowOffset = CurDAG->getTargetConstant(0, DL, MVT::i16);
      SDValue HighOffset = CurDAG->getTargetConstant(2, DL, MVT::i16);
      SDNode *Low =
          CurDAG->getMachineNode(C166::MOVfi, DL, MVT::i16, MVT::Other,
                                 TargetFI, LowOffset, Load->getChain());
      SDNode *High =
          CurDAG->getMachineNode(C166::MOVfi, DL, MVT::i16, MVT::Other,
                                 TargetFI, HighOffset, SDValue(Low, 1));
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Low), {Load->getMemOperand()});
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(High),
                             {Load->getMemOperand()});

      SDValue Ops[] = {
          CurDAG->getTargetConstant(C166::GR32RegClassID, DL, MVT::i32),
          SDValue(Low, 0),
          CurDAG->getTargetConstant(sub_lo16, DL, MVT::i32),
          SDValue(High, 0),
          CurDAG->getTargetConstant(sub_hi16, DL, MVT::i32),
      };
      SDValue Pair(
          CurDAG->getMachineNode(TargetOpcode::REG_SEQUENCE, DL, MVT::i32, Ops),
          0);
      CurDAG->ReplaceAllUsesOfValueWith(SDValue(Node, 0), Pair);
      CurDAG->ReplaceAllUsesOfValueWith(SDValue(Node, 1), SDValue(High, 1));
      CurDAG->RemoveDeadNode(Node);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i32 &&
        isNearGlobalAddress(Load->getBasePtr())) {
      auto *GA = cast<GlobalAddressSDNode>(Load->getBasePtr());
      SDValue Address = getTargetNearGlobalAddress(GA, SDLoc(Node));
      SDNode *Selected =
          CurDAG->getMachineNode(C166::NEARGLOBALLOAD32, SDLoc(Node), MVT::i32,
                                 MVT::Other, Address, Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i32 &&
        Load->getBasePtr().getValueType() == MVT::i16) {
      SDValue Offset = CurDAG->getTargetConstant(0, SDLoc(Node), MVT::i16);
      SDNode *Selected = CurDAG->getMachineNode(
          C166::NEARLOAD32, SDLoc(Node), MVT::i32, MVT::Other,
          Load->getBasePtr(), Offset, Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i32 &&
        Load->getBasePtr().getValueType() == MVT::i32) {
      assert(Load->getAddressSpace() != C166::HugeDataAddressSpace &&
             "_huge i32 loads must be split before instruction selection");
      unsigned Opcode = Load->getAddressSpace() == C166::SHugeDataAddressSpace
                            ? C166::SHUGELOAD32
                            : C166::FARLOAD32;
      SDNode *Selected =
          CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i32, MVT::Other,
                                 Load->getBasePtr(), Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && Store->getMemoryVT() == MVT::i8 &&
        Store->getBasePtr().getValueType() == MVT::i16 &&
        !isNearGlobalAddress(Store->getBasePtr())) {
      SDValue Offset = CurDAG->getTargetConstant(0, SDLoc(Node), MVT::i16);
      SDValue Ops[] = {Store->getBasePtr(), Offset, Store->getValue(),
                       Store->getChain()};
      SDNode *Selected = CurDAG->getMachineNode(C166::NEARSTORE8, SDLoc(Node),
                                                MVT::Other, Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && Store->getMemoryVT() == MVT::i8 &&
        Store->getBasePtr().getValueType() == MVT::i32) {
      SDValue FrameIndex;
      SDValue Offset;
      if (selectFrameAddress(Store->getBasePtr(), FrameIndex, Offset)) {
        SDValue Ops[] = {FrameIndex, Offset, Store->getValue(),
                         Store->getChain()};
        SDNode *Selected = CurDAG->getMachineNode(C166::FRAMESTORE8,
                                                  SDLoc(Node), MVT::Other, Ops);
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Store->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && Store->getMemoryVT() == MVT::i8 &&
        isPagedGlobalAddress(Store->getBasePtr())) {
      auto *GA = cast<GlobalAddressSDNode>(Store->getBasePtr());
      SDValue Address = CurDAG->getTargetGlobalAddress(
          GA->getGlobal(), SDLoc(Node), MVT::i32, GA->getOffset());
      SDNode *Selected =
          CurDAG->getMachineNode(C166::GLOBALSTORE8, SDLoc(Node), MVT::Other,
                                 Address, Store->getValue(), Store->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && Store->getMemoryVT() == MVT::i8 &&
        isNearGlobalAddress(Store->getBasePtr())) {
      auto *GA = cast<GlobalAddressSDNode>(Store->getBasePtr());
      SDValue Address = getTargetNearGlobalAddress(GA, SDLoc(Node));
      SDNode *Selected = CurDAG->getMachineNode(
          C166::NEARGLOBALSTORE8, SDLoc(Node), MVT::Other, Address,
          Store->getValue(), Store->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && Store->getMemoryVT() == MVT::i8 &&
        Store->getBasePtr().getValueType() == MVT::i32) {
      unsigned Opcode =
          Store->getAddressSpace() == C166::HugeDataAddressSpace ||
                  Store->getAddressSpace() == C166::SHugeDataAddressSpace
              ? C166::SEGSTORE8
              : C166::FARSTORE8;
      SDNode *Selected = CurDAG->getMachineNode(
          Opcode, SDLoc(Node), MVT::Other, Store->getBasePtr(),
          Store->getValue(), Store->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && !Store->isTruncatingStore() &&
        Store->getMemoryVT() == MVT::i32 &&
        isPagedGlobalAddress(Store->getBasePtr())) {
      auto *GA = cast<GlobalAddressSDNode>(Store->getBasePtr());
      SDValue Address = CurDAG->getTargetGlobalAddress(
          GA->getGlobal(), SDLoc(Node), MVT::i32, GA->getOffset());
      SDNode *Selected =
          CurDAG->getMachineNode(C166::GLOBALSTORE32, SDLoc(Node), MVT::Other,
                                 Address, Store->getValue(), Store->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && !Store->isTruncatingStore() &&
        Store->getMemoryVT() == MVT::i32 &&
        isNearGlobalAddress(Store->getBasePtr())) {
      auto *GA = cast<GlobalAddressSDNode>(Store->getBasePtr());
      SDValue Address = getTargetNearGlobalAddress(GA, SDLoc(Node));
      SDNode *Selected = CurDAG->getMachineNode(
          C166::NEARGLOBALSTORE32, SDLoc(Node), MVT::Other, Address,
          Store->getValue(), Store->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && !Store->isTruncatingStore() &&
        Store->getMemoryVT() == MVT::i32 &&
        Store->getBasePtr().getOpcode() == ISD::FrameIndex) {
      auto *FI = cast<FrameIndexSDNode>(Store->getBasePtr());
      SDLoc DL(Node);
      SDValue TargetFI = CurDAG->getTargetFrameIndex(FI->getIndex(), MVT::i32);
      SDValue LowOffset = CurDAG->getTargetConstant(0, DL, MVT::i16);
      SDValue HighOffset = CurDAG->getTargetConstant(2, DL, MVT::i16);
      SDValue Value = Store->getValue();
      auto ExtractWord = [&](unsigned SubReg) {
        if (Value.getOpcode() == ISD::BUILD_PAIR)
          return Value.getOperand(SubReg == sub_lo16 ? 0 : 1);
        SDValue SubRegIndex = CurDAG->getTargetConstant(SubReg, DL, MVT::i32);
        return SDValue(CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG, DL,
                                              MVT::i16, Value, SubRegIndex),
                       0);
      };
      SDValue LowOps[] = {TargetFI, LowOffset, ExtractWord(sub_lo16),
                          Store->getChain()};
      SDNode *Low =
          CurDAG->getMachineNode(C166::MOVfiStore, DL, MVT::Other, LowOps);
      SDValue HighOps[] = {TargetFI, HighOffset, ExtractWord(sub_hi16),
                           SDValue(Low, 0)};
      SDNode *High =
          CurDAG->getMachineNode(C166::MOVfiStore, DL, MVT::Other, HighOps);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Low),
                             {Store->getMemOperand()});
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(High),
                             {Store->getMemOperand()});
      ReplaceNode(Node, High);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && !Store->isTruncatingStore() &&
        Store->getMemoryVT() == MVT::i32 &&
        Store->getBasePtr().getValueType() == MVT::i16) {
      SDValue Offset = CurDAG->getTargetConstant(0, SDLoc(Node), MVT::i16);
      SDValue Ops[] = {Store->getBasePtr(), Offset, Store->getValue(),
                       Store->getChain()};
      SDNode *Selected = CurDAG->getMachineNode(C166::NEARSTORE32, SDLoc(Node),
                                                MVT::Other, Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && !Store->isTruncatingStore() &&
        Store->getMemoryVT() == MVT::i32 &&
        Store->getBasePtr().getValueType() == MVT::i32) {
      assert(Store->getAddressSpace() != C166::HugeDataAddressSpace &&
             "_huge i32 stores must be split before instruction selection");
      unsigned Opcode = Store->getAddressSpace() == C166::SHugeDataAddressSpace
                            ? C166::SHUGESTORE32
                            : C166::FARSTORE32;
      SDNode *Selected = CurDAG->getMachineNode(
          Opcode, SDLoc(Node), MVT::Other, Store->getBasePtr(),
          Store->getValue(), Store->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && Store->getMemoryVT() == MVT::i16 &&
        isNearGlobalAddress(Store->getBasePtr())) {
      SDValue StoreValue = Store->getValue();
      if (Store->isTruncatingStore()) {
        assert(StoreValue.getValueType() == MVT::i32 &&
               "unsupported C166 truncating near-global word store");
        StoreValue = CurDAG->getTargetExtractSubreg(sub_lo16, SDLoc(Node),
                                                    MVT::i16, StoreValue);
      }
      auto *GA = cast<GlobalAddressSDNode>(Store->getBasePtr());
      SDValue Address = getTargetNearGlobalAddress(GA, SDLoc(Node));
      SDNode *Selected = CurDAG->getMachineNode(
          C166::NEARGLOBALSTORE16, SDLoc(Node), MVT::Other, Address, StoreValue,
          Store->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && Store->getMemoryVT() == MVT::i16 &&
        Store->getBasePtr().getValueType() == MVT::i16) {
      SDValue StoreValue = Store->getValue();
      if (Store->isTruncatingStore()) {
        assert(StoreValue.getValueType() == MVT::i32 &&
               "unsupported C166 truncating near word store");
        StoreValue = CurDAG->getTargetExtractSubreg(sub_lo16, SDLoc(Node),
                                                    MVT::i16, StoreValue);
      }
      SDNode *Selected = CurDAG->getMachineNode(C166::MOVmr, SDLoc(Node),
                                                MVT::Other, Store->getBasePtr(),
                                                StoreValue, Store->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && Store->getMemoryVT() == MVT::i16 &&
        Store->getBasePtr().getValueType() == MVT::i32) {
      SDValue StoreValue = Store->getValue();
      if (Store->isTruncatingStore()) {
        assert(StoreValue.getValueType() == MVT::i32 &&
               "unsupported C166 truncating word store");
        StoreValue = CurDAG->getTargetExtractSubreg(sub_lo16, SDLoc(Node),
                                                    MVT::i16, StoreValue);
      }
      SDValue FrameIndex;
      SDValue Offset;
      if (selectFrameAddress(Store->getBasePtr(), FrameIndex, Offset)) {
        SDValue Ops[] = {FrameIndex, Offset, StoreValue, Store->getChain()};
        SDNode *Selected = CurDAG->getMachineNode(C166::MOVfiStore, SDLoc(Node),
                                                  MVT::Other, Ops);
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Store->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
      if (isPagedGlobalAddress(Store->getBasePtr())) {
        auto *GA = cast<GlobalAddressSDNode>(Store->getBasePtr());
        SDValue Address = CurDAG->getTargetGlobalAddress(
            GA->getGlobal(), SDLoc(Node), MVT::i32, GA->getOffset());
        SDNode *Selected =
            CurDAG->getMachineNode(C166::GLOBALSTORE16, SDLoc(Node), MVT::Other,
                                   Address, StoreValue, Store->getChain());
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Store->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
      unsigned Opcode =
          Store->getAddressSpace() == C166::HugeDataAddressSpace ||
                  Store->getAddressSpace() == C166::SHugeDataAddressSpace
              ? C166::SEGSTORE16
              : C166::FARSTORE16;
      SDNode *Selected = CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::Other,
                                                Store->getBasePtr(), StoreValue,
                                                Store->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() != ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i16 && Load->getValueType(0) == MVT::i32) {
      bool IsSigned = Load->getExtensionType() == ISD::SEXTLOAD;
      SDValue FrameIndex;
      SDValue Offset;
      if (selectFrameAddress(Load->getBasePtr(), FrameIndex, Offset)) {
        SDNode *Selected = CurDAG->getMachineNode(
            C166::MOVfi, SDLoc(Node), MVT::i16, MVT::Other, FrameIndex, Offset,
            Load->getChain());
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }

      if (isPagedGlobalAddress(Load->getBasePtr())) {
        auto *GA = cast<GlobalAddressSDNode>(Load->getBasePtr());
        SDValue Address = CurDAG->getTargetGlobalAddress(
            GA->getGlobal(), SDLoc(Node), MVT::i32, GA->getOffset());
        SDNode *Selected =
            CurDAG->getMachineNode(C166::GLOBALLOAD16, SDLoc(Node), MVT::i16,
                                   MVT::Other, Address, Load->getChain());
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }

      if (isNearGlobalAddress(Load->getBasePtr())) {
        auto *GA = cast<GlobalAddressSDNode>(Load->getBasePtr());
        SDValue Address = getTargetNearGlobalAddress(GA, SDLoc(Node));
        SDNode *Selected = CurDAG->getMachineNode(
            C166::NEARGLOBALLOAD16, SDLoc(Node), MVT::i16, MVT::Other, Address,
            Load->getChain());
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }

      if (Load->getBasePtr().getValueType() == MVT::i16) {
        SDNode *Selected = CurDAG->getMachineNode(
            C166::MOVrm, SDLoc(Node), MVT::i16, MVT::Other, Load->getBasePtr(),
            Load->getChain());
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }

      if (Load->getBasePtr().getValueType() == MVT::i32) {
        unsigned Opcode =
            Load->getAddressSpace() == C166::HugeDataAddressSpace ||
                    Load->getAddressSpace() == C166::SHugeDataAddressSpace
                ? C166::SEGLOAD16
                : C166::FARLOAD16;
        SDNode *Selected =
            CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16, MVT::Other,
                                   Load->getBasePtr(), Load->getChain());
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i16 &&
        Load->getBasePtr().getOpcode() == ISD::FrameIndex) {
      int FI = cast<FrameIndexSDNode>(Load->getBasePtr())->getIndex();
      SDLoc DL(Node);
      SDValue TargetFI =
          CurDAG->getTargetFrameIndex(FI, Load->getBasePtr().getValueType());
      SDValue Offset = CurDAG->getTargetConstant(0, DL, MVT::i16);
      SDNode *Selected =
          CurDAG->getMachineNode(C166::MOVfi, DL, MVT::i16, MVT::Other,
                                 TargetFI, Offset, Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i16 &&
        isNearGlobalAddress(Load->getBasePtr())) {
      auto *GA = cast<GlobalAddressSDNode>(Load->getBasePtr());
      SDValue Address = getTargetNearGlobalAddress(GA, SDLoc(Node));
      SDNode *Selected =
          CurDAG->getMachineNode(C166::NEARGLOBALLOAD16, SDLoc(Node), MVT::i16,
                                 MVT::Other, Address, Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i16 &&
        Load->getBasePtr().getValueType() == MVT::i16) {
      SDNode *Selected =
          CurDAG->getMachineNode(C166::MOVrm, SDLoc(Node), MVT::i16, MVT::Other,
                                 Load->getBasePtr(), Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i16 &&
        Load->getBasePtr().getValueType() == MVT::i32) {
      SDValue FrameIndex;
      SDValue Offset;
      if (selectFrameAddress(Load->getBasePtr(), FrameIndex, Offset)) {
        SDNode *Selected = CurDAG->getMachineNode(
            C166::MOVfi, SDLoc(Node), MVT::i16, MVT::Other, FrameIndex, Offset,
            Load->getChain());
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Load->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
      if (isPagedGlobalAddress(Load->getBasePtr())) {
        auto *GA = cast<GlobalAddressSDNode>(Load->getBasePtr());
        SDValue Address = CurDAG->getTargetGlobalAddress(
            GA->getGlobal(), SDLoc(Node), MVT::i32, GA->getOffset());
        SDNode *Selected =
            CurDAG->getMachineNode(C166::GLOBALLOAD16, SDLoc(Node), MVT::i16,
                                   MVT::Other, Address, Load->getChain());
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Load->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
      unsigned Opcode =
          Load->getAddressSpace() == C166::HugeDataAddressSpace ||
                  Load->getAddressSpace() == C166::SHugeDataAddressSpace
              ? C166::SEGLOAD16
              : C166::FARLOAD16;
      SDNode *Selected =
          CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16, MVT::Other,
                                 Load->getBasePtr(), Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if ((Node->getOpcode() == ISD::XOR || Node->getOpcode() == ISD::AND ||
         Node->getOpcode() == ISD::OR) &&
        Node->getValueType(0) == MVT::i32) {
      // A legal i32 is a pair of adjacent word registers.  Select logical
      // operations word-wise so an i32 compare reduction does not leave an
      // unselectable wide XOR/AND/OR behind.
      SDLoc DL(Node);
      unsigned WordOpcode = Node->getOpcode() == ISD::XOR   ? C166::XORrr
                            : Node->getOpcode() == ISD::AND ? C166::ANDrr
                                                            : C166::ORrr;
      auto Extract = [&](SDValue Value, unsigned SubReg) {
        return CurDAG->getTargetExtractSubreg(SubReg, DL, MVT::i16, Value);
      };
      SDValue Low(
          CurDAG->getMachineNode(WordOpcode, DL, MVT::i16,
                                 Extract(Node->getOperand(0), sub_lo16),
                                 Extract(Node->getOperand(1), sub_lo16)),
          0);
      SDValue High(
          CurDAG->getMachineNode(WordOpcode, DL, MVT::i16,
                                 Extract(Node->getOperand(0), sub_hi16),
                                 Extract(Node->getOperand(1), sub_hi16)),
          0);
      SDValue Ops[] = {
          CurDAG->getTargetConstant(C166::GR32RegClassID, DL, MVT::i32),
          Low,
          CurDAG->getTargetConstant(sub_lo16, DL, MVT::i32),
          High,
          CurDAG->getTargetConstant(sub_hi16, DL, MVT::i32),
      };
      ReplaceNode(Node, CurDAG->getMachineNode(TargetOpcode::REG_SEQUENCE, DL,
                                               MVT::i32, Ops));
      return;
    }

    if (Node->getOpcode() == ISD::TRUNCATE &&
        Node->getValueType(0) == MVT::i16) {
      SDLoc DL(Node);
      SDValue Wide = Node->getOperand(0);
      auto ExtractWord = [&](auto &&Self, SDValue Source,
                             unsigned SubReg) -> SDValue {
        if (Source.getOpcode() == ISD::BUILD_PAIR)
          return Source.getOperand(SubReg == sub_lo16 ? 0 : 1);
        unsigned WordOpcode = 0;
        switch (Source.getOpcode()) {
        case ISD::XOR:
          WordOpcode = C166::XORrr;
          break;
        case ISD::OR:
          WordOpcode = C166::ORrr;
          break;
        case ISD::AND:
          WordOpcode = C166::ANDrr;
          break;
        default:
          break;
        }
        if (WordOpcode) {
          SDValue LHS = Self(Self, Source.getOperand(0), SubReg);
          SDValue RHS = Self(Self, Source.getOperand(1), SubReg);
          return SDValue(
              CurDAG->getMachineNode(WordOpcode, DL, MVT::i16, LHS, RHS), 0);
        }
        SDValue SubRegIndex = CurDAG->getTargetConstant(SubReg, DL, MVT::i32);
        return SDValue(CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG, DL,
                                              MVT::i16, Source, SubRegIndex),
                       0);
      };

      if (Wide.getOpcode() == ISD::OR) {
        SDValue Source;
        for (unsigned ShiftOperand : {0u, 1u}) {
          SDValue Shift = Wide.getOperand(ShiftOperand);
          SDValue Other = Wide.getOperand(1 - ShiftOperand);
          if (Shift.getOpcode() != ISD::SRL || Shift.getOperand(0) != Other)
            continue;
          auto *Amount = dyn_cast<ConstantSDNode>(Shift.getOperand(1));
          if (Amount && Amount->getZExtValue() == 16) {
            Source = Other;
            break;
          }
        }
        if (Source) {
          SDValue Low = ExtractWord(ExtractWord, Source, sub_lo16);
          SDValue High = ExtractWord(ExtractWord, Source, sub_hi16);
          ReplaceNode(Node, CurDAG->getMachineNode(C166::ORrr, DL, MVT::i16,
                                                   Low, High));
          return;
        }
      }

      unsigned SubReg = sub_lo16;
      if (Wide.getOpcode() == ISD::SRL) {
        auto *Amount = dyn_cast<ConstantSDNode>(Wide.getOperand(1));
        if (Amount && Amount->getZExtValue() == 16) {
          Wide = Wide.getOperand(0);
          SubReg = sub_hi16;
        }
      }

      if (Wide.getOpcode() != ISD::BUILD_PAIR) {
        ReplaceNode(Node, ExtractWord(ExtractWord, Wide, SubReg).getNode());
        return;
      }

      SDValue RC =
          CurDAG->getTargetConstant(C166::GR16RegClassID, DL, MVT::i32);
      ReplaceNode(Node, CurDAG->getMachineNode(
                            TargetOpcode::COPY_TO_REGCLASS, DL, MVT::i16,
                            Wide.getOperand(SubReg == sub_lo16 ? 0 : 1), RC));
      return;
    }

    if ((Node->getOpcode() == ISD::ZERO_EXTEND ||
         Node->getOpcode() == ISD::SIGN_EXTEND) &&
        Node->getValueType(0) == MVT::i32 &&
        Node->getOperand(0).getValueType() == MVT::i16) {
      SDValue Extended = extendWordToI32(SDLoc(Node), Node->getOperand(0),
                                         Node->getOpcode() == ISD::SIGN_EXTEND);
      ReplaceNode(Node, Extended.getNode());
      return;
    }

    if (Node->getOpcode() == ISD::BUILD_PAIR &&
        Node->getValueType(0) == MVT::i32) {
      SDLoc DL(Node);
      SDValue Ops[] = {
          CurDAG->getTargetConstant(C166::GR32RegClassID, DL, MVT::i32),
          Node->getOperand(0),
          CurDAG->getTargetConstant(sub_lo16, DL, MVT::i32),
          Node->getOperand(1),
          CurDAG->getTargetConstant(sub_hi16, DL, MVT::i32),
      };
      ReplaceNode(Node, CurDAG->getMachineNode(TargetOpcode::REG_SEQUENCE, DL,
                                               MVT::i32, Ops));
      return;
    }

    if (auto *FI = dyn_cast<FrameIndexSDNode>(Node);
        FI && Node->getOpcode() == ISD::FrameIndex &&
        Node->getValueType(0) == MVT::i16) {
      SDLoc DL(Node);
      SDValue TargetFI = CurDAG->getTargetFrameIndex(FI->getIndex(), MVT::i32);
      SDValue Offset = CurDAG->getTargetConstant(0, DL, MVT::i16);
      ReplaceNode(Node, CurDAG->getMachineNode(C166::LEAfi, DL, MVT::i16,
                                               TargetFI, Offset));
      return;
    }

    if (auto *FI = dyn_cast<FrameIndexSDNode>(Node);
        FI && Node->getOpcode() == ISD::FrameIndex &&
        Node->getValueType(0) == MVT::i32) {
      SDLoc DL(Node);
      SDValue TargetFI = CurDAG->getTargetFrameIndex(FI->getIndex(), MVT::i32);
      SDValue Offset = CurDAG->getTargetConstant(0, DL, MVT::i16);
      SDValue RawLow(
          CurDAG->getMachineNode(C166::LEAfi, DL, MVT::i16, TargetFI, Offset),
          0);
      SDValue MaskValue = CurDAG->getTargetConstant(0x3fff, DL, MVT::i16);
      SDValue Mask(
          CurDAG->getMachineNode(C166::MOVri16, DL, MVT::i16, MaskValue), 0);
      SDValue Low(
          CurDAG->getMachineNode(C166::ANDrr, DL, MVT::i16, RawLow, Mask), 0);
      SDValue DPP1 = CurDAG->getRegister(C166::DPP1, MVT::i16);
      SDValue High(CurDAG->getMachineNode(C166::MOVgsfr, DL, MVT::i16, DPP1),
                   0);
      SDValue Ops[] = {
          CurDAG->getTargetConstant(C166::GR32RegClassID, DL, MVT::i32),
          Low,
          CurDAG->getTargetConstant(sub_lo16, DL, MVT::i32),
          High,
          CurDAG->getTargetConstant(sub_hi16, DL, MVT::i32),
      };
      ReplaceNode(Node, CurDAG->getMachineNode(TargetOpcode::REG_SEQUENCE, DL,
                                               MVT::i32, Ops));
      return;
    }

    SelectCode(Node);
  }

#include "C166GenDAGISel.inc"
};

class C166DAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  C166DAGToDAGISelLegacy(C166TargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISelLegacy(
            ID, std::make_unique<C166DAGToDAGISel>(TM, OptLevel)) {}
};

} // namespace

char C166DAGToDAGISelLegacy::ID;

INITIALIZE_PASS(C166DAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

FunctionPass *llvm::createC166ISelDag(C166TargetMachine &TM,
                                      CodeGenOptLevel OptLevel) {
  return new C166DAGToDAGISelLegacy(TM, OptLevel);
}
