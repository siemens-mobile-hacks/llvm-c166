//===-- C166ISelDAGToDAG.cpp - C166 DAG instruction selector ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "C166.h"
#include "C166ISelLowering.h"
#include "C166MachineFunctionInfo.h"
#include "C166SelectionDAGInfo.h"
#include "C166TargetMachine.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicsC166.h"
#include "llvm/Pass.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MathExtras.h"
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

  static bool isCarryResult(SDValue Value) {
    if (Value.getOpcode() == ISD::AssertZext)
      Value = Value.getOperand(0);
    if (Value.isMachineOpcode() &&
        Value.getMachineOpcode() == C166::SUB64Carryrr)
      return Value.getResNo() == 0;
    if (Value.getResNo() != 1)
      return false;
    if (!Value.isMachineOpcode())
      return Value.getOpcode() == ISD::UADDO ||
             Value.getOpcode() == ISD::USUBO ||
             Value.getOpcode() == ISD::UADDO_CARRY ||
             Value.getOpcode() == ISD::USUBO_CARRY;
    switch (Value.getMachineOpcode()) {
    case C166::ADD32CCarryrr:
    case C166::ADD32Carryrr:
    case C166::ADDCCarryrr:
    case C166::ADDCarryri3:
    case C166::ADDCarryrr:
    case C166::SUB32CCarryrr:
    case C166::SUB32Carryrr:
    case C166::SUBCCarryrr:
    case C166::SUBCarryri3:
    case C166::SUBCarryrr:
    case C166::SETCARRY:
      return true;
    default:
      return false;
    }
  }

  static bool isCarryConsumer(const SDUse &Use) {
    unsigned Opcode = Use.getUser()->getOpcode();
    if (!Use.getUser()->isMachineOpcode())
      return Opcode == ISD::UADDO_CARRY || Opcode == ISD::USUBO_CARRY ||
             Opcode == ISD::BR_CC || Opcode == ISD::BRCOND;
    switch (Use.getUser()->getMachineOpcode()) {
    case C166::ADD32CCarryrr:
    case C166::ADD32CCarryValuerr:
    case C166::ADDCCarryrr:
    case C166::ADDCCarryInrr:
    case C166::ADDCCarryValuerr:
    case C166::SUB32CCarryrr:
    case C166::SUB32CCarryValuerr:
    case C166::SUBCCarryrr:
    case C166::SUBCCarryInrr:
    case C166::SUBCCarryValuerr:
    case C166::FLAGSBR:
      return true;
    default:
      return false;
    }
  }

  static SDValue getWordForZeroCompare(SDValue Value) {
    if ((Value.getOpcode() == ISD::ZERO_EXTEND ||
         Value.getOpcode() == ISD::SIGN_EXTEND) &&
        Value.getOperand(0).getValueType() == MVT::i16)
      return Value.getOperand(0);

    if (Value.getOpcode() == ISD::BUILD_PAIR) {
      auto *High = dyn_cast<ConstantSDNode>(Value.getOperand(1));
      if (High && High->isZero())
        return Value.getOperand(0);
    }

    if (Value.isMachineOpcode() &&
        (Value.getMachineOpcode() == C166::ZEXT16_32 ||
         Value.getMachineOpcode() == C166::SEXT16_32))
      return Value.getOperand(0);

    return {};
  }

  bool selectFrameAddress(SDValue Address, SDValue &FrameIndex,
                          SDValue &Offset) {
    SDLoc DL(Address);
    int64_t Displacement = 0;
    auto GetConstant = [](SDValue Value, int64_t &Result) {
      if (auto *Constant = dyn_cast<ConstantSDNode>(Value)) {
        Result = Constant->getSExtValue();
        return true;
      }
      if (!Value.isMachineOpcode() ||
          (Value.getMachineOpcode() != C166::MOVri4 &&
           Value.getMachineOpcode() != C166::MOVri16))
        return false;
      auto *Constant = dyn_cast<ConstantSDNode>(Value.getOperand(0));
      if (!Constant)
        return false;
      Result = Constant->getSExtValue();
      return true;
    };

    while (true) {
      if (auto *FI = dyn_cast<FrameIndexSDNode>(Address)) {
        if (!isUInt<16>(Displacement))
          return false;
        FrameIndex = CurDAG->getTargetFrameIndex(FI->getIndex(), MVT::i32);
        Offset = CurDAG->getTargetConstant(Displacement, DL, MVT::i16);
        return true;
      }

      SDValue Base;
      SDValue Amount;
      if (CurDAG->isBaseWithConstantOffset(Address)) {
        Base = Address.getOperand(0);
        Amount = Address.getOperand(1);
      } else if (Address.getOpcode() == C166ISD::FARADD) {
        Base = Address.getOperand(0);
        Amount = Address.getOperand(1);
      } else if (Address.getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
                 isa<ConstantSDNode>(Address.getOperand(0)) &&
                 cast<ConstantSDNode>(Address.getOperand(0))->getZExtValue() ==
                     Intrinsic::c166_far_add) {
        Base = Address.getOperand(1);
        Amount = Address.getOperand(2);
      } else if (Address.isMachineOpcode() &&
                 (Address.getMachineOpcode() == C166::FRAMEADDR32 ||
                  Address.getMachineOpcode() == C166::FARADD32 ||
                  Address.getMachineOpcode() == C166::FARADD32i ||
                  Address.getMachineOpcode() == C166::ADD32ri)) {
        Base = Address.getOperand(0);
        Amount = Address.getOperand(1);
      } else {
        return false;
      }

      int64_t Constant;
      if (!GetConstant(Amount, Constant))
        return false;
      Displacement += Constant;
      Address = Base;
    }
  }

  bool selectIndexedFrameAddress(SDValue Address, SDValue &NearAddress) {
    // An automatic object is represented by a far pointer when its address is
    // observable, but an in-bounds access rooted in the frame cannot leave the
    // DPP1 stack page.  Keep such accesses on the direct 16-bit address path;
    // other uses of the same pointer retain the normal far representation.
    SDValue Base;
    SDValue Offset;
    if (Address.getOpcode() == C166ISD::FARADD) {
      Base = Address.getOperand(0);
      Offset = Address.getOperand(1);
    } else if (Address.getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
               isa<ConstantSDNode>(Address.getOperand(0)) &&
               cast<ConstantSDNode>(Address.getOperand(0))->getZExtValue() ==
                   Intrinsic::c166_far_add) {
      Base = Address.getOperand(1);
      Offset = Address.getOperand(2);
    } else {
      return false;
    }

    auto *FI = dyn_cast<FrameIndexSDNode>(Base);
    if (!FI || Offset.getValueType() != MVT::i16)
      return false;

    SDLoc DL(Address);
    SDValue TargetFI = CurDAG->getTargetFrameIndex(FI->getIndex(), MVT::i32);
    SDValue Zero = CurDAG->getTargetConstant(0, DL, MVT::i16);
    SDValue Frame(
        CurDAG->getMachineNode(C166::LEAfi, DL, MVT::i16, TargetFI, Zero), 0);
    NearAddress = SDValue(
        CurDAG->getMachineNode(C166::ADDrr, DL, MVT::i16, Frame, Offset), 0);
    return true;
  }

  void selectNearAddress(SDValue Address, SDValue &Base, SDValue &Offset) {
    Base = Address;
    uint16_t Displacement = 0;

    // A register-displacement memory operation is no larger than a short
    // address update followed by an indirect operation, and it does not need
    // a temporary register.  Only consume a single-use update: retaining a
    // shared address computation would make the memory operation larger.
    if (Address->hasOneUse()) {
      SDValue CandidateBase;
      SDValue Amount;
      bool Subtract = false;

      if (CurDAG->isBaseWithConstantOffset(Address)) {
        CandidateBase = Address.getOperand(0);
        Amount = Address.getOperand(1);
      } else if (Address.getOpcode() == ISD::SUB) {
        CandidateBase = Address.getOperand(0);
        Amount = Address.getOperand(1);
        Subtract = true;
      } else if (Address.isMachineOpcode()) {
        switch (Address.getMachineOpcode()) {
        case C166::ADDri3:
        case C166::ADDri16:
          CandidateBase = Address.getOperand(0);
          Amount = Address.getOperand(1);
          break;
        case C166::SUBri3:
          CandidateBase = Address.getOperand(0);
          Amount = Address.getOperand(1);
          Subtract = true;
          break;
        default:
          break;
        }
      }

      if (auto *Constant = dyn_cast_or_null<ConstantSDNode>(Amount.getNode())) {
        uint16_t Value = static_cast<uint16_t>(Constant->getZExtValue());
        Base = CandidateBase;
        Displacement = Subtract ? static_cast<uint16_t>(0U - Value) : Value;
      }
    }

    Offset = CurDAG->getTargetConstant(Displacement, SDLoc(Address), MVT::i16);
  }

  void selectFarAddress(SDValue Address, SDValue &Base, SDValue &Offset) {
    Base = Address;
    uint16_t Displacement = 0;

    SDValue CandidateBase;
    SDValue Amount;
    if (CurDAG->isBaseWithConstantOffset(Address)) {
      CandidateBase = Address.getOperand(0);
      Amount = Address.getOperand(1);
    } else if (Address.getOpcode() == C166ISD::FARADD) {
      CandidateBase = Address.getOperand(0);
      Amount = Address.getOperand(1);
    } else if (Address.getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
               isa<ConstantSDNode>(Address.getOperand(0)) &&
               cast<ConstantSDNode>(Address.getOperand(0))->getZExtValue() ==
                   Intrinsic::c166_far_add) {
      CandidateBase = Address.getOperand(1);
      Amount = Address.getOperand(2);
    } else if (Address.isMachineOpcode() &&
               (Address.getMachineOpcode() == C166::FARADD32i ||
                Address.getMachineOpcode() == C166::ADD32ri)) {
      CandidateBase = Address.getOperand(0);
      Amount = Address.getOperand(1);
    } else if (Address.isMachineOpcode() &&
               Address.getMachineOpcode() == C166::FARADD32) {
      SDValue CandidateAmount = Address.getOperand(1);
      if (CandidateAmount.isMachineOpcode() &&
          (CandidateAmount.getMachineOpcode() == C166::MOVri4 ||
           CandidateAmount.getMachineOpcode() == C166::MOVri16)) {
        CandidateBase = Address.getOperand(0);
        Amount = CandidateAmount.getOperand(0);
      }
    }

    bool OneAddressUse = Address->hasNUsesOfValue(1, Address.getResNo());
    bool TwoAddressUses = Address->hasNUsesOfValue(2, Address.getResNo());
    bool BaseIsShared = CandidateBase && !CandidateBase->hasNUsesOfValue(
                                             1, CandidateBase.getResNo());
    if ((OneAddressUse || (TwoAddressUses && BaseIsShared))) {
      if (auto *Constant = dyn_cast_or_null<ConstantSDNode>(Amount.getNode())) {
        Base = CandidateBase;
        Displacement = static_cast<uint16_t>(Constant->getZExtValue());
      }
    }

    Offset = CurDAG->getTargetConstant(Displacement, SDLoc(Address), MVT::i16);
  }

  static bool isSFRAddressSpace(unsigned AddressSpace) {
    return AddressSpace == C166::SFRAddressSpace ||
           AddressSpace == C166::ESFRAddressSpace;
  }

  static bool getSFRPhysicalAddress(SDValue Pointer, unsigned AddressSpace,
                                    uint16_t &Address) {
    auto *Constant = dyn_cast<ConstantSDNode>(Pointer);
    if (!Constant || !Constant->getAPIntValue().isIntN(16))
      return false;
    Address = static_cast<uint16_t>(Constant->getZExtValue());
    uint16_t Base = AddressSpace == C166::ESFRAddressSpace ? 0xf000 : 0xfe00;
    return Address >= Base && Address <= Base + 2 * 0xef &&
           ((Address - Base) & 1) == 0;
  }

  static unsigned getSFRBitAddress(uint16_t Address, bool IsESFR,
                                   unsigned Bit) {
    uint16_t Base = IsESFR ? 0xf000 : 0xfe00;
    unsigned Word = (Address - Base) / 2;
    assert(Word >= 0x80 && Word <= 0xef && Bit < 16 &&
           "invalid C166 bit-addressable SFR");
    return (Word << 4) | Bit;
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

  struct BitValue {
    SDValue Source;
    unsigned SourceBit;
    unsigned DestinationBit;
    bool Inverted;
  };

  static const ConstantSDNode *getImmediate(SDValue Value) {
    if (auto *Constant = dyn_cast<ConstantSDNode>(Value))
      return Constant;
    if (!Value.isMachineOpcode())
      return nullptr;
    unsigned Opcode = Value.getMachineOpcode();
    if (Opcode != C166::MOVri4 && Opcode != C166::MOVri16)
      return nullptr;
    return dyn_cast<ConstantSDNode>(Value.getOperand(0));
  }

  static bool matchImmediateBinary(SDValue Value, unsigned GenericOpcode,
                                   unsigned ShortOpcode, unsigned LongOpcode,
                                   SDValue &Source, uint16_t &Immediate) {
    if (Value.getOpcode() == GenericOpcode) {
      const ConstantSDNode *Constant = getImmediate(Value.getOperand(1));
      Source = Value.getOperand(0);
      if (!Constant) {
        Constant = getImmediate(Source);
        Source = Value.getOperand(1);
      }
      if (!Constant)
        return false;
      Immediate = static_cast<uint16_t>(Constant->getZExtValue());
      return true;
    }

    if (!Value.isMachineOpcode() || (Value.getMachineOpcode() != ShortOpcode &&
                                     Value.getMachineOpcode() != LongOpcode))
      return false;
    const ConstantSDNode *Constant = getImmediate(Value.getOperand(1));
    if (!Constant)
      return false;
    Source = Value.getOperand(0);
    Immediate = static_cast<uint16_t>(Constant->getZExtValue());
    return true;
  }

  static bool matchShift(SDValue Value, SDValue &Source, unsigned &Amount,
                         bool &IsLeft) {
    unsigned Opcode = Value.getOpcode();
    if (Opcode == ISD::SHL || Opcode == ISD::SRL) {
      const ConstantSDNode *Constant = getImmediate(Value.getOperand(1));
      if (!Constant || Constant->getZExtValue() >= 16)
        return false;
      Source = Value.getOperand(0);
      Amount = Constant->getZExtValue();
      IsLeft = Opcode == ISD::SHL;
      return true;
    }

    if (!Value.isMachineOpcode())
      return false;
    Opcode = Value.getMachineOpcode();
    if (Opcode != C166::SHLri4 && Opcode != C166::SHRri4)
      return false;
    const ConstantSDNode *Constant = getImmediate(Value.getOperand(1));
    if (!Constant || Constant->getZExtValue() >= 16)
      return false;
    Source = Value.getOperand(0);
    Amount = Constant->getZExtValue();
    IsLeft = Opcode == C166::SHLri4;
    return true;
  }

  static bool matchSourceBit(SDValue Value, unsigned Bit, SDValue &Source,
                             unsigned &SourceBit, bool &Inverted) {
    Inverted = false;
    while (true) {
      SDValue ShiftSource;
      unsigned ShiftAmount;
      bool IsLeft;
      if (matchShift(Value, ShiftSource, ShiftAmount, IsLeft)) {
        if ((IsLeft && Bit < ShiftAmount) ||
            (!IsLeft && Bit + ShiftAmount >= 16))
          return false;
        Bit = IsLeft ? Bit - ShiftAmount : Bit + ShiftAmount;
        Value = ShiftSource;
        continue;
      }

      if (Value.getOpcode() == ISD::XOR) {
        const ConstantSDNode *Constant = getImmediate(Value.getOperand(1));
        SDValue Candidate = Value.getOperand(0);
        if (!Constant) {
          Constant = getImmediate(Candidate);
          Candidate = Value.getOperand(1);
        }
        if (Constant &&
            static_cast<uint16_t>(Constant->getZExtValue()) == 0xffff) {
          Inverted = !Inverted;
          Value = Candidate;
          continue;
        }
      }

      if (Value.isMachineOpcode() && Value.getMachineOpcode() == C166::CPL) {
        Inverted = !Inverted;
        Value = Value.getOperand(0);
        continue;
      }

      if (Value.getValueType() != MVT::i16)
        return false;
      Source = Value;
      SourceBit = Bit;
      return true;
    }
  }

  static bool matchOneBitValue(SDValue Value, BitValue &Match) {
    SmallVector<std::pair<bool, unsigned>, 4> Shifts;
    while (true) {
      SDValue ShiftSource;
      unsigned ShiftAmount;
      bool IsLeft;
      if (!matchShift(Value, ShiftSource, ShiftAmount, IsLeft))
        break;
      Shifts.emplace_back(IsLeft, ShiftAmount);
      Value = ShiftSource;
    }

    SDValue MaskedSource;
    uint16_t Mask;
    if (!matchImmediateBinary(Value, ISD::AND, C166::ANDri3, C166::ANDri16,
                              MaskedSource, Mask) ||
        !isPowerOf2_32(Mask))
      return false;

    unsigned DestinationBit = countr_zero(Mask);
    for (auto [IsLeft, Amount] : llvm::reverse(Shifts)) {
      if ((IsLeft && DestinationBit + Amount >= 16) ||
          (!IsLeft && DestinationBit < Amount))
        return false;
      DestinationBit =
          IsLeft ? DestinationBit + Amount : DestinationBit - Amount;
    }

    Match.DestinationBit = DestinationBit;
    return matchSourceBit(MaskedSource, countr_zero(Mask), Match.Source,
                          Match.SourceBit, Match.Inverted);
  }

  static bool matchClearedBit(SDValue Value, SDValue &Source, unsigned &Bit) {
    if (Value.isMachineOpcode() && Value.getMachineOpcode() == C166::BCLRreg) {
      const ConstantSDNode *BitOperand = getImmediate(Value.getOperand(1));
      if (!BitOperand || BitOperand->getZExtValue() >= 16)
        return false;
      Source = Value.getOperand(0);
      Bit = BitOperand->getZExtValue();
      return true;
    }

    uint16_t Mask;
    if (!matchImmediateBinary(Value, ISD::AND, C166::ANDri3, C166::ANDri16,
                              Source, Mask))
      return false;
    uint16_t Cleared = static_cast<uint16_t>(~Mask);
    if (!isPowerOf2_32(Cleared))
      return false;
    Bit = countr_zero(Cleared);
    return true;
  }

  static bool matchBitMask(SDValue Value, SDValue &Source, unsigned &Bit,
                           unsigned &SourceBit, bool &Inverted) {
    SDValue Variable;
    uint16_t Mask;
    if (!matchImmediateBinary(Value, ISD::OR, C166::ORri3, C166::ORri16,
                              Variable, Mask))
      return false;
    uint16_t Cleared = static_cast<uint16_t>(~Mask);
    if (!isPowerOf2_32(Cleared))
      return false;
    Bit = countr_zero(Cleared);
    return matchSourceBit(Variable, Bit, Source, SourceBit, Inverted);
  }

  void selectBitBinary(SDNode *Node, unsigned Opcode, SDValue OldValue,
                       const BitValue &Bit) {
    SDLoc DL(Node);
    SDValue Ops[] = {
        OldValue, Bit.Source,
        CurDAG->getTargetConstant(Bit.DestinationBit, DL, MVT::i16),
        CurDAG->getTargetConstant(Bit.SourceBit, DL, MVT::i16)};
    ReplaceNode(Node, CurDAG->getMachineNode(Opcode, DL, MVT::i16, Ops));
  }

  bool selectInvertedBitBoolean(SDNode *Node) {
    if (Node->getOpcode() != ISD::SELECT_CC ||
        Node->getValueType(0) != MVT::i16)
      return false;

    ISD::CondCode CC = cast<CondCodeSDNode>(Node->getOperand(4))->get();
    if (CC != ISD::SETEQ && CC != ISD::SETNE)
      return false;

    SDValue Compared = Node->getOperand(0);
    const ConstantSDNode *Other = getImmediate(Node->getOperand(1));
    if (!Other || !Other->isZero()) {
      Other = getImmediate(Compared);
      if (!Other || !Other->isZero())
        return false;
      Compared = Node->getOperand(1);
    }

    const ConstantSDNode *TrueValue = getImmediate(Node->getOperand(2));
    const ConstantSDNode *FalseValue = getImmediate(Node->getOperand(3));
    if (!TrueValue || !FalseValue)
      return false;
    uint64_t True = TrueValue->getZExtValue();
    uint64_t False = FalseValue->getZExtValue();
    if (!((True == 1 && False == 0) || (True == 0 && False == 1)))
      return false;

    SDValue Source;
    uint16_t Mask;
    if (!matchImmediateBinary(Compared, ISD::AND, C166::ANDri3, C166::ANDri16,
                              Source, Mask) ||
        !isPowerOf2_32(Mask))
      return false;

    BitValue Bit;
    Bit.DestinationBit = 0;
    if (!matchSourceBit(Source, countr_zero(Mask), Bit.Source, Bit.SourceBit,
                        Bit.Inverted))
      return false;

    Bit.Inverted ^= CC == ISD::SETEQ;
    Bit.Inverted ^= True == 0;
    if (!Bit.Inverted)
      return false;

    SDLoc DL(Node);
    SDValue Zero(
        CurDAG->getMachineNode(C166::MOVri4, DL, MVT::i16,
                               CurDAG->getTargetConstant(0, DL, MVT::i16)),
        0);
    selectBitBinary(Node, C166::BMOVNreg, Zero, Bit);
    return true;
  }

  bool selectShiftedSignMask(SDNode *Node) {
    if (Node->getOpcode() != ISD::AND || Node->getValueType(0) != MVT::i16)
      return false;

    SDValue Shift = Node->getOperand(0);
    const ConstantSDNode *Mask = getImmediate(Node->getOperand(1));
    if (!Mask) {
      Shift = Node->getOperand(1);
      Mask = getImmediate(Node->getOperand(0));
    }

    SDValue Source;
    unsigned Amount;
    bool IsLeft;
    if (!Mask || !Shift.hasOneUse() ||
        !matchShift(Shift, Source, Amount, IsLeft) || IsLeft || Amount == 0 ||
        Amount >= 12)
      return false;

    uint16_t ExpectedMask = (uint16_t(1) << (15 - Amount)) - 1;
    if (Mask->getZExtValue() != ExpectedMask)
      return false;

    SDLoc DL(Node);
    SDValue ClearOps[] = {Source, CurDAG->getTargetConstant(15, DL, MVT::i16)};
    SDValue Cleared(
        CurDAG->getMachineNode(C166::BCLRreg, DL, MVT::i16, ClearOps), 0);
    SDValue ShiftOps[] = {Cleared,
                          CurDAG->getTargetConstant(Amount, DL, MVT::i16)};
    ReplaceNode(Node,
                CurDAG->getMachineNode(C166::SHRri4, DL, MVT::i16, ShiftOps));
    return true;
  }

  bool selectShiftedBitfieldMask(SDNode *Node) {
    if (Node->getOpcode() != ISD::AND || Node->getValueType(0) != MVT::i16)
      return false;

    SDValue Shift = Node->getOperand(0);
    const ConstantSDNode *MaskNode = getImmediate(Node->getOperand(1));
    if (!MaskNode) {
      Shift = Node->getOperand(1);
      MaskNode = getImmediate(Node->getOperand(0));
    }

    SDValue Source;
    unsigned Amount;
    bool IsLeft;
    if (!MaskNode || !Shift.hasOneUse() ||
        !matchShift(Shift, Source, Amount, IsLeft) || IsLeft || Amount == 0)
      return false;

    uint16_t Mask = static_cast<uint16_t>(MaskNode->getZExtValue());
    if (!isMask_32(Mask) || isUInt<3>(Mask) || Mask == 0xff)
      return false;

    unsigned Width = llvm::bit_width(Mask);
    if (Amount + Width > 16)
      return false;

    uint16_t KnownZero = static_cast<uint16_t>(
        CurDAG->computeKnownBits(Shift).Zero.getZExtValue());
    uint16_t Cleared =
        static_cast<uint16_t>(~Mask) & static_cast<uint16_t>(~KnownZero);
    if (Cleared == 0 || isPowerOf2_32(Cleared))
      return false;

    SDLoc DL(Node);
    unsigned LeftAmount = 16 - Amount - Width;
    SDValue Value = Source;
    if (LeftAmount != 0) {
      SDValue Ops[] = {Value,
                       CurDAG->getTargetConstant(LeftAmount, DL, MVT::i16)};
      Value =
          SDValue(CurDAG->getMachineNode(C166::SHLri4, DL, MVT::i16, Ops), 0);
    }
    SDValue Ops[] = {Value,
                     CurDAG->getTargetConstant(16 - Width, DL, MVT::i16)};
    ReplaceNode(Node, CurDAG->getMachineNode(C166::SHRri4, DL, MVT::i16, Ops));
    return true;
  }

public:
  C166DAGToDAGISel(C166TargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISel(TM, OptLevel) {}

  void Select(SDNode *Node) override {
    if (Node->isMachineOpcode()) {
      Node->setNodeId(-1);
      return;
    }

    if (Node->getOpcode() == ISD::INTRINSIC_VOID &&
        isa<ConstantSDNode>(Node->getOperand(1)) &&
        cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue() ==
            Intrinsic::c166_sfr_bit_write) {
      auto *Address = cast<ConstantSDNode>(Node->getOperand(2));
      auto *Bit = cast<ConstantSDNode>(Node->getOperand(3));
      auto *IsESFR = cast<ConstantSDNode>(Node->getOperand(5));
      bool Extended = IsESFR->isOne();
      unsigned BitAddress =
          getSFRBitAddress(static_cast<uint16_t>(Address->getZExtValue()),
                           Extended, Bit->getZExtValue());
      SDLoc DL(Node);
      SDValue Chain = Node->getOperand(0);
      SDValue TargetAddress =
          CurDAG->getTargetConstant(BitAddress, DL, MVT::i16);
      SDValue Value = Node->getOperand(4);
      if (auto *Constant = dyn_cast<ConstantSDNode>(Value);
          Constant && Constant->getZExtValue() <= 1) {
        unsigned Opcode = Constant->isZero()
                              ? (Extended ? C166::BCLResfr : C166::BCLR)
                              : (Extended ? C166::BSETesfr : C166::BSET);
        ReplaceNode(Node, CurDAG->getMachineNode(Opcode, DL, MVT::Other,
                                                 TargetAddress, Chain));
      } else {
        SDValue SourceBit = CurDAG->getTargetConstant(0, DL, MVT::i16);
        unsigned Opcode = Extended ? C166::BMOVesfrreg : C166::BMOVsfrreg;
        SDValue Operands[] = {TargetAddress, Value, SourceBit, Chain};
        ReplaceNode(
            Node, CurDAG->getMachineNode(Opcode, DL, MVT::Other, Operands));
      }
      return;
    }

    if (Node->getOpcode() == ISD::INTRINSIC_W_CHAIN &&
        isa<ConstantSDNode>(Node->getOperand(1)) &&
        cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue() ==
            Intrinsic::c166_sfr_bit_read) {
      auto *Address = cast<ConstantSDNode>(Node->getOperand(2));
      auto *Bit = cast<ConstantSDNode>(Node->getOperand(3));
      auto *IsESFR = cast<ConstantSDNode>(Node->getOperand(4));
      bool Extended = IsESFR->isOne();
      unsigned BitAddress =
          getSFRBitAddress(static_cast<uint16_t>(Address->getZExtValue()),
                           Extended, Bit->getZExtValue());
      SDLoc DL(Node);
      SDValue Zero = SDValue(CurDAG->getMachineNode(
                                 C166::MOVri4, DL, MVT::i16,
                                 CurDAG->getTargetConstant(0, DL, MVT::i16)),
                             0);
      SDValue Operands[] = {
          Zero, CurDAG->getTargetConstant(0, DL, MVT::i16),
          CurDAG->getTargetConstant(BitAddress, DL, MVT::i16),
          Node->getOperand(0)};
      unsigned Opcode = Extended ? C166::BMOVregesfr : C166::BMOVregsfr;
      ReplaceNode(Node, CurDAG->getMachineNode(Opcode, DL,
                                               {MVT::i16, MVT::Other},
                                               Operands));
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i16 && Load->getValueType(0) == MVT::i16 &&
        isSFRAddressSpace(Load->getAddressSpace())) {
      uint16_t Address;
      if (getSFRPhysicalAddress(Load->getBasePtr(), Load->getAddressSpace(),
                                Address)) {
        SDValue TargetAddress =
            CurDAG->getTargetConstant(Address, SDLoc(Node), MVT::i16);
        SDNode *Selected =
            CurDAG->getMachineNode(C166::MOVabsgd, SDLoc(Node), MVT::i16,
                                   MVT::Other, TargetAddress, Load->getChain());
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Load->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && !Store->isTruncatingStore() &&
        Store->getMemoryVT() == MVT::i16 &&
        isSFRAddressSpace(Store->getAddressSpace())) {
      uint16_t Address;
      if (getSFRPhysicalAddress(Store->getBasePtr(), Store->getAddressSpace(),
                                Address)) {
        SDValue TargetAddress =
            CurDAG->getTargetConstant(Address, SDLoc(Node), MVT::i16);
        SDNode *Selected = CurDAG->getMachineNode(
            C166::MOVabsdg, SDLoc(Node), MVT::Other, TargetAddress,
            Store->getValue(), Store->getChain());
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Store->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
    }

    if (selectShiftedSignMask(Node))
      return;

    if (selectShiftedBitfieldMask(Node))
      return;

    if (Node->getOpcode() == C166ISD::SHL64) {
      auto *Amount = cast<ConstantSDNode>(Node->getOperand(2));
      SDLoc DL(Node);
      SDValue Ops[] = {
          Node->getOperand(0), Node->getOperand(1),
          CurDAG->getTargetConstant(Amount->getZExtValue(), DL, MVT::i16)};
      ReplaceNode(Node, CurDAG->getMachineNode(C166::SHL64ri4, DL,
                                               Node->getVTList(), Ops));
      return;
    }

    if (Node->getOpcode() == C166ISD::SRL64_1 ||
        Node->getOpcode() == C166ISD::SRA64_1) {
      unsigned Opcode = Node->getOpcode() == C166ISD::SRL64_1 ? C166::SRL64ri1
                                                              : C166::SRA64ri1;
      ReplaceNode(Node, CurDAG->getMachineNode(
                            Opcode, SDLoc(Node), Node->getVTList(),
                            {Node->getOperand(0), Node->getOperand(1),
                             Node->getOperand(1)}));
      return;
    }

    if (Node->getOpcode() == C166ISD::SRL32_1 ||
        Node->getOpcode() == C166ISD::SRA32_1) {
      unsigned Opcode = Node->getOpcode() == C166ISD::SRL32_1 ? C166::SRL32ri1
                                                              : C166::SRA32ri1;
      ReplaceNode(Node, CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i32,
                                               Node->getOperand(0)));
      return;
    }

    if (Node->getOpcode() == C166ISD::SRLPAIR1 ||
        Node->getOpcode() == C166ISD::SRAPAIR1) {
      unsigned Opcode = Node->getOpcode() == C166ISD::SRLPAIR1
                            ? C166::SRLPAIRri1
                            : C166::SRAPAIRri1;
      ReplaceNode(Node, CurDAG->getMachineNode(
                            Opcode, SDLoc(Node), Node->getVTList(),
                            {Node->getOperand(0), Node->getOperand(1)}));
      return;
    }

    if (Node->getOpcode() == ISD::UADDO || Node->getOpcode() == ISD::USUBO ||
        Node->getOpcode() == ISD::UADDO_CARRY ||
        Node->getOpcode() == ISD::USUBO_CARRY) {
      SDLoc DL(Node);
      bool HasCarryIn = Node->getNumOperands() == 3;
      SDValue CarryIn = HasCarryIn ? Node->getOperand(2) : SDValue();
      auto GetConstant = [](SDValue Value) -> const ConstantSDNode * {
        if (auto *Constant = dyn_cast<ConstantSDNode>(Value))
          return Constant;
        if (!Value.isMachineOpcode())
          return nullptr;
        unsigned Opcode = Value.getMachineOpcode();
        if (Opcode != C166::MOVri4 && Opcode != C166::MOVri16)
          return nullptr;
        return dyn_cast<ConstantSDNode>(Value.getOperand(0));
      };
      bool IsAdd = Node->getOpcode() == ISD::UADDO ||
                   Node->getOpcode() == ISD::UADDO_CARRY;
      EVT ValueType = Node->getValueType(0);
      assert((ValueType == MVT::i16 || ValueType == MVT::i32) &&
             "unexpected C166 carry operation type");
      bool IsWide = ValueType == MVT::i32;
      bool HasValueConsumer = false;
      for (const SDUse &Use : Node->uses()) {
        if (Use.getResNo() != 1)
          continue;
        HasValueConsumer |= !isCarryConsumer(Use);
      }
      bool MaterializeCarry = HasValueConsumer;
      const ConstantSDNode *Constant =
          HasCarryIn ? GetConstant(CarryIn) : nullptr;
      if (!HasCarryIn && !IsWide && !MaterializeCarry) {
        SDValue LHS = Node->getOperand(0);
        SDValue RHS = Node->getOperand(1);
        const ConstantSDNode *Immediate = GetConstant(RHS);
        if (IsAdd && !Immediate) {
          Immediate = GetConstant(LHS);
          std::swap(LHS, RHS);
        }
        if (Immediate && Immediate->getZExtValue() <= 7) {
          SDLoc DL(Node);
          SDValue Ops[] = {LHS, CurDAG->getTargetConstant(
                                    Immediate->getZExtValue(), DL, MVT::i16)};
          unsigned Opcode = IsAdd ? C166::ADDCarryri3 : C166::SUBCarryri3;
          ReplaceNode(
              Node, CurDAG->getMachineNode(Opcode, DL, Node->getVTList(), Ops));
          return;
        }
      }

      unsigned Opcode;
      SmallVector<SDValue, 3> Ops = {Node->getOperand(0), Node->getOperand(1)};
      if (!HasCarryIn || (Constant && Constant->isZero())) {
        if (IsWide) {
          if (MaterializeCarry)
            Opcode = IsAdd ? C166::ADD32CarryValuerr : C166::SUB32CarryValuerr;
          else
            Opcode = IsAdd ? C166::ADD32Carryrr : C166::SUB32Carryrr;
        } else if (MaterializeCarry) {
          Opcode = IsAdd ? C166::ADDCarryValuerr : C166::SUBCarryValuerr;
        } else {
          Opcode = IsAdd ? C166::ADDCarryrr : C166::SUBCarryrr;
        }
      } else {
        if (IsWide) {
          if (MaterializeCarry)
            Opcode =
                IsAdd ? C166::ADD32CCarryValuerr : C166::SUB32CCarryValuerr;
          else
            Opcode = IsAdd ? C166::ADD32CCarryrr : C166::SUB32CCarryrr;
        } else if (MaterializeCarry) {
          Opcode = IsAdd ? C166::ADDCCarryValuerr : C166::SUBCCarryValuerr;
        } else {
          Opcode = IsAdd ? C166::ADDCCarryrr : C166::SUBCCarryrr;
        }
        if (!isCarryResult(CarryIn)) {
          unsigned RegClass = C166::GR16RegClass.getID();
          SDValue RC = CurDAG->getTargetConstant(RegClass, DL, MVT::i32);
          SDValue Scratch(CurDAG->getMachineNode(
                              TargetOpcode::COPY_TO_REGCLASS, DL, MVT::i16,
                              CarryIn, RC),
                          0);
          CarryIn = SDValue(CurDAG->getMachineNode(
                                C166::SETCARRY, DL, MVT::i16, Scratch),
                            0);
        }
        Ops.push_back(CarryIn);
      }

      ReplaceNode(Node,
                  CurDAG->getMachineNode(Opcode, DL, Node->getVTList(), Ops));
      return;
    }

    // With a dead final overflow result, generic combines may leave the last
    // limb as (a +/- b) +/- carry instead of an *_CARRY node.  Fold that
    // canonical form back into the native carry-consuming instruction.
    if ((Node->getOpcode() == ISD::ADD || Node->getOpcode() == ISD::SUB) &&
        Node->getValueType(0) == MVT::i16) {
      bool IsAdd = Node->getOpcode() == ISD::ADD;
      SDValue Arithmetic = Node->getOperand(0);
      SDValue Carry = Node->getOperand(1);
      if (IsAdd && !isCarryResult(Carry) && isCarryResult(Arithmetic))
        std::swap(Arithmetic, Carry);

      unsigned InnerOpcode = IsAdd ? ISD::ADD : ISD::SUB;
      if (isCarryResult(Carry) && Arithmetic->hasOneUse() &&
          Arithmetic.getOpcode() == InnerOpcode) {
        unsigned Opcode = IsAdd ? C166::ADDCCarryInrr : C166::SUBCCarryInrr;
        SDValue Ops[] = {Arithmetic.getOperand(0), Arithmetic.getOperand(1),
                         Carry};
        ReplaceNode(Node,
                    CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16, Ops));
        return;
      }
    }

    if ((Node->getOpcode() == ISD::AND || Node->getOpcode() == ISD::OR ||
         Node->getOpcode() == ISD::XOR) &&
        Node->getValueType(0) == MVT::i16) {
      SDValue LHS = Node->getOperand(0);
      SDValue RHS = Node->getOperand(1);

      if (Node->getOpcode() == ISD::OR) {
        for (unsigned Swap = 0; Swap != 2; ++Swap) {
          SDValue OldValue;
          unsigned ClearedBit;
          BitValue Bit;
          if (matchClearedBit(LHS, OldValue, ClearedBit) &&
              matchOneBitValue(RHS, Bit) && ClearedBit == Bit.DestinationBit) {
            selectBitBinary(Node, Bit.Inverted ? C166::BMOVNreg : C166::BMOVreg,
                            OldValue, Bit);
            return;
          }
          std::swap(LHS, RHS);
        }
      }

      if (Node->getOpcode() == ISD::OR || Node->getOpcode() == ISD::XOR) {
        for (unsigned Swap = 0; Swap != 2; ++Swap) {
          BitValue Bit;
          if (matchOneBitValue(RHS, Bit) && !Bit.Inverted &&
              (Bit.SourceBit != Bit.DestinationBit ||
               Bit.DestinationBit >= 3)) {
            selectBitBinary(Node,
                            Node->getOpcode() == ISD::OR ? C166::BORreg
                                                         : C166::BXORreg,
                            LHS, Bit);
            return;
          }
          std::swap(LHS, RHS);
        }
      }

      if (Node->getOpcode() == ISD::AND) {
        for (unsigned Swap = 0; Swap != 2; ++Swap) {
          BitValue Bit;
          if (matchBitMask(RHS, Bit.Source, Bit.DestinationBit, Bit.SourceBit,
                           Bit.Inverted) &&
              !Bit.Inverted) {
            selectBitBinary(Node, C166::BANDreg, LHS, Bit);
            return;
          }
          std::swap(LHS, RHS);
        }
      }
    }

    if ((Node->getOpcode() == ISD::AND || Node->getOpcode() == ISD::OR) &&
        Node->getValueType(0) == MVT::i16) {
      auto GetImmediate = [](SDValue Value) -> const ConstantSDNode * {
        if (auto *Constant = dyn_cast<ConstantSDNode>(Value))
          return Constant;
        if (Value.isMachineOpcode() &&
            (Value.getMachineOpcode() == C166::MOVri4 ||
             Value.getMachineOpcode() == C166::MOVri16))
          return dyn_cast<ConstantSDNode>(Value.getOperand(0));
        return nullptr;
      };

      SDValue Value = Node->getOperand(0);
      const ConstantSDNode *Immediate = GetImmediate(Node->getOperand(1));
      if (!Immediate) {
        Immediate = GetImmediate(Value);
        Value = Node->getOperand(1);
      }

      if (Immediate) {
        uint16_t Mask = static_cast<uint16_t>(Immediate->getZExtValue());
        if (Node->getOpcode() == ISD::AND && Mask == 0xff) {
          ReplaceNode(Node, CurDAG->getMachineNode(C166::ZEXT8rr, SDLoc(Node),
                                                   MVT::i16, Value));
          return;
        }

        // BFLDL/BFLDH replace one byte with (old & ~mask) | data.  Select
        // them for the equivalent two-operation expression when the other
        // byte is unchanged and the OR only sets bits cleared by the AND.
        if (Node->getOpcode() == ISD::OR && Mask != 0 && Value.hasOneUse()) {
          SDValue Source;
          const ConstantSDNode *KeepImmediate = nullptr;
          if (Value.getOpcode() == ISD::AND) {
            Source = Value.getOperand(0);
            KeepImmediate = GetImmediate(Value.getOperand(1));
            if (!KeepImmediate) {
              Source = Value.getOperand(1);
              KeepImmediate = GetImmediate(Value.getOperand(0));
            }
          } else if (Value.isMachineOpcode() &&
                     (Value.getMachineOpcode() == C166::ANDri3 ||
                      Value.getMachineOpcode() == C166::ANDri16)) {
            Source = Value.getOperand(0);
            KeepImmediate = GetImmediate(Value.getOperand(1));
          }

          if (KeepImmediate) {
            uint16_t Keep =
                static_cast<uint16_t>(KeepImmediate->getZExtValue());
            if (isPowerOf2_32(Mask) && Mask <= 0x80 &&
                Keep == static_cast<uint16_t>(0xff & ~Mask)) {
              SDLoc DL(Node);
              SDValue ZeroExtended(
                  CurDAG->getMachineNode(C166::ZEXT8rr, DL, MVT::i16, Source),
                  0);
              SDValue Ops[] = {
                  ZeroExtended,
                  CurDAG->getTargetConstant(countr_zero(Mask), DL, MVT::i16)};
              ReplaceNode(Node, CurDAG->getMachineNode(C166::BSETreg, DL,
                                                       MVT::i16, Ops));
              return;
            }

            bool UpdatesLow = (Keep & 0xff00) == 0xff00 &&
                              (Mask & 0xff00) == 0 && (Keep & Mask) == 0;
            bool UpdatesHigh = (Keep & 0x00ff) == 0x00ff &&
                               (Mask & 0x00ff) == 0 && (Keep & Mask) == 0;
            if (UpdatesLow || UpdatesHigh) {
              unsigned Shift = UpdatesHigh ? 8 : 0;
              uint16_t ByteMask = static_cast<uint16_t>(0xff << Shift);
              uint16_t Clear = static_cast<uint16_t>(~Keep) & ByteMask;
              if (Clear != 0) {
                SDLoc DL(Node);
                SDValue Ops[] = {
                    Source,
                    CurDAG->getTargetConstant(Clear >> Shift, DL, MVT::i16),
                    CurDAG->getTargetConstant(Mask >> Shift, DL, MVT::i16)};
                ReplaceNode(Node,
                            CurDAG->getMachineNode(UpdatesHigh ? C166::BFLDHreg
                                                               : C166::BFLDLreg,
                                                   DL, MVT::i16, Ops));
                return;
              }
            }
          }
        }

        bool IsSet =
            Node->getOpcode() == ISD::OR && isPowerOf2_32(Mask) && Mask >= 8;
        uint16_t ClearedBit = static_cast<uint16_t>(~Mask);
        if (Node->getOpcode() == ISD::AND && !isUInt<3>(Mask)) {
          uint16_t KnownZero = static_cast<uint16_t>(
              CurDAG->computeKnownBits(Value).Zero.getZExtValue());
          ClearedBit &= static_cast<uint16_t>(~KnownZero);
        }
        bool IsClear =
            Node->getOpcode() == ISD::AND && isPowerOf2_32(ClearedBit);
        if (IsSet || IsClear) {
          unsigned Bit = countr_zero(IsSet ? Mask : ClearedBit);
          SDValue Ops[] = {
              Value, CurDAG->getTargetConstant(Bit, SDLoc(Node), MVT::i16)};
          ReplaceNode(Node, CurDAG->getMachineNode(IsSet ? C166::BSETreg
                                                         : C166::BCLRreg,
                                                   SDLoc(Node), MVT::i16, Ops));
          return;
        }
      }
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
      SDValue LHS = Node->getOperand(2);
      SDValue RHS = Node->getOperand(3);
      auto GetImmediate = [](SDValue Value) -> std::optional<uint64_t> {
        if (auto *Constant = dyn_cast<ConstantSDNode>(Value))
          return Constant->getZExtValue();
        if (Value.isMachineOpcode() &&
            (Value.getMachineOpcode() == C166::MOVri4 ||
             Value.getMachineOpcode() == C166::MOVri16))
          return cast<ConstantSDNode>(Value.getOperand(0))->getZExtValue();
        return std::nullopt;
      };
      std::optional<uint64_t> Immediate = GetImmediate(RHS);
      if (isCarryResult(LHS) && Immediate && *Immediate <= 1 &&
          (CC == ISD::SETEQ || CC == ISD::SETNE)) {
        bool BranchOnSet = CC == ISD::SETEQ ? *Immediate == 1 : *Immediate == 0;
        SDLoc DL(Node);
        SDValue Ops[] = {
            LHS,
            CurDAG->getTargetConstant(BranchOnSet ? C166::CC_ULT : C166::CC_UGE,
                                      DL, MVT::i16),
            Node->getOperand(4), Node->getOperand(0)};
        ReplaceNode(Node,
                    CurDAG->getMachineNode(C166::FLAGSBR, DL, MVT::Other, Ops));
        if (RHS->use_empty())
          CurDAG->RemoveDeadNode(RHS.getNode());
        return;
      }
      if ((CC == ISD::SETEQ || CC == ISD::SETNE) && Immediate == 0 &&
          LHS.getValueType() == MVT::i32) {
        if (SDValue Word = getWordForZeroCompare(LHS)) {
          SDLoc DL(Node);
          SDValue Ops[] = {
              Word, CurDAG->getTargetConstant(0, DL, MVT::i16),
              CurDAG->getTargetConstant(getTargetCC(CC), DL, MVT::i16),
              Node->getOperand(4), Node->getOperand(0)};
          ReplaceNode(
              Node, CurDAG->getMachineNode(C166::CMPBRi, DL, MVT::Other, Ops));
          return;
        }
      }
      if ((CC == ISD::SETEQ || CC == ISD::SETNE) && Immediate == 0 &&
          (LHS.getValueType() == MVT::i16 || LHS.getValueType() == MVT::i32) &&
          LHS.hasOneUse()) {
        SDValue Source;
        const ConstantSDNode *Mask = nullptr;
        if (LHS.getOpcode() == ISD::AND) {
          Mask = dyn_cast<ConstantSDNode>(LHS.getOperand(1));
          Source = LHS.getOperand(0);
          if (!Mask) {
            Mask = dyn_cast<ConstantSDNode>(LHS.getOperand(0));
            Source = LHS.getOperand(1);
          }
        } else if (LHS.isMachineOpcode() &&
                   (LHS.getMachineOpcode() == C166::ANDri3 ||
                    LHS.getMachineOpcode() == C166::ANDri16 ||
                    LHS.getMachineOpcode() == C166::AND32ri)) {
          Mask = dyn_cast<ConstantSDNode>(LHS.getOperand(1));
          Source = LHS.getOperand(0);
        }

        if (Mask) {
          uint32_t MaskValue = static_cast<uint32_t>(Mask->getZExtValue());
          if (isPowerOf2_32(MaskValue)) {
            SDLoc DL(Node);
            unsigned Bit = llvm::countr_zero(MaskValue);
            SDValue Word = Source;
            if (LHS.getValueType() == MVT::i32) {
              unsigned SubReg = Bit < 16 ? sub_lo16 : sub_hi16;
              Word =
                  CurDAG->getTargetExtractSubreg(SubReg, DL, MVT::i16, Source);
              Bit &= 15;
            }
            SDValue Ops[] = {
                Word, CurDAG->getTargetConstant(Bit, DL, MVT::i16),
                CurDAG->getTargetConstant(getTargetCC(CC), DL, MVT::i16),
                Node->getOperand(4), Node->getOperand(0)};
            ReplaceNode(
                Node, CurDAG->getMachineNode(C166::BITBR, DL, MVT::Other, Ops));
            return;
          }

          if (LHS.getValueType() == MVT::i32) {
            uint16_t LowMask = static_cast<uint16_t>(MaskValue);
            uint16_t HighMask = static_cast<uint16_t>(MaskValue >> 16);
            if ((LowMask == 0) != (HighMask == 0)) {
              SDLoc DL(Node);
              SDValue Ops[] = {
                  Source, CurDAG->getTargetConstant(MaskValue, DL, MVT::i32),
                  CurDAG->getTargetConstant(getTargetCC(CC), DL, MVT::i16),
                  Node->getOperand(4), Node->getOperand(0)};
              SDNode *Selected = CurDAG->getMachineNode(
                  C166::MASK32BR, DL, MVT::i32, MVT::Other, Ops);
              CurDAG->ReplaceAllUsesOfValueWith(SDValue(Node, 0),
                                                SDValue(Selected, 1));
              CurDAG->RemoveDeadNode(Node);
              return;
            }
          }
        }
      }
      bool IsWordOr =
          LHS.getOpcode() == ISD::OR ||
          (LHS.isMachineOpcode() && LHS.getMachineOpcode() == C166::ORrr);
      if ((CC == ISD::SETEQ || CC == ISD::SETNE) && Immediate == 0 &&
          LHS.getValueType() == MVT::i16 && IsWordOr) {
        SDLoc DL(Node);
        SDNode *DeadOr = LHS.getNode();
        SDValue Ops[] = {
            LHS.getOperand(0), LHS.getOperand(1),
            CurDAG->getTargetConstant(getTargetCC(CC), DL, MVT::i16),
            Node->getOperand(4), Node->getOperand(0)};
        ReplaceNode(
            Node, CurDAG->getMachineNode(C166::TEST32BR, DL, MVT::Other, Ops));
        if (DeadOr->isMachineOpcode() && DeadOr->use_empty())
          CurDAG->RemoveDeadNode(DeadOr);
        return;
      }
      if (Immediate && LHS.getValueType() == MVT::i16) {
        SDLoc DL(Node);
        SDNode *DeadImmediate = RHS.getNode();
        SDValue Ops[] = {
            LHS, CurDAG->getTargetConstant(*Immediate, DL, MVT::i16),
            CurDAG->getTargetConstant(getTargetCC(CC), DL, MVT::i16),
            Node->getOperand(4), Node->getOperand(0)};
        ReplaceNode(Node,
                    CurDAG->getMachineNode(C166::CMPBRi, DL, MVT::Other, Ops));
        if (DeadImmediate->isMachineOpcode() && DeadImmediate->use_empty())
          CurDAG->RemoveDeadNode(DeadImmediate);
        return;
      }
      SDValue Ops[] = {
          LHS, RHS,
          CurDAG->getTargetConstant(getTargetCC(CC), SDLoc(Node), MVT::i16),
          Node->getOperand(4), Node->getOperand(0)};
      unsigned Opcode =
          LHS.getValueType() == MVT::i32 ? C166::CMP32BR : C166::CMPBR;
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
      SDValue Condition = Node->getOperand(1);
      if (isCarryResult(Condition)) {
        SDValue Ops[] = {Condition,
                         CurDAG->getTargetConstant(C166::CC_ULT, DL, MVT::i16),
                         Node->getOperand(2), Node->getOperand(0)};
        ReplaceNode(Node,
                    CurDAG->getMachineNode(C166::FLAGSBR, DL, MVT::Other, Ops));
        return;
      }
      SDValue ZeroImmediate = CurDAG->getTargetConstant(0, DL, MVT::i16);
      SDValue Ops[] = {Condition, ZeroImmediate,
                       CurDAG->getTargetConstant(C166::CC_NE, DL, MVT::i16),
                       Node->getOperand(2), Node->getOperand(0)};
      ReplaceNode(Node,
                  CurDAG->getMachineNode(C166::CMPBRi, DL, MVT::Other, Ops));
      return;
    }

    if (Node->getOpcode() == ISD::SELECT_CC &&
        (Node->getValueType(0) == MVT::i16 ||
         Node->getValueType(0) == MVT::i32)) {
      if (selectInvertedBitBoolean(Node))
        return;

      auto CC = cast<CondCodeSDNode>(Node->getOperand(4))->get();
      SDValue CompareLHS = Node->getOperand(0);
      SDValue CompareRHS = Node->getOperand(1);
      if ((CC == ISD::SETEQ || CC == ISD::SETNE) &&
          CompareLHS.getValueType() == MVT::i32) {
        auto *Zero = dyn_cast<ConstantSDNode>(CompareRHS);
        if (Zero && Zero->isZero()) {
          if (SDValue Word = getWordForZeroCompare(CompareLHS)) {
            SDLoc DL(Node);
            SDValue ZeroWord(CurDAG->getMachineNode(
                                 C166::MOVri4, DL, MVT::i16,
                                 CurDAG->getTargetConstant(0, DL, MVT::i16)),
                             0);
            CompareLHS = Word;
            CompareRHS = ZeroWord;
          }
        }
      }
      SDValue Ops[] = {
          CompareLHS, CompareRHS, Node->getOperand(2), Node->getOperand(3),
          CurDAG->getTargetConstant(getTargetCC(CC), SDLoc(Node), MVT::i16)};
      bool Compare32 = CompareLHS.getValueType() == MVT::i32;
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
      SDValue OffsetValue = Node->getOperand(1);
      ConstantSDNode *Offset = dyn_cast<ConstantSDNode>(OffsetValue);
      if (!Offset && OffsetValue.isMachineOpcode() &&
          (OffsetValue.getMachineOpcode() == C166::MOVri4 ||
           OffsetValue.getMachineOpcode() == C166::MOVri16))
        Offset = dyn_cast<ConstantSDNode>(OffsetValue.getOperand(0));
      if (Offset) {
        SDValue Immediate = CurDAG->getTargetConstant(Offset->getZExtValue(),
                                                      SDLoc(Node), MVT::i16);
        ReplaceNode(Node, CurDAG->getMachineNode(C166::FARADD32i, SDLoc(Node),
                                                 MVT::i32, Node->getOperand(0),
                                                 Immediate));
        if (OffsetValue.isMachineOpcode() && OffsetValue->use_empty())
          CurDAG->RemoveDeadNode(OffsetValue.getNode());
        return;
      }
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

    if (Node->getOpcode() == ISD::OR && Node->getValueType(0) == MVT::i32) {
      auto MatchLowWord = [](SDValue Value) -> SDValue {
        if (Value.getOpcode() == ISD::ZERO_EXTEND &&
            Value.getOperand(0).getValueType() == MVT::i16)
          return Value.getOperand(0);
        if (Value.getOpcode() == ISD::AND) {
          for (unsigned ConstantOperand : {0u, 1u}) {
            auto *Mask =
                dyn_cast<ConstantSDNode>(Value.getOperand(ConstantOperand));
            if (Mask && Mask->getZExtValue() == 0xffff)
              return Value.getOperand(ConstantOperand ^ 1);
          }
        }
        return {};
      };
      auto MatchHighWord = [](SDValue Value) -> SDValue {
        auto UnwrapExtendedWord = [](SDValue Word) -> SDValue {
          if (Word.getOpcode() != C166ISD::LOWORD)
            return Word;
          SDValue Wide = Word.getOperand(0);
          if ((Wide.getOpcode() == ISD::ANY_EXTEND ||
               Wide.getOpcode() == ISD::ZERO_EXTEND ||
               Wide.getOpcode() == ISD::SIGN_EXTEND) &&
              Wide.getOperand(0).getValueType() == MVT::i16)
            return Wide.getOperand(0);
          return Word;
        };

        // LowerI32Shift represents a left shift by one word as {0, high}.
        if (Value.getOpcode() == ISD::BUILD_PAIR) {
          auto *Low = dyn_cast<ConstantSDNode>(Value.getOperand(0));
          if (Low && Low->isZero())
            return UnwrapExtendedWord(Value.getOperand(1));
          return {};
        }

        if (Value.getOpcode() == ISD::SHL) {
          auto *Amount = dyn_cast<ConstantSDNode>(Value.getOperand(1));
          SDValue Extended = Value.getOperand(0);
          if (Amount && Amount->getZExtValue() == 16 &&
              Extended.getOpcode() == ISD::ZERO_EXTEND &&
              Extended.getOperand(0).getValueType() == MVT::i16)
            return Extended.getOperand(0);
        }
        return {};
      };

      for (unsigned LowOperand : {0u, 1u}) {
        SDValue LowWide = MatchLowWord(Node->getOperand(LowOperand));
        SDValue High = MatchHighWord(Node->getOperand(LowOperand ^ 1));
        if (!LowWide || !High)
          continue;
        SDLoc DL(Node);
        SDValue Low = LowWide.getValueType() == MVT::i16
                          ? LowWide
                          : CurDAG->getTargetExtractSubreg(sub_lo16, DL,
                                                           MVT::i16, LowWide);
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

    if ((Node->getOpcode() == ISD::ADD || Node->getOpcode() == ISD::SUB ||
         Node->getOpcode() == ISD::XOR || Node->getOpcode() == ISD::AND ||
         Node->getOpcode() == ISD::OR) &&
        Node->getValueType(0) == MVT::i32) {
      auto GetConstant = [](SDValue Value) -> const ConstantSDNode * {
        if (auto *Constant = dyn_cast<ConstantSDNode>(Value))
          return Constant;
        if (Value.isMachineOpcode() &&
            Value.getMachineOpcode() == C166::CONST32)
          return dyn_cast<ConstantSDNode>(Value.getOperand(0));
        return nullptr;
      };

      SDValue Lhs = Node->getOperand(0);
      const ConstantSDNode *Constant = GetConstant(Node->getOperand(1));
      if (!Constant && Node->getOpcode() != ISD::SUB) {
        Constant = GetConstant(Node->getOperand(0));
        Lhs = Node->getOperand(1);
      }

      if (Constant) {
        uint32_t Value = static_cast<uint32_t>(Constant->getZExtValue());
        unsigned Opcode;
        if (Node->getOpcode() == ISD::ADD || Node->getOpcode() == ISD::SUB) {
          uint32_t Delta = Node->getOpcode() == ISD::SUB ? 0U - Value : Value;
          auto ImmediateCost = [](uint32_t Immediate) {
            auto WordCost = [](uint16_t Word) { return Word <= 7 ? 2 : 4; };
            return WordCost(static_cast<uint16_t>(Immediate)) +
                   WordCost(static_cast<uint16_t>(Immediate >> 16));
          };
          uint32_t Subtrahend = 0U - Delta;
          bool UseSub = ImmediateCost(Subtrahend) < ImmediateCost(Delta);
          Opcode = UseSub ? C166::SUB32ri : C166::ADD32ri;
          Value = UseSub ? Subtrahend : Delta;
        } else if (Node->getOpcode() == ISD::XOR) {
          Opcode = C166::XOR32ri;
        } else if (Node->getOpcode() == ISD::AND) {
          Opcode = C166::AND32ri;
        } else {
          Opcode = C166::OR32ri;
        }

        SDValue Immediate =
            CurDAG->getTargetConstant(Value, SDLoc(Node), MVT::i32);
        ReplaceNode(Node, CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i32,
                                                 Lhs, Immediate));
        return;
      }
    }

    if ((Node->getOpcode() == ISD::SHL || Node->getOpcode() == ISD::SRL ||
         Node->getOpcode() == ISD::SRA) &&
        Node->getValueType(0) == MVT::i32) {
      if (auto *Amount = dyn_cast<ConstantSDNode>(Node->getOperand(1));
          Amount && Amount->getZExtValue() >= 1 &&
          Amount->getZExtValue() <= 31) {
        unsigned Opcode;
        if (Node->getOpcode() == ISD::SHL && Amount->getZExtValue() <= 2)
          Opcode = C166::SHL32ri4;
        else if (Node->getOpcode() == ISD::SHL)
          Opcode = C166::SHL32ri5;
        else if (Node->getOpcode() == ISD::SRL && Amount->getZExtValue() == 1)
          Opcode = C166::SRL32ri1;
        else if (Node->getOpcode() == ISD::SRA && Amount->getZExtValue() == 1)
          Opcode = C166::SRA32ri1;
        else if (Node->getOpcode() == ISD::SRL)
          Opcode = C166::SRL32ri5;
        else
          Opcode = C166::SRA32ri5;
        if (Opcode == C166::SRL32ri1 || Opcode == C166::SRA32ri1) {
          ReplaceNode(Node,
                      CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i32,
                                             Node->getOperand(0)));
          return;
        }
        SDValue TargetAmount = CurDAG->getTargetConstant(Amount->getZExtValue(),
                                                         SDLoc(Node), MVT::i16);
        if (Opcode == C166::SHL32ri4) {
          ReplaceNode(
              Node, CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i32,
                                           Node->getOperand(0), TargetAmount));
          return;
        }
        SDNode *Shift =
            CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i32, MVT::i16,
                                   Node->getOperand(0), TargetAmount);
        CurDAG->ReplaceAllUsesOfValueWith(SDValue(Node, 0), SDValue(Shift, 0));
        CurDAG->RemoveDeadNode(Node);
        return;
      }
    }

    if (Node->getOpcode() == ISD::ROTL && Node->getValueType(0) == MVT::i32) {
      if (auto *Amount = dyn_cast<ConstantSDNode>(Node->getOperand(1));
          Amount && Amount->getZExtValue() >= 1 &&
          Amount->getZExtValue() <= 31) {
        SDLoc DL(Node);
        if (Amount->getZExtValue() == 16) {
          SDValue Value = Node->getOperand(0);
          SDValue Low =
              CurDAG->getTargetExtractSubreg(sub_hi16, DL, MVT::i16, Value);
          SDValue High =
              CurDAG->getTargetExtractSubreg(sub_lo16, DL, MVT::i16, Value);
          SDValue Ops[] = {
              CurDAG->getTargetConstant(C166::GR32RegClassID, DL, MVT::i32),
              Low, CurDAG->getTargetConstant(sub_lo16, DL, MVT::i32), High,
              CurDAG->getTargetConstant(sub_hi16, DL, MVT::i32)};
          ReplaceNode(Node, CurDAG->getMachineNode(TargetOpcode::REG_SEQUENCE,
                                                   DL, MVT::i32, Ops));
          return;
        }
        SDValue TargetAmount =
            CurDAG->getTargetConstant(Amount->getZExtValue(), DL, MVT::i16);
        SDNode *Rotate =
            CurDAG->getMachineNode(C166::ROTL32ri5, DL, MVT::i32, MVT::i16,
                                   Node->getOperand(0), TargetAmount);
        CurDAG->ReplaceAllUsesOfValueWith(SDValue(Node, 0), SDValue(Rotate, 0));
        CurDAG->RemoveDeadNode(Node);
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

    if (Node->getOpcode() == C166ISD::TAILCALL ||
        Node->getOpcode() == C166ISD::NEARTAILCALL) {
      const bool IsNear = Node->getOpcode() == C166ISD::NEARTAILCALL;
      SmallVector<SDValue, 10> Ops;
      unsigned FirstVariableOperand;
      if (IsNear) {
        Ops.push_back(Node->getOperand(1));
        FirstVariableOperand = 2;
      } else {
        Ops.push_back(Node->getOperand(1));
        Ops.push_back(Node->getOperand(2));
        FirstVariableOperand = 3;
      }
      unsigned Last = Node->getNumOperands();
      SDValue Glue;
      if (Node->getOperand(Last - 1).getValueType() == MVT::Glue)
        Glue = Node->getOperand(--Last);
      for (unsigned I = FirstVariableOperand; I != Last; ++I)
        Ops.push_back(Node->getOperand(I));
      Ops.push_back(Node->getOperand(0));
      if (Glue)
        Ops.push_back(Glue);
      ReplaceNode(
          Node, CurDAG->getMachineNode(IsNear ? C166::TAILJMPA : C166::TAILJMPS,
                                       SDLoc(Node), MVT::Other, Ops));
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

    if (Node->getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
        isa<ConstantSDNode>(Node->getOperand(0)) &&
        cast<ConstantSDNode>(Node->getOperand(0))->getZExtValue() ==
            Intrinsic::c166_divlu) {
      ReplaceNode(Node, CurDAG->getMachineNode(C166::UDIVREM32_16, SDLoc(Node),
                                               MVT::i32, Node->getOperand(1),
                                               Node->getOperand(2)));
      return;
    }

    if (Node->getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
        isa<ConstantSDNode>(Node->getOperand(0)) &&
        cast<ConstantSDNode>(Node->getOperand(0))->getZExtValue() ==
            Intrinsic::c166_va_start) {
      const C166MachineFunctionInfo *FuncInfo =
          CurDAG->getMachineFunction().getInfo<C166MachineFunctionInfo>();
      assert(FuncInfo->hasVarArgsFrameIndex() &&
             "C166 va_start used outside a variadic function");
      SDLoc DL(Node);
      SDValue FI = CurDAG->getTargetFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                               MVT::i32);
      SDValue Offset = CurDAG->getTargetConstant(0, DL, MVT::i16);
      unsigned Opcode =
          Node->getValueType(0) == MVT::i16 ? C166::LEAfi : C166::FRAMEADDR32;
      ReplaceNode(Node, CurDAG->getMachineNode(
                            Opcode, DL, Node->getValueType(0), FI, Offset));
      return;
    }

    if (Node->getOpcode() == ISD::INTRINSIC_WO_CHAIN &&
        isa<ConstantSDNode>(Node->getOperand(0)) &&
        cast<ConstantSDNode>(Node->getOperand(0))->getZExtValue() ==
            Intrinsic::c166_stack_address) {
      SDValue FrameIndex;
      SDValue Offset;
      SDValue Address = Node->getOperand(1);
      if (selectFrameAddress(Address, FrameIndex, Offset)) {
        ReplaceNode(Node, CurDAG->getMachineNode(C166::LEAfi, SDLoc(Node),
                                                 MVT::i16, FrameIndex, Offset));
        return;
      }

      // Fixed arguments can cross a basic-block boundary after formal-argument
      // lowering.  Their frame address then arrives through a virtual i32
      // register rather than as a FrameIndex.  Recover the logical DPP1 near
      // address from the offset word of that stack pointer.
      if (Address.getValueType() == MVT::i32) {
        SDLoc DL(Node);
        SDValue Low =
            CurDAG->getTargetExtractSubreg(sub_lo16, DL, MVT::i16, Address);
        SDValue PageSelector = CurDAG->getTargetConstant(0x4000, DL, MVT::i16);
        ReplaceNode(Node, CurDAG->getMachineNode(C166::ORri16, DL, MVT::i16,
                                                 Low, PageSelector));
        return;
      }

      assert(Address.getValueType() == MVT::i16 &&
             "unexpected C166 stack pointer type");
      CurDAG->ReplaceAllUsesOfValueWith(SDValue(Node, 0), Address);
      CurDAG->RemoveDeadNode(Node);
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
        Load && Load->getAddressingMode() == ISD::UNINDEXED &&
        Load->getMemoryVT() == MVT::i8 &&
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

      SDValue NearAddress;
      if (selectIndexedFrameAddress(Load->getBasePtr(), NearAddress)) {
        SDValue Zero = CurDAG->getTargetConstant(0, SDLoc(Node), MVT::i16);
        unsigned Opcode = IsSigned ? C166::NEARLOAD8S : C166::NEARLOAD8Z;
        SDNode *Selected =
            CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16, MVT::Other,
                                   NearAddress, Zero, Load->getChain());
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
        SDValue Base;
        SDValue Offset;
        selectNearAddress(Load->getBasePtr(), Base, Offset);
        unsigned Opcode = IsSigned ? C166::NEARLOAD8S : C166::NEARLOAD8Z;
        SDNode *Selected =
            CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16, MVT::Other,
                                   Base, Offset, Load->getChain());
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
        SDValue Base = Load->getBasePtr();
        SmallVector<SDValue, 3> Ops = {Base};
        if (!IsSegmented) {
          SDValue Offset;
          selectFarAddress(Base, Base, Offset);
          Ops[0] = Base;
          Ops.push_back(Offset);
        }
        Ops.push_back(Load->getChain());
        SDNode *Selected = CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16,
                                                  MVT::Other, Ops);
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
        Load->getMemoryVT() == MVT::i32) {
      SDValue FrameIndex;
      SDValue Offset;
      if (selectFrameAddress(Load->getBasePtr(), FrameIndex, Offset)) {
        SDNode *Selected = CurDAG->getMachineNode(
            C166::FRAMELOAD32, SDLoc(Node), MVT::i32, MVT::Other, FrameIndex,
            Offset, Load->getChain());
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Load->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
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
      SDValue Base;
      SDValue Offset;
      selectNearAddress(Load->getBasePtr(), Base, Offset);
      SDNode *Selected =
          CurDAG->getMachineNode(C166::NEARLOAD32, SDLoc(Node), MVT::i32,
                                 MVT::Other, Base, Offset, Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i32 &&
        Load->getBasePtr().getValueType() == MVT::i32) {
      SDValue NearAddress;
      if (selectIndexedFrameAddress(Load->getBasePtr(), NearAddress)) {
        SDValue Zero = CurDAG->getTargetConstant(0, SDLoc(Node), MVT::i16);
        SDNode *Selected = CurDAG->getMachineNode(
            C166::NEARLOAD32, SDLoc(Node), MVT::i32, MVT::Other, NearAddress,
            Zero, Load->getChain());
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Load->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
      if (Load->getAddressSpace() == C166::HugeDataAddressSpace) {
        SDLoc DL(Node);
        SDValue Base = Load->getBasePtr();
        SDValue HighAddress(
            CurDAG->getMachineNode(C166::ADD32ri, DL, MVT::i32, Base,
                                   CurDAG->getTargetConstant(2, DL, MVT::i32)),
            0);
        SDNode *Low = CurDAG->getMachineNode(
            C166::SEGLOAD16, DL, MVT::i16, MVT::Other, Base, Load->getChain());
        SDNode *High =
            CurDAG->getMachineNode(C166::SEGLOAD16, DL, MVT::i16, MVT::Other,
                                   HighAddress, SDValue(Low, 1));
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Low),
                               {Load->getMemOperand()});
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(High),
                               {Load->getMemOperand()});
        SDValue Ops[] = {
            CurDAG->getTargetConstant(C166::GR32RegClassID, DL, MVT::i32),
            SDValue(Low, 0),
            CurDAG->getTargetConstant(sub_lo16, DL, MVT::i32),
            SDValue(High, 0),
            CurDAG->getTargetConstant(sub_hi16, DL, MVT::i32),
        };
        SDValue Pair(CurDAG->getMachineNode(TargetOpcode::REG_SEQUENCE, DL,
                                            MVT::i32, Ops),
                     0);
        CurDAG->ReplaceAllUsesOfValueWith(SDValue(Node, 0), Pair);
        CurDAG->ReplaceAllUsesOfValueWith(SDValue(Node, 1), SDValue(High, 1));
        CurDAG->RemoveDeadNode(Node);
        return;
      }
      unsigned Opcode = Load->getAddressSpace() == C166::SHugeDataAddressSpace
                            ? C166::SHUGELOAD32
                            : C166::FARLOAD32;
      SDValue Base = Load->getBasePtr();
      SmallVector<SDValue, 3> Ops = {Base};
      if (Opcode == C166::FARLOAD32) {
        SDValue Offset;
        selectFarAddress(Base, Base, Offset);
        Ops[0] = Base;
        Ops.push_back(Offset);
      }
      Ops.push_back(Load->getChain());
      SDNode *Selected = CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i32,
                                                MVT::Other, Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Store = dyn_cast<StoreSDNode>(Node);
        Store && Store->getMemoryVT() == MVT::i8 &&
        Store->getBasePtr().getValueType() == MVT::i16 &&
        !isNearGlobalAddress(Store->getBasePtr())) {
      SDValue Base;
      SDValue Offset;
      selectNearAddress(Store->getBasePtr(), Base, Offset);
      SDValue Ops[] = {Base, Offset, Store->getValue(), Store->getChain()};
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
      SDValue NearAddress;
      if (selectIndexedFrameAddress(Store->getBasePtr(), NearAddress)) {
        SDValue Zero = CurDAG->getTargetConstant(0, SDLoc(Node), MVT::i16);
        SDValue Ops[] = {NearAddress, Zero, Store->getValue(),
                         Store->getChain()};
        SDNode *Selected = CurDAG->getMachineNode(C166::NEARSTORE8, SDLoc(Node),
                                                  MVT::Other, Ops);
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
      bool IsSegmented =
          Store->getAddressSpace() == C166::HugeDataAddressSpace ||
          Store->getAddressSpace() == C166::SHugeDataAddressSpace;
      unsigned Opcode = IsSegmented ? C166::SEGSTORE8 : C166::FARSTORE8;
      SDValue Base = Store->getBasePtr();
      SmallVector<SDValue, 4> Ops = {Base};
      if (!IsSegmented) {
        SDValue Offset;
        selectFarAddress(Base, Base, Offset);
        Ops[0] = Base;
        Ops.push_back(Offset);
      }
      Ops.push_back(Store->getValue());
      Ops.push_back(Store->getChain());
      SDNode *Selected =
          CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::Other, Ops);
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
      SDValue Base;
      SDValue Offset;
      selectNearAddress(Store->getBasePtr(), Base, Offset);
      SDValue Ops[] = {Base, Offset, Store->getValue(), Store->getChain()};
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
      SDValue FrameIndex;
      SDValue Offset;
      if (selectFrameAddress(Store->getBasePtr(), FrameIndex, Offset)) {
        SDValue Ops[] = {FrameIndex, Offset, Store->getValue(),
                         Store->getChain()};
        SDNode *Selected = CurDAG->getMachineNode(C166::FRAMESTORE32,
                                                  SDLoc(Node), MVT::Other, Ops);
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Store->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
      SDValue NearAddress;
      if (selectIndexedFrameAddress(Store->getBasePtr(), NearAddress)) {
        SDValue Zero = CurDAG->getTargetConstant(0, SDLoc(Node), MVT::i16);
        SDValue Ops[] = {NearAddress, Zero, Store->getValue(),
                         Store->getChain()};
        SDNode *Selected = CurDAG->getMachineNode(C166::NEARSTORE32,
                                                  SDLoc(Node), MVT::Other, Ops);
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                               {Store->getMemOperand()});
        ReplaceNode(Node, Selected);
        return;
      }
      if (Store->getAddressSpace() == C166::HugeDataAddressSpace) {
        SDLoc DL(Node);
        SDValue Base = Store->getBasePtr();
        SDValue Value = Store->getValue();
        auto ExtractWord = [&](unsigned SubReg) {
          if (Value.getOpcode() == ISD::BUILD_PAIR)
            return Value.getOperand(SubReg == sub_lo16 ? 0 : 1);
          SDValue SubRegIndex = CurDAG->getTargetConstant(SubReg, DL, MVT::i32);
          return SDValue(CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG,
                                                DL, MVT::i16, Value,
                                                SubRegIndex),
                         0);
        };
        SDValue HighAddress(
            CurDAG->getMachineNode(C166::ADD32ri, DL, MVT::i32, Base,
                                   CurDAG->getTargetConstant(2, DL, MVT::i32)),
            0);
        SDValue LowOps[] = {Base, ExtractWord(sub_lo16), Store->getChain()};
        SDNode *Low =
            CurDAG->getMachineNode(C166::SEGSTORE16, DL, MVT::Other, LowOps);
        SDValue HighOps[] = {HighAddress, ExtractWord(sub_hi16),
                             SDValue(Low, 0)};
        SDNode *High =
            CurDAG->getMachineNode(C166::SEGSTORE16, DL, MVT::Other, HighOps);
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Low),
                               {Store->getMemOperand()});
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(High),
                               {Store->getMemOperand()});
        ReplaceNode(Node, High);
        return;
      }
      unsigned Opcode = Store->getAddressSpace() == C166::SHugeDataAddressSpace
                            ? C166::SHUGESTORE32
                            : C166::FARSTORE32;
      SDValue Base = Store->getBasePtr();
      SmallVector<SDValue, 4> Ops = {Base};
      if (Opcode == C166::FARSTORE32) {
        SDValue Offset;
        selectFarAddress(Base, Base, Offset);
        Ops[0] = Base;
        Ops.push_back(Offset);
      }
      Ops.push_back(Store->getValue());
      Ops.push_back(Store->getChain());
      SDNode *Selected =
          CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::Other, Ops);
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
      SDValue Base;
      SDValue Offset;
      selectNearAddress(Store->getBasePtr(), Base, Offset);
      auto *Displacement = cast<ConstantSDNode>(Offset);
      unsigned Opcode = Displacement->isZero() ? C166::MOVmr : C166::MOVmr16;
      SmallVector<SDValue, 4> Ops = {Base};
      if (!Displacement->isZero())
        Ops.push_back(Offset);
      Ops.push_back(StoreValue);
      Ops.push_back(Store->getChain());
      SDNode *Selected =
          CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::Other, Ops);
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
      SDValue NearAddress;
      if (selectIndexedFrameAddress(Store->getBasePtr(), NearAddress)) {
        SDNode *Selected =
            CurDAG->getMachineNode(C166::MOVmr, SDLoc(Node), MVT::Other,
                                   NearAddress, StoreValue, Store->getChain());
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
      SDValue Base = Store->getBasePtr();
      SmallVector<SDValue, 4> Ops = {Base};
      if (Opcode == C166::FARSTORE16) {
        SDValue Offset;
        selectFarAddress(Base, Base, Offset);
        Ops[0] = Base;
        Ops.push_back(Offset);
      }
      Ops.push_back(StoreValue);
      Ops.push_back(Store->getChain());
      SDNode *Selected =
          CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::Other, Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Store->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getAddressingMode() == ISD::POST_INC &&
        Load->getMemoryVT() == MVT::i8 && Load->getValueType(0) == MVT::i16 &&
        Load->getAddressSpace() == C166::FarDataAddressSpace &&
        Load->getOffset()->getAsZExtVal() == 1) {
      unsigned Opcode = Load->getExtensionType() == ISD::SEXTLOAD
                            ? C166::FARLOAD8S_POSTINC
                            : C166::FARLOAD8Z_POSTINC;
      SDNode *Selected = CurDAG->getMachineNode(
          Opcode, SDLoc(Node), MVT::i16, MVT::i32, MVT::Other,
          Load->getBasePtr(), Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getAddressingMode() == ISD::POST_INC &&
        Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i16 &&
        Load->getAddressSpace() == C166::FarDataAddressSpace &&
        Load->getOffset()->getAsZExtVal() == 2) {
      SDNode *Selected = CurDAG->getMachineNode(
          C166::FARLOAD16_POSTINC, SDLoc(Node), MVT::i16, MVT::i32, MVT::Other,
          Load->getBasePtr(), Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getAddressingMode() == ISD::POST_INC &&
        Load->getMemoryVT() == MVT::i8 && Load->getValueType(0) == MVT::i16 &&
        (Load->getAddressSpace() == C166::NearAddressSpace ||
         Load->getAddressSpace() == C166::XNearDataAddressSpace) &&
        Load->getOffset()->getAsZExtVal() == 1) {
      unsigned Opcode = Load->getExtensionType() == ISD::SEXTLOAD
                            ? C166::NEARLOAD8S_POSTINC
                            : C166::NEARLOAD8Z_POSTINC;
      SDNode *Selected = CurDAG->getMachineNode(
          Opcode, SDLoc(Node), MVT::i16, MVT::i16, MVT::Other,
          Load->getBasePtr(), Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
      ReplaceNode(Node, Selected);
      return;
    }

    if (auto *Load = dyn_cast<LoadSDNode>(Node);
        Load && Load->getAddressingMode() == ISD::POST_INC &&
        Load->getExtensionType() == ISD::NON_EXTLOAD &&
        Load->getMemoryVT() == MVT::i16 &&
        (Load->getAddressSpace() == C166::NearAddressSpace ||
         Load->getAddressSpace() == C166::XNearDataAddressSpace) &&
        Load->getOffset()->getAsZExtVal() == 2) {
      SDNode *Selected = CurDAG->getMachineNode(
          C166::MOVrmPostInc, SDLoc(Node), MVT::i16, MVT::i16, MVT::Other,
          Load->getBasePtr(), Load->getChain());
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Selected),
                             {Load->getMemOperand()});
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

      SDValue NearAddress;
      if (selectIndexedFrameAddress(Load->getBasePtr(), NearAddress)) {
        SDNode *Selected =
            CurDAG->getMachineNode(C166::MOVrm, SDLoc(Node), MVT::i16,
                                   MVT::Other, NearAddress, Load->getChain());
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
        SDValue Base;
        SDValue Offset;
        selectNearAddress(Load->getBasePtr(), Base, Offset);
        auto *Displacement = cast<ConstantSDNode>(Offset);
        unsigned Opcode = Displacement->isZero() ? C166::MOVrm : C166::MOVrm16;
        SmallVector<SDValue, 3> Ops = {Base};
        if (!Displacement->isZero())
          Ops.push_back(Offset);
        Ops.push_back(Load->getChain());
        SDNode *Selected = CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16,
                                                  MVT::Other, Ops);
        replaceExtendingLoad(Load, Selected, IsSigned);
        return;
      }

      if (Load->getBasePtr().getValueType() == MVT::i32) {
        bool IsSegmented =
            Load->getAddressSpace() == C166::HugeDataAddressSpace ||
            Load->getAddressSpace() == C166::SHugeDataAddressSpace;
        unsigned Opcode = IsSegmented ? C166::SEGLOAD16 : C166::FARLOAD16;
        SDValue Base = Load->getBasePtr();
        SmallVector<SDValue, 3> Ops = {Base};
        if (!IsSegmented) {
          SDValue Offset;
          selectFarAddress(Base, Base, Offset);
          Ops[0] = Base;
          Ops.push_back(Offset);
        }
        Ops.push_back(Load->getChain());
        SDNode *Selected = CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16,
                                                  MVT::Other, Ops);
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
      SDValue Base;
      SDValue Offset;
      selectNearAddress(Load->getBasePtr(), Base, Offset);
      auto *Displacement = cast<ConstantSDNode>(Offset);
      unsigned Opcode = Displacement->isZero() ? C166::MOVrm : C166::MOVrm16;
      SmallVector<SDValue, 3> Ops = {Base};
      if (!Displacement->isZero())
        Ops.push_back(Offset);
      Ops.push_back(Load->getChain());
      SDNode *Selected = CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16,
                                                MVT::Other, Ops);
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
      SDValue NearAddress;
      if (selectIndexedFrameAddress(Load->getBasePtr(), NearAddress)) {
        SDNode *Selected =
            CurDAG->getMachineNode(C166::MOVrm, SDLoc(Node), MVT::i16,
                                   MVT::Other, NearAddress, Load->getChain());
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
      SDValue Base = Load->getBasePtr();
      SmallVector<SDValue, 3> Ops = {Base};
      if (Opcode == C166::FARLOAD16) {
        SDValue Offset;
        selectFarAddress(Base, Base, Offset);
        Ops[0] = Base;
        Ops.push_back(Offset);
      }
      Ops.push_back(Load->getChain());
      SDNode *Selected = CurDAG->getMachineNode(Opcode, SDLoc(Node), MVT::i16,
                                                MVT::Other, Ops);
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
        if (Source.getOpcode() == C166ISD::LOWORD)
          return Self(Self, Source.getOperand(0), sub_lo16);
        if (Source.getOpcode() == C166ISD::HIWORD)
          return Self(Self, Source.getOperand(0), sub_hi16);
        if (Source.getValueType() == MVT::i16)
          return Source;
        if (Source.getOpcode() == ISD::BUILD_PAIR)
          return Self(Self, Source.getOperand(SubReg == sub_lo16 ? 0 : 1),
                      sub_lo16);
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
      auto *High = dyn_cast<ConstantSDNode>(Node->getOperand(1));
      if (High && High->isZero() && Node->hasOneUse()) {
        SDNode *User = Node->use_begin()->getUser();
        bool FeedsAdd = User->getOpcode() == ISD::ADD ||
                        (User->isMachineOpcode() &&
                         User->getMachineOpcode() == C166::ADD32rr);
        if (FeedsAdd) {
          ReplaceNode(
              Node, extendWordToI32(DL, Node->getOperand(0), false).getNode());
          return;
        }
      }
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
      ReplaceNode(Node, CurDAG->getMachineNode(C166::FRAMEADDR32, DL, MVT::i32,
                                               TargetFI, Offset));
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
