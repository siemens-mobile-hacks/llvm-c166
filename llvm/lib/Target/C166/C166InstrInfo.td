//===-- C166InstrInfo.td - C166 instruction definitions --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

def UImm3AsmOperand : AsmOperandClass {
  let Name = "UImm3";
  let DiagnosticType = "InvalidUImm3";
  let RenderMethod = "addImmOperands";
}

def UImm4AsmOperand : AsmOperandClass {
  let Name = "UImm4";
  let DiagnosticType = "InvalidUImm4";
  let RenderMethod = "addImmOperands";
}

def UImm7AsmOperand : AsmOperandClass {
  let Name = "UImm7";
  let DiagnosticType = "InvalidUImm7";
  let RenderMethod = "addImmOperands";
}

def UImm8AsmOperand : AsmOperandClass {
  let Name = "UImm8";
  let DiagnosticType = "InvalidUImm8";
  let RenderMethod = "addImmOperands";
}

def UImm8ALUAsmOperand : AsmOperandClass {
  let Name = "UImm8ALU";
  let PredicateMethod = "isUImm8ALU";
  let DiagnosticType = "InvalidUImm8ALU";
  let RenderMethod = "addImmOperands";
}

def ByteImmFullAsmOperand : AsmOperandClass {
  let Name = "ByteImmFull";
  let DiagnosticType = "InvalidUImm8ALU";
  let RenderMethod = "addImmOperands";
}

def ByteImmediateAsmOperand : AsmOperandClass {
  let Name = "ByteImmediate";
  let DiagnosticType = "InvalidUImm8ALU";
  let RenderMethod = "addImmOperands";
}

def ExtensionSegment8AsmOperand : AsmOperandClass {
  let Name = "ExtensionSegment8";
  let DiagnosticType = "InvalidSeg8";
  let RenderMethod = "addImmOperands";
}

def UImm16AsmOperand : AsmOperandClass {
  let Name = "UImm16";
  let DiagnosticType = "InvalidUImm16";
  let RenderMethod = "addImmOperands";
}

def Immediate16AsmOperand : AsmOperandClass {
  let Name = "Immediate16";
  let DiagnosticType = "InvalidUImm16";
  let RenderMethod = "addImmOperands";
}

def Word16AsmOperand : AsmOperandClass {
  let Name = "Word16";
  let DiagnosticType = "InvalidUImm16";
  let RenderMethod = "addImmOperands";
}

def Memory16AsmOperand : AsmOperandClass {
  let Name = "Memory16";
  let DiagnosticType = "InvalidAbs16";
  let RenderMethod = "addImmOperands";
}

def UImm16LargeAsmOperand : AsmOperandClass {
  let Name = "UImm16Large";
  let PredicateMethod = "isUImm16Large";
  let DiagnosticType = "InvalidUImm16Large";
  let RenderMethod = "addImmOperands";
}

def UImm16ALUAsmOperand : AsmOperandClass {
  let Name = "UImm16ALU";
  let PredicateMethod = "isUImm16ALU";
  let DiagnosticType = "InvalidUImm16ALU";
  let RenderMethod = "addImmOperands";
}

def SequenceCountAsmOperand : AsmOperandClass {
  let Name = "SequenceCount";
  let PredicateMethod = "isSequenceCount";
  let DiagnosticType = "InvalidSequenceCount";
  let RenderMethod = "addImmOperands";
}

def AtomicCountAsmOperand : AsmOperandClass {
  let Name = "AtomicCount";
  let PredicateMethod = "isAtomicCount";
  let DiagnosticType = "InvalidAtomicCount";
  let RenderMethod = "addImmOperands";
}

def BitAddressAsmOperand : AsmOperandClass {
  let Name = "BitAddress";
  let PredicateMethod = "isBitAddress";
  let DiagnosticType = "InvalidBitAddress";
  let RenderMethod = "addImmOperands";
}

def BitOffsetAsmOperand : AsmOperandClass {
  let Name = "BitOffset";
  let PredicateMethod = "isBitOffset";
  let DiagnosticType = "InvalidBitOffset";
  let RenderMethod = "addBitOffsetOperands";
}

def Seg8AsmOperand : AsmOperandClass {
  let Name = "Seg8";
  let DiagnosticType = "InvalidSeg8";
  let RenderMethod = "addImmOperands";
}

def Sof16AsmOperand : AsmOperandClass {
  let Name = "Sof16";
  let DiagnosticType = "InvalidSof16";
  let RenderMethod = "addImmOperands";
}

def Cof16AsmOperand : AsmOperandClass {
  let Name = "Cof16";
  let PredicateMethod = "isCof16";
  let DiagnosticType = "InvalidCof16";
  let RenderMethod = "addImmOperands";
}

def Pag10AsmOperand : AsmOperandClass {
  let Name = "Pag10";
  let DiagnosticType = "InvalidPag10";
  let RenderMethod = "addImmOperands";
}

def Pof14AsmOperand : AsmOperandClass {
  let Name = "Pof14";
  let DiagnosticType = "InvalidPof14";
  let RenderMethod = "addImmOperands";
}

def Abs16AsmOperand : AsmOperandClass {
  let Name = "Abs16";
  let PredicateMethod = "isAbs16";
  let DiagnosticType = "InvalidAbs16";
  let RenderMethod = "addImmOperands";
}

def DPP1AddressAsmOperand : AsmOperandClass {
  let Name = "DPP1Address";
  let PredicateMethod = "isDPP1Address";
  let DiagnosticType = "InvalidPof14";
  let RenderMethod = "addImmOperands";
}

def DPP2AddressAsmOperand : AsmOperandClass {
  let Name = "DPP2Address";
  let PredicateMethod = "isDPP2Address";
  let DiagnosticType = "InvalidPof14";
  let RenderMethod = "addImmOperands";
}

def BrTargetAsmOperand : AsmOperandClass {
  let Name = "BrTarget";
  let PredicateMethod = "isBrTarget";
  let DiagnosticType = "InvalidBrTarget";
  let RenderMethod = "addImmOperands";
}

def uimm3 : Operand<i16>, ImmLeaf<i16, [{ return isUInt<3>(Imm); }]> {
  let ParserMatchClass = UImm3AsmOperand;
  let PrintMethod = "printImmediate";
}

def uimm4 : Operand<i16>, ImmLeaf<i16, [{ return isUInt<4>(Imm); }]> {
  let ParserMatchClass = UImm4AsmOperand;
  let PrintMethod = "printImmediate";
}

def uimm5 : Operand<i16>, ImmLeaf<i16, [{ return isUInt<5>(Imm); }]> {
  let PrintMethod = "printImmediate";
}

def SDT_C166Ret : SDTypeProfile<0, -1, []>;
def C166ret : SDNode<"C166ISD::RET", SDT_C166Ret,
                     [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def C166nearret : SDNode<"C166ISD::NEARRET", SDT_C166Ret,
                         [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def C166interruptret : SDNode<"C166ISD::INTERRUPTRET", SDT_C166Ret,
                              [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

def SDT_C166Call
    : SDTypeProfile<0, -1, [SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_C166CallSeqStart
    : SDCallSeqStart<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_C166CallSeqEnd
    : SDCallSeqEnd<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def C166callseq_start
    : SDNode<"ISD::CALLSEQ_START", SDT_C166CallSeqStart,
             [SDNPHasChain, SDNPOutGlue]>;
def C166callseq_end
    : SDNode<"ISD::CALLSEQ_END", SDT_C166CallSeqEnd,
             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def C166call : SDNode<"C166ISD::CALL", SDT_C166Call,
                      [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                       SDNPVariadic]>;

def SDT_C166NearCall : SDTypeProfile<0, -1, [SDTCisVT<0, i16>]>;
def C166nearcall : SDNode<"C166ISD::NEARCALL", SDT_C166NearCall,
                          [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                           SDNPVariadic]>;

def C166tailcall : SDNode<"C166ISD::TAILCALL", SDT_C166Call,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def C166neartailcall : SDNode<"C166ISD::NEARTAILCALL", SDT_C166NearCall,
                              [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

def SDT_C166PushArg : SDTypeProfile<0, 2, [
  SDTCisVT<0, i16>, SDTCisVT<1, i16>
]>;
def C166pusharg : SDNode<"C166ISD::PUSHARG", SDT_C166PushArg,
                         [SDNPHasChain, SDNPMayStore]>;

def SDT_C166ExtractWord : SDTypeProfile<1, 1, [
  SDTCisVT<0, i16>, SDTCisVT<1, i32>
]>;
def C166lowword : SDNode<"C166ISD::LOWORD", SDT_C166ExtractWord>;
def C166highword : SDNode<"C166ISD::HIWORD", SDT_C166ExtractWord>;

def SDT_C166Shift64 : SDTypeProfile<2, 3, [
  SDTCisVT<0, i32>, SDTCisVT<1, i32>, SDTCisVT<2, i32>,
  SDTCisVT<3, i32>, SDTCisVT<4, i16>
]>;
def C166shl64 : SDNode<"C166ISD::SHL64", SDT_C166Shift64>;

def SDT_C166ShiftRight64ByOne : SDTypeProfile<2, 2, [
  SDTCisVT<0, i32>, SDTCisVT<1, i32>,
  SDTCisVT<2, i32>, SDTCisVT<3, i32>
]>;
def C166srl64_1
    : SDNode<"C166ISD::SRL64_1", SDT_C166ShiftRight64ByOne>;
def C166sra64_1
    : SDNode<"C166ISD::SRA64_1", SDT_C166ShiftRight64ByOne>;

def SDT_C166ShiftRight32ByOne : SDTypeProfile<1, 1, [
  SDTCisVT<0, i32>, SDTCisVT<1, i32>
]>;
def C166srl32_1
    : SDNode<"C166ISD::SRL32_1", SDT_C166ShiftRight32ByOne>;
def C166sra32_1
    : SDNode<"C166ISD::SRA32_1", SDT_C166ShiftRight32ByOne>;

def SDT_C166ShiftRightPairByOne : SDTypeProfile<2, 2, [
  SDTCisVT<0, i16>, SDTCisVT<1, i16>,
  SDTCisVT<2, i16>, SDTCisVT<3, i16>
]>;
def C166srlpair1
    : SDNode<"C166ISD::SRLPAIR1", SDT_C166ShiftRightPairByOne>;
def C166srapair1
    : SDNode<"C166ISD::SRAPAIR1", SDT_C166ShiftRightPairByOne>;

def SDT_C166Mul16 : SDTypeProfile<1, 2, [
  SDTCisVT<0, i32>, SDTCisVT<1, i16>, SDTCisVT<2, i16>
]>;
def C166smul16 : SDNode<"C166ISD::SMUL16", SDT_C166Mul16>;
def C166umul16 : SDNode<"C166ISD::UMUL16", SDT_C166Mul16>;

def SDT_C166UDivRem32By16 : SDTypeProfile<2, 2, [
  SDTCisVT<0, i32>, SDTCisVT<1, i16>,
  SDTCisVT<2, i32>, SDTCisVT<3, i16>
]>;
def C166udivrem32by16
    : SDNode<"C166ISD::UDIVREM32BY16", SDT_C166UDivRem32By16>;

def SDT_C166AdjSP : SDTypeProfile<0, 1, [SDTCisVT<0, i16>]>;
def C166adjsp
    : SDNode<"C166ISD::ADJSP", SDT_C166AdjSP,
             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def SDT_C166AllocSP : SDTypeProfile<0, 2, [
  SDTCisVT<0, i16>, SDTCisVT<1, i16>
]>;
def C166allocsp
    : SDNode<"C166ISD::ALLOCSP", SDT_C166AllocSP, [SDNPHasChain]>;

def SDT_C166NearLoad
    : SDTypeProfile<1, 2, [SDTCisVT<0, i16>, SDTCisSameAs<0, 1>,
                           SDTCisSameAs<0, 2>]>;
def C166nearload : SDNode<"C166ISD::NEARLOAD", SDT_C166NearLoad,
                          [SDNPHasChain, SDNPMayLoad]>;

def SDT_C166FarAdd
    : SDTypeProfile<1, 2, [SDTCisVT<0, i32>, SDTCisSameAs<0, 1>,
                           SDTCisVT<2, i16>]>;
def C166faradd : SDNode<"C166ISD::FARADD", SDT_C166FarAdd>;

// An aggregate returns its caller-reserved user-stack address
// as a raw 16-bit near address in R4. The FrameIndex input follows the
// model's alloca address space and may therefore be i16 or i32.
def SDT_C166FrameAddr
    : SDTypeProfile<1, 1, [SDTCisVT<0, i16>]>;
def C166frameaddr : SDNode<"C166ISD::FRAMEADDR", SDT_C166FrameAddr>;

def uimm7 : Operand<i16> {
  let ParserMatchClass = UImm7AsmOperand;
  let PrintMethod = "printImmediate";
}

def uimm8 : Operand<i16> {
  let ParserMatchClass = UImm8AsmOperand;
  let PrintMethod = "printImmediate";
}

def uimm8alu : Operand<i16>,
    ImmLeaf<i16, [{ return isUInt<8>(Imm) && !isUInt<3>(Imm); }]> {
  let ParserMatchClass = UImm8ALUAsmOperand;
  let PrintMethod = "printImmediate";
  let EncoderMethod = "getByteImmediateOpValue";
}

def byteimmfull : Operand<i16> {
  let ParserMatchClass = ByteImmFullAsmOperand;
  let PrintMethod = "printImmediate";
  let EncoderMethod = "getByteImmediateOpValue";
}

def immediate8 : Operand<i16> {
  let ParserMatchClass = ByteImmediateAsmOperand;
  let PrintMethod = "printImmediate";
  let EncoderMethod = "getByteImmediateOpValue";
}

def uimm16 : Operand<i16>, ImmLeaf<i16, [{ return isUInt<16>(Imm); }]> {
  let ParserMatchClass = UImm16AsmOperand;
  let PrintMethod = "printImmediate";
}

def imm16full : Operand<i16>, ImmLeaf<i16, [{ return isUInt<16>(Imm); }]> {
  let ParserMatchClass = UImm16LargeAsmOperand;
  let PrintMethod = "printImmediate16";
}

def immediate16 : Operand<i16> {
  let ParserMatchClass = Immediate16AsmOperand;
  let PrintMethod = "printImmediate16";
}

def displacement16 : Operand<i16> {
  let ParserMatchClass = Word16AsmOperand;
  let PrintMethod = "printImmediate16";
}

def memory16 : Operand<i16> {
  let ParserMatchClass = Memory16AsmOperand;
  let PrintMethod = "printAddress";
  let EncoderMethod = "getDirectAddressOpValue";
}

def imm16alu : Operand<i16>,
    ImmLeaf<i16, [{
      return (isInt<16>(Imm) || isUInt<16>(Imm)) && !isUInt<3>(Imm);
    }]> {
  let ParserMatchClass = UImm16ALUAsmOperand;
  let PrintMethod = "printImmediate16";
}

def sequence_count : Operand<i16> {
  let ParserMatchClass = SequenceCountAsmOperand;
  let PrintMethod = "printImmediate";
  let EncoderMethod = "getSequenceCountOpValue";
}

def atomic_count : Operand<i16> {
  let ParserMatchClass = AtomicCountAsmOperand;
  let PrintMethod = "printImmediate";
  let EncoderMethod = "getSequenceCountOpValue";
}

def bit_address : Operand<i16> {
  let ParserMatchClass = BitAddressAsmOperand;
  let PrintMethod = "printBitAddress";
  let EncoderMethod = "getBitAddressOpValue";
}

def bit_offset : Operand<i16> {
  let ParserMatchClass = BitOffsetAsmOperand;
  let PrintMethod = "printBitOffset";
  let EncoderMethod = "getBitOffsetOpValue";
}

def directreg : RegisterOperand<GR16> {
  let EncoderMethod = "getDirectRegOpValue";
}

def SFRShortAsmOperand : AsmOperandClass {
  let Name = "SFRShort";
  let RenderMethod = "addShortRegisterOperands";
}

def SFRDirectAsmOperand : AsmOperandClass {
  let Name = "SFRDirect";
  let PredicateMethod = "isSFRShort";
  let RenderMethod = "addSFRDirectOperands";
}

def PushPopRegAsmOperand : AsmOperandClass {
  let Name = "PushPopReg";
  let RenderMethod = "addShortRegisterOperands";
}

def pushpopreg : RegisterOperand<Direct16> {
  let EncoderMethod = "getDirectRegOpValue";
  let ParserMatchClass = PushPopRegAsmOperand;
  let PrintMethod = "printShortRegister";
}

def directbytereg : RegisterOperand<GR8> {
  let EncoderMethod = "getDirectByteRegOpValue";
}

def sfrdirect : RegisterOperand<SFR16> {
  let EncoderMethod = "getSFRAddressOpValue";
  let ParserMatchClass = SFRDirectAsmOperand;
}

def sfrshort : RegisterOperand<SFR16> {
  let EncoderMethod = "getSFRShortOpValue";
  let ParserMatchClass = SFRShortAsmOperand;
  let PrintMethod = "printShortRegister";
}

def address8 : Operand<i16> {
  let ParserMatchClass = Seg8AsmOperand;
  let PrintMethod = "printAddress";
  let EncoderMethod = "getSegmentOpValue";
}

def address16 : Operand<i16> {
  let ParserMatchClass = Sof16AsmOperand;
  let PrintMethod = "printAddress";
}

def codeAddress16 : Operand<i16> {
  let ParserMatchClass = Cof16AsmOperand;
  let PrintMethod = "printAddress";
  let EncoderMethod = "getCodeAddressOpValue";
}

def addressPag10 : Operand<i16> {
  let ParserMatchClass = Pag10AsmOperand;
  let PrintMethod = "printAddress";
}

def extensionSegment8 : Operand<i16> {
  let ParserMatchClass = ExtensionSegment8AsmOperand;
  let PrintMethod = "printAddress";
}

def addressPof14 : Operand<i16> {
  let ParserMatchClass = Pof14AsmOperand;
  let PrintMethod = "printAddress";
}

def addressAbs16 : Operand<i16> {
  let ParserMatchClass = Abs16AsmOperand;
  let PrintMethod = "printAddress";
  let EncoderMethod = "getDirectAddressOpValue";
}

def addressDPP1 : Operand<i16> {
  let ParserMatchClass = DPP1AddressAsmOperand;
  let PrintMethod = "printDPP1Address";
}

def addressDPP2 : Operand<i16> {
  let ParserMatchClass = DPP2AddressAsmOperand;
  let PrintMethod = "printDPP2Address";
}

def brtarget : Operand<OtherVT> {
  let ParserMatchClass = BrTargetAsmOperand;
  let PrintMethod = "printBranchTarget";
  let EncoderMethod = "getBranchTargetOpValue";
  let OperandType = "OPERAND_PCREL";
}

def bitbrtarget : Operand<OtherVT> {
  let ParserMatchClass = BrTargetAsmOperand;
  let PrintMethod = "printBranchTarget";
  let EncoderMethod = "getBitBranchTargetOpValue";
  let OperandType = "OPERAND_PCREL";
}

class C166Inst16<bits<8> Opcode, dag Outs, dag Ins, string Asm>
    : Instruction {
  bits<16> Inst;
  let Namespace = "C166";
  let Size = 2;
  let OutOperandList = Outs;
  let InOperandList = Ins;
  let AsmString = Asm;
  let hasSideEffects = 0;
  let Inst{7-0} = Opcode;
}

class C166Inst32<bits<8> Opcode, dag Outs, dag Ins, string Asm>
    : Instruction {
  bits<32> Inst;
  let Namespace = "C166";
  let Size = 4;
  let OutOperandList = Outs;
  let InOperandList = Ins;
  let AsmString = Asm;
  let hasSideEffects = 0;
  let Inst{7-0} = Opcode;
}

class C166Pseudo<dag Outs, dag Ins> : Instruction {
  let Namespace = "C166";
  let OutOperandList = Outs;
  let InOperandList = Ins;
  let AsmString = "";
  let Pattern = [];
  let Size = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let hasSideEffects = 0;
}

let Defs = [R0, PSW], Uses = [R0], hasSideEffects = 1 in {
def ADJCALLSTACKDOWN : C166Pseudo<(outs),
                                  (ins i32imm:$amount, i32imm:$callee_pop)> {
  let Pattern = [(C166callseq_start timm:$amount, timm:$callee_pop)];
}

def ADJCALLSTACKUP : C166Pseudo<(outs),
                                (ins i32imm:$amount, i32imm:$callee_pop)> {
  let Pattern = [(C166callseq_end timm:$amount, timm:$callee_pop)];
}
}

def ADJSP : C166Pseudo<(outs), (ins uimm16:$amount)> {
  let Pattern = [(C166adjsp uimm16:$amount)];
  let hasSideEffects = 1;
  let Defs = [R0, PSW, C];
  let Uses = [R0];
}

def ALLOCSP : C166Pseudo<(outs),
                         (ins uimm16:$amount, uimm16:$dynamic_offset)> {
  let Pattern = [(C166allocsp uimm16:$amount, uimm16:$dynamic_offset)];
  let hasSideEffects = 1;
  let Defs = [R0, PSW, C];
  let Uses = [R0];
}

def PUSHARG : C166Pseudo<(outs),
                         (ins uimm16:$dynamic_offset, GR16:$src)> {
  let Pattern = [(C166pusharg uimm16:$dynamic_offset, GR16:$src)];
  let hasSideEffects = 1;
  let mayStore = 1;
  let Uses = [R0];
  let Defs = [R0, PSW];
}

def ADD32rr : C166Pseudo<(outs GR32:$dst),
                         (ins GR32:$lhs, GR32:$rhs)> {
  let Constraints = "$dst = $lhs";
  let Pattern = [(set GR32:$dst, (add GR32:$lhs, GR32:$rhs))];
  let Defs = [PSW, C];
  let isCommutable = 1;
}

def SUB32rr : C166Pseudo<(outs GR32:$dst),
                         (ins GR32:$lhs, GR32:$rhs)> {
  let Constraints = "$dst = $lhs";
  let Pattern = [(set GR32:$dst, (sub GR32:$lhs, GR32:$rhs))];
  let Defs = [PSW, C];
}

let Defs = [PSW] in {
def ADD32Carryrr : C166Pseudo<(outs GR32:$dst, CARRY:$cout),
                              (ins GR32:$lhs, GR32:$rhs)> {
  let Constraints = "$dst = $lhs";
  let isCommutable = 1;
}

def ADD32CCarryrr : C166Pseudo<(outs GR32:$dst, CARRY:$cout),
                               (ins GR32:$lhs, GR32:$rhs, CARRY:$cin)> {
  let Constraints = "$dst = $lhs";
  let Uses = [PSW];
  let isCommutable = 1;
}

def SUB32Carryrr : C166Pseudo<(outs GR32:$dst, CARRY:$cout),
                              (ins GR32:$lhs, GR32:$rhs)> {
  let Constraints = "$dst = $lhs";
}

def SUB32CCarryrr : C166Pseudo<(outs GR32:$dst, CARRY:$cout),
                               (ins GR32:$lhs, GR32:$rhs, CARRY:$cin)> {
  let Constraints = "$dst = $lhs";
  let Uses = [PSW];
}
}

let Defs = [PSW, C] in {
def ADD32CarryValuerr : C166Pseudo<(outs GR32:$dst, GR16:$cout),
                                   (ins GR32:$lhs, GR32:$rhs)> {
  let Constraints = "$dst = $lhs";
  let isCommutable = 1;
  let Size = 8;
}

def ADD32CCarryValuerr : C166Pseudo<(outs GR32:$dst, GR16:$cout),
                                    (ins GR32:$lhs, GR32:$rhs,
                                         CARRY:$cin)> {
  let Constraints = "$dst = $lhs";
  let Uses = [PSW];
  let isCommutable = 1;
  let Size = 8;
}

def SUB32CarryValuerr : C166Pseudo<(outs GR32:$dst, GR16:$cout),
                                   (ins GR32:$lhs, GR32:$rhs)> {
  let Constraints = "$dst = $lhs";
  let Size = 8;
}

def SUB32CCarryValuerr : C166Pseudo<(outs GR32:$dst, GR16:$cout),
                                    (ins GR32:$lhs, GR32:$rhs,
                                         CARRY:$cin)> {
  let Constraints = "$dst = $lhs";
  let Uses = [PSW];
  let Size = 8;
}
}

class C166ALU32ri : C166Pseudo<(outs GR32:$dst),
                               (ins GR32:$lhs, i32imm:$rhs)> {
  let Constraints = "$dst = $lhs";
  let Defs = [PSW, C];
}

def ADD32ri : C166ALU32ri;
def SUB32ri : C166ALU32ri;
def XOR32ri : C166ALU32ri;
def AND32ri : C166ALU32ri;
def OR32ri : C166ALU32ri;

def ROTL32ri5 : C166Pseudo<(outs GR32:$dst, GR16:$scratch),
                            (ins GR32:$lhs, uimm5:$amount)> {
  let Constraints = "@earlyclobber $dst,@earlyclobber $scratch";
  let Defs = [PSW, C];
}

def SHL32ri4 : C166Pseudo<(outs GR32:$dst),
                          (ins GR32:$lhs, uimm4:$amount)> {
  let Constraints = "$dst = $lhs";
  let Defs = [PSW, C];
}

class C166ShiftRight32ByOne
    : C166Pseudo<(outs GR32:$dst), (ins GR32:$lhs)> {
  let Constraints = "$dst = $lhs";
  let Size = 8;
  let Defs = [PSW, C];
}

def SRL32ri1 : C166ShiftRight32ByOne;
def SRA32ri1 : C166ShiftRight32ByOne;

class C166ShiftRightPairByOne
    : C166Pseudo<(outs GR16:$dst_low, GR16:$dst_high),
                 (ins GR16:$src_low, GR16:$src_high)> {
  let Constraints = "$dst_low = $src_low,$dst_high = $src_high";
  let Size = 8;
  let Defs = [PSW, C];
}

def SRLPAIRri1 : C166ShiftRightPairByOne;
def SRAPAIRri1 : C166ShiftRightPairByOne;

def SHL64ri4 : C166Pseudo<(outs GR32:$dst_low, GR32:$dst_high),
                          (ins GR32:$src_low, GR32:$src_high,
                               uimm4:$amount)> {
  let Constraints = "$dst_low = $src_low,$dst_high = $src_high";
  let Defs = [PSW, C];
}

// The low result also consumes the high input.  Keep that dependency separate
// from the tied high result so a dead high result cannot make the input undef.
class C166ShiftRight64ByOne
    : C166Pseudo<(outs GR32:$dst_low, GR32:$dst_high),
                 (ins GR32:$src_low, GR32:$src_high,
                      GR32:$src_high_for_low)> {
  let Constraints = "$dst_low = $src_low,$dst_high = $src_high";
  let Size = 14;
  let Defs = [PSW, C];
}

def SRL64ri1 : C166ShiftRight64ByOne;
def SRA64ri1 : C166ShiftRight64ByOne;

class C166Shift32ri5
    : C166Pseudo<(outs GR32:$dst, GR16:$scratch),
                 (ins GR32:$lhs, uimm5:$amount)> {
  let Constraints = "$dst = $lhs,@earlyclobber $scratch";
  let Defs = [PSW, C];
}

def SHL32ri5 : C166Shift32ri5;
def SRL32ri5 : C166Shift32ri5;
def SRA32ri5 : C166Shift32ri5;

def ZEXT8rr : C166Pseudo<(outs GR16:$dst), (ins GR16Byte:$src)> {
  let Defs = [PSW];
  let Size = 2;
}

def SEXT8rr : C166Pseudo<(outs GR16Byte:$dst), (ins GR16Byte:$src)> {
  let Defs = [PSW];
  let Size = 2;
}

// Keep extensions as one tuple operation through register allocation.
// Building the pair with REG_SEQUENCE lets the overlapping GR32 register
// class coalesce the source into the pair and spill it before the high word
// has been defined.
let Defs = [PSW] in {
def ZEXT16_32 : C166Pseudo<(outs GR32:$dst), (ins GR16:$src)>;
def SEXT16_32 : C166Pseudo<(outs GR32:$dst), (ins GR16:$src)> {
  let Defs = [PSW, C];
}

def FARADD32 : C166Pseudo<(outs GR32:$dst),
                          (ins GR32:$base, GR16:$offset)> {
  let Constraints = "$dst = $base";
  let Defs = [PSW, C];
}

def FARADD32i : C166Pseudo<(outs GR32:$dst),
                           (ins GR32:$base, i16imm:$offset)> {
  let Constraints = "$dst = $base";
  let Defs = [PSW, C];
}

let mayLoad = 1 in
def FARLOAD16 : C166Pseudo<(outs GR16:$dst),
                            (ins GR32:$address, i16imm:$disp)>;

let mayLoad = 1,
    Constraints = "$address_wb = $address,@earlyclobber $dst" in
def FARLOAD16_POSTINC : C166Pseudo<(outs GR16:$dst, GR32:$address_wb),
                                   (ins GR32:$address)>;

let mayLoad = 1,
    Constraints = "$address_wb = $address,@earlyclobber $dst" in {
def FARLOAD8Z_POSTINC : C166Pseudo<(outs GR16Byte:$dst, GR32:$address_wb),
                                   (ins GR32:$address)>;
def FARLOAD8S_POSTINC : C166Pseudo<(outs GR16Byte:$dst, GR32:$address_wb),
                                   (ins GR32:$address)>;
}

let mayStore = 1 in
def FARSTORE16 : C166Pseudo<(outs),
                            (ins GR32:$address, i16imm:$disp,
                                 GR16:$value)>;

let mayLoad = 1 in {
def FARLOAD8Z : C166Pseudo<(outs GR16Byte:$dst),
                           (ins GR32:$address, i16imm:$disp)>;
def FARLOAD8S : C166Pseudo<(outs GR16Byte:$dst),
                           (ins GR32:$address, i16imm:$disp)>;
def FRAMELOAD8Z : C166Pseudo<(outs GR16Byte:$dst),
                             (ins i32imm:$fi, i16imm:$disp)>;
def FRAMELOAD8S : C166Pseudo<(outs GR16Byte:$dst),
                             (ins i32imm:$fi, i16imm:$disp)>;
def NEARLOAD8Z : C166Pseudo<(outs GR16Byte:$dst),
                            (ins GR16:$base, i16imm:$disp)>;
def NEARLOAD8S : C166Pseudo<(outs GR16Byte:$dst),
                            (ins GR16:$base, i16imm:$disp)>;
let Constraints = "$base_wb = $base,@earlyclobber $dst" in {
def NEARLOAD8Z_POSTINC : C166Pseudo<(outs GR16Byte:$dst, GR16:$base_wb),
                                    (ins GR16:$base)>;
def NEARLOAD8S_POSTINC : C166Pseudo<(outs GR16Byte:$dst, GR16:$base_wb),
                                    (ins GR16:$base)>;
}
def FRAMELOAD32 : C166Pseudo<(outs GR32:$dst),
                             (ins i32imm:$fi, i16imm:$disp)>;
def NEARLOAD32 : C166Pseudo<(outs GR32:$dst),
                            (ins GR16:$base, i16imm:$disp)> {
  // An arbitrary near/xnear base can allocate to either result word.  Both
  // loads must consume the address before the first result definition is
  // allowed to clobber it.
  let Constraints = "@earlyclobber $dst";
}
}

let mayStore = 1 in
def FARSTORE8 : C166Pseudo<(outs),
                           (ins GR32:$address, i16imm:$disp,
                                GR16Byte:$value)>;

let mayStore = 1 in
def FRAMESTORE8 : C166Pseudo<(outs),
                             (ins i32imm:$fi, i16imm:$disp,
                                  GR16Byte:$value)>;

let mayStore = 1 in
def NEARSTORE8 : C166Pseudo<(outs),
                            (ins GR16:$base, i16imm:$disp,
                                 GR16Byte:$value)>;

let mayStore = 1 in {
def FRAMESTORE32 : C166Pseudo<(outs),
                              (ins i32imm:$fi, i16imm:$disp, GR32:$value)>;
def NEARSTORE32 : C166Pseudo<(outs),
                             (ins GR16:$base, i16imm:$disp, GR32:$value)>;
}

let mayLoad = 1, Constraints = "@earlyclobber $dst" in
def FARLOAD32 : C166Pseudo<(outs GR32:$dst),
                            (ins GR32:$address, i16imm:$disp)>;

let mayStore = 1 in
def FARSTORE32 : C166Pseudo<(outs),
                            (ins GR32:$address, i16imm:$disp,
                                 GR32:$value)>;

// Huge and signed-huge pointers store offset:segment. Both use EXTS for
// memory access; only pointer arithmetic and boundary-crossing 32-bit access
// differ between the two qualifiers.
let mayLoad = 1 in {
def SEGLOAD8Z : C166Pseudo<(outs GR16Byte:$dst), (ins GR32:$address)>;
def SEGLOAD8S : C166Pseudo<(outs GR16Byte:$dst), (ins GR32:$address)>;
def SEGLOAD16 : C166Pseudo<(outs GR16:$dst), (ins GR32:$address)>;
}

let mayStore = 1 in {
def SEGSTORE8 : C166Pseudo<(outs),
                           (ins GR32:$address, GR16Byte:$value)>;
def SEGSTORE16 : C166Pseudo<(outs),
                            (ins GR32:$address, GR16:$value)>;
}

let mayLoad = 1, Constraints = "@earlyclobber $dst" in
def SHUGELOAD32 : C166Pseudo<(outs GR32:$dst), (ins GR32:$address)>;

let mayStore = 1 in
def SHUGESTORE32 : C166Pseudo<(outs),
                              (ins GR32:$address, GR32:$value)>;
}

let Defs = [PSW] in {
let mayLoad = 1 in
def GLOBALLOAD16 : C166Pseudo<(outs GR16:$dst), (ins i32imm:$address)>;

let mayStore = 1 in
def GLOBALSTORE16 : C166Pseudo<(outs),
                               (ins i32imm:$address, GR16:$value)>;

let mayLoad = 1 in {
def GLOBALLOAD8Z : C166Pseudo<(outs GR16:$dst), (ins i32imm:$address)>;
def GLOBALLOAD8S : C166Pseudo<(outs GR16:$dst), (ins i32imm:$address)>;
}

let mayStore = 1 in
def GLOBALSTORE8 : C166Pseudo<(outs),
                              (ins i32imm:$address, GR16Byte:$value)>;

let mayLoad = 1 in
def GLOBALLOAD32 : C166Pseudo<(outs GR32:$dst), (ins i32imm:$address)>;

let mayStore = 1 in
def GLOBALSTORE32 : C166Pseudo<(outs),
                               (ins i32imm:$address, GR32:$value)>;

let mayLoad = 1 in {
def NEARGLOBALLOAD8Z : C166Pseudo<(outs GR16:$dst), (ins i16imm:$address)>;
def NEARGLOBALLOAD8S : C166Pseudo<(outs GR16:$dst), (ins i16imm:$address)>;
def NEARGLOBALLOAD16 : C166Pseudo<(outs GR16:$dst), (ins i16imm:$address)>;
def NEARGLOBALLOAD32 : C166Pseudo<(outs GR32:$dst), (ins i16imm:$address)>;
}

let mayStore = 1 in {
def NEARGLOBALSTORE8 : C166Pseudo<(outs),
                                  (ins i16imm:$address, GR16Byte:$value)>;
def NEARGLOBALSTORE16 : C166Pseudo<(outs),
                                   (ins i16imm:$address, GR16:$value)>;
def NEARGLOBALSTORE32 : C166Pseudo<(outs),
                                   (ins i16imm:$address, GR32:$value)>;
}

def GLOBALADDR32 : C166Pseudo<(outs GR32:$dst), (ins i32imm:$address)>;
def GLOBALDATAADDR32 : C166Pseudo<(outs GR32:$dst), (ins i32imm:$address)>;
def GLOBALHUGEDATAADDR32 : C166Pseudo<(outs GR32:$dst),
                                      (ins i32imm:$address)>;
def NEARGLOBALADDR16 : C166Pseudo<(outs GR16:$dst), (ins i16imm:$address)>;
def CONST32 : C166Pseudo<(outs GR32:$dst), (ins i32imm:$value)> {
  let isReMaterializable = 1;
}

let mayLoad = 1 in
def MOVfi : C166Pseudo<(outs GR16:$dst), (ins i32imm:$fi, i16imm:$disp)>;

def LEAfi : C166Pseudo<(outs GR16:$dst), (ins i32imm:$fi, i16imm:$disp)> {
  let isReMaterializable = 1;
}

def FRAMEADDR32 : C166Pseudo<(outs GR32:$dst),
                             (ins i32imm:$fi, i16imm:$disp)> {
  let isReMaterializable = 1;
}
}

let Defs = [PSW, C] in {
def SMUL16_32 : C166Pseudo<(outs GR32:$dst),
                            (ins GR16:$lhs, GR16:$rhs)> {
  let Pattern = [(set GR32:$dst, (C166smul16 GR16:$lhs, GR16:$rhs))];
}

def UMUL16_32 : C166Pseudo<(outs GR32:$dst),
                            (ins GR16:$lhs, GR16:$rhs)> {
  let Pattern = [(set GR32:$dst, (C166umul16 GR16:$lhs, GR16:$rhs))];
}

class C166MUL16_ADD32 : C166Pseudo<(outs GR32:$dst),
                                   (ins GR32:$addend, GR16:$lhs,
                                        GR16:$rhs)> {
  let Constraints = "$dst = $addend";
  let Size = 10;
}

def SMUL16_ADD32 : C166MUL16_ADD32;
def UMUL16_ADD32 : C166MUL16_ADD32;

def MUL16rr : C166Pseudo<(outs GR16:$dst),
                          (ins GR16:$lhs, GR16:$rhs)> {
  let Pattern = [(set GR16:$dst, (mul GR16:$lhs, GR16:$rhs))];
}

def MULHU16rr : C166Pseudo<(outs GR16:$dst),
                            (ins GR16:$lhs, GR16:$rhs)> {
  let Pattern = [(set GR16:$dst, (mulhu GR16:$lhs, GR16:$rhs))];
}

def MULHS16rr : C166Pseudo<(outs GR16:$dst),
                            (ins GR16:$lhs, GR16:$rhs)> {
  let Pattern = [(set GR16:$dst, (mulhs GR16:$lhs, GR16:$rhs))];
}

def SDIV16rr : C166Pseudo<(outs GR16:$dst),
                           (ins GR16:$lhs, GR16:$rhs)> {
  let Pattern = [(set GR16:$dst, (sdiv GR16:$lhs, GR16:$rhs))];
}

def UDIV16rr : C166Pseudo<(outs GR16:$dst),
                           (ins GR16:$lhs, GR16:$rhs)> {
  let Pattern = [(set GR16:$dst, (udiv GR16:$lhs, GR16:$rhs))];
}

def SREM16rr : C166Pseudo<(outs GR16:$dst),
                           (ins GR16:$lhs, GR16:$rhs)> {
  let Pattern = [(set GR16:$dst, (srem GR16:$lhs, GR16:$rhs))];
}

def UREM16rr : C166Pseudo<(outs GR16:$dst),
                           (ins GR16:$lhs, GR16:$rhs)> {
  let Pattern = [(set GR16:$dst, (urem GR16:$lhs, GR16:$rhs))];
}
}

let Defs = [PSW] in {
let mayLoad = 1 in
def MOVBfi : C166Pseudo<(outs GR8:$dst),
                        (ins i32imm:$fi, i16imm:$disp)>;

let mayStore = 1 in {
def MOVBfiStore : C166Pseudo<(outs),
                             (ins i32imm:$fi, i16imm:$disp, GR8:$src)>;
def MOVfiStore : C166Pseudo<(outs),
                            (ins i32imm:$fi, i16imm:$disp, GR16:$src)>;
}
}

def CMPBR : C166Pseudo<(outs),
                       (ins GR16:$lhs, GR16:$rhs, i16imm:$cc,
                            brtarget:$target)> {
  let isBranch = 1;
  let isTerminator = 1;
  let Defs = [PSW, C];
}

def CMPBBR : C166Pseudo<(outs),
                        (ins GR16Byte:$lhs, GR16Byte:$rhs, i16imm:$cc,
                             brtarget:$target)> {
  let Size = 4;
  let isBranch = 1;
  let isTerminator = 1;
  let Defs = [PSW, C];
}

def CMPBBRi : C166Pseudo<(outs),
                         (ins GR16Byte:$lhs, uimm8:$rhs, i16imm:$cc,
                              brtarget:$target)> {
  let isBranch = 1;
  let isTerminator = 1;
  let Defs = [PSW, C];
}

def CMPBRm : C166Pseudo<(outs),
                        (ins GR16:$lhs, GR16Indirect:$base, i16imm:$cc,
                             brtarget:$target)> {
  let Size = 4;
  let isBranch = 1;
  let isTerminator = 1;
  let mayLoad = 1;
  let Defs = [PSW, C];
}

def CMPBRi : C166Pseudo<(outs),
                        (ins GR16:$lhs, i16imm:$rhs, i16imm:$cc,
                             brtarget:$target)> {
  let isBranch = 1;
  let isTerminator = 1;
  let Defs = [PSW, C];
}

def TEST32BR : C166Pseudo<(outs),
                          (ins GR16:$lhs, GR16:$rhs, i16imm:$cc,
                               brtarget:$target)> {
  let isBranch = 1;
  let isTerminator = 1;
  let Defs = [R1, PSW, C];
}

def MASK32BR : C166Pseudo<(outs GR32:$scratch),
                          (ins GR32:$lhs, i32imm:$mask, i16imm:$cc,
                               brtarget:$target)> {
  let Constraints = "$scratch = $lhs";
  let isBranch = 1;
  let isTerminator = 1;
  let Defs = [PSW, C];
}

def BITBR : C166Pseudo<(outs),
                       (ins GR16:$word, uimm4:$bit, i16imm:$cc,
                            brtarget:$target)> {
  let Size = 4;
  let isBranch = 1;
  let isTerminator = 1;
}

class C166RegisterBitUpdate
    : C166Pseudo<(outs GR16:$dst), (ins GR16:$src, uimm4:$bit)> {
  let Constraints = "$dst = $src";
  let Size = 2;
  let Defs = [PSW, C];
}

def BCLRreg : C166RegisterBitUpdate;
def BSETreg : C166RegisterBitUpdate;

class C166RegisterBitFieldUpdate
    : C166Pseudo<(outs GR16:$dst),
                  (ins GR16:$src, uimm8:$mask, uimm8:$data)> {
  let Constraints = "$dst = $src";
  let Size = 4;
  let Defs = [PSW, C];
}

def BFLDLreg : C166RegisterBitFieldUpdate;
def BFLDHreg : C166RegisterBitFieldUpdate;

class C166RegisterBitBinaryUpdate
    : C166Pseudo<(outs GR16:$dst),
                  (ins GR16:$old_dst, GR16:$src,
                       uimm4:$dst_bit, uimm4:$src_bit)> {
  let Constraints = "$dst = $old_dst";
  let Size = 4;
  let Defs = [PSW, C];
}

def BMOVreg : C166RegisterBitBinaryUpdate;
def BMOVNreg : C166RegisterBitBinaryUpdate;
def BANDreg : C166RegisterBitBinaryUpdate;
def BORreg : C166RegisterBitBinaryUpdate;
def BXORreg : C166RegisterBitBinaryUpdate;

def BMOVsfrreg : C166Pseudo<(outs),
                            (ins bit_address:$dst, GR16:$src,
                                 uimm4:$src_bit)> {
  let Size = 4;
  let Defs = [PSW, C];
  let mayLoad = 1;
  let mayStore = 1;
  let hasSideEffects = 1;
}

def BMOVregsfr : C166Pseudo<(outs GR16:$dst),
                            (ins GR16:$old_dst, uimm4:$dst_bit,
                                 bit_address:$src)> {
  let Constraints = "$dst = $old_dst";
  let Size = 4;
  let Defs = [PSW, C];
  let mayLoad = 1;
  let hasSideEffects = 1;
}

class C166ESFRBitUpdate : C166Pseudo<(outs), (ins bit_address:$address)> {
  let Size = 4;
  let Defs = [PSW, C];
  let mayLoad = 1;
  let mayStore = 1;
  let hasSideEffects = 1;
}

def BCLResfr : C166ESFRBitUpdate;
def BSETesfr : C166ESFRBitUpdate;

def BMOVesfrreg : C166Pseudo<(outs),
                             (ins bit_address:$dst, GR16:$src,
                                  uimm4:$src_bit)> {
  let Size = 6;
  let Defs = [PSW, C];
  let mayLoad = 1;
  let mayStore = 1;
  let hasSideEffects = 1;
}

def BMOVregesfr : C166Pseudo<(outs GR16:$dst),
                             (ins GR16:$old_dst, uimm4:$dst_bit,
                                  bit_address:$src)> {
  let Constraints = "$dst = $old_dst";
  let Size = 6;
  let Defs = [PSW, C];
  let mayLoad = 1;
  let hasSideEffects = 1;
}

def BMOVPSWreg : C166Pseudo<(outs GR16:$dst),
                             (ins GR16:$old_dst, uimm4:$dst_bit,
                                  uimm4:$psw_bit)> {
  let Constraints = "$dst = $old_dst";
  let Size = 4;
  let Uses = [PSW];
  let Defs = [PSW, C];
}

def BCMPreg : C166Pseudo<(outs),
                          (ins GR16:$lhs, GR16:$rhs,
                               uimm4:$lhs_bit, uimm4:$rhs_bit)> {
  let Size = 4;
  let Defs = [PSW, C];
}

let isCodeGenOnly = 1 in {
def JBreg : C166Pseudo<(outs),
                       (ins GR16:$word, uimm4:$bit, brtarget:$target)> {
  let Size = 4;
  let isBranch = 1;
  let isTerminator = 1;
}

def JNBreg : C166Pseudo<(outs),
                        (ins GR16:$word, uimm4:$bit, brtarget:$target)> {
  let Size = 4;
  let isBranch = 1;
  let isTerminator = 1;
}
}

def SUB32BR : C166Pseudo<(outs GR32:$scratch),
                         (ins GR32:$lhs, GR32:$rhs, i16imm:$cc,
                              brtarget:$target)> {
  let Constraints = "$scratch = $lhs";
  let isBranch = 1;
  let isTerminator = 1;
  let Defs = [PSW, C];
}

def SUB64Carryrr
    : C166Pseudo<(outs CARRY:$cout, GR32:$dst_low, GR32:$dst_high),
                 (ins GR32:$lhs_low, GR32:$lhs_high,
                      GR32:$rhs_low, GR32:$rhs_high)> {
  let Constraints = "@earlyclobber $dst_low,@earlyclobber $dst_high,"
                    "$dst_low = $lhs_low,"
                    "$dst_high = $lhs_high";
  let Size = 8;
  let Defs = [PSW];
}

def SUB64Borrowrr
    : C166Pseudo<(outs GR16:$result, GR32:$dst_low, GR32:$dst_high),
                 (ins GR32:$lhs_low, GR32:$lhs_high,
                      GR32:$rhs_low, GR32:$rhs_high)> {
  let Constraints = "@earlyclobber $result,@earlyclobber $dst_low,"
                    "@earlyclobber $dst_high,$dst_low = $lhs_low,"
                    "$dst_high = $lhs_high";
  let Size = 12;
  let Defs = [PSW, C];
}

def CMP32BR : C166Pseudo<(outs),
                         (ins GR32:$lhs, GR32:$rhs, i16imm:$cc,
                              brtarget:$target)> {
  let isBranch = 1;
  let isTerminator = 1;
  let usesCustomInserter = 1;
  let Defs = [PSW, C];
}

let Uses = [PSW] in
def FLAGSBR : C166Pseudo<(outs),
                          (ins CARRY:$carry, i16imm:$cc, brtarget:$target)> {
  let isBranch = 1;
  let isTerminator = 1;
}

def BR : C166Pseudo<(outs), (ins brtarget:$target)> {
  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
}

def SELECT16 : C166Pseudo<(outs GR16:$dst),
                          (ins GR16:$lhs, GR16:$rhs, GR16:$true_value,
                               GR16:$false_value, i16imm:$cc)> {
  let usesCustomInserter = 1;
}


def SELECT16_32CMP : C166Pseudo<(outs GR16:$dst),
                                (ins GR32:$lhs, GR32:$rhs,
                                     GR16:$true_value, GR16:$false_value,
                                     i16imm:$cc)> {
  let usesCustomInserter = 1;
}

def SELECT32_16CMP : C166Pseudo<(outs GR32:$dst),
                                (ins GR16:$lhs, GR16:$rhs,
                                     GR32:$true_value, GR32:$false_value,
                                     i16imm:$cc)> {
  let usesCustomInserter = 1;
}

def SELECT32_32CMP : C166Pseudo<(outs GR32:$dst),
                                (ins GR32:$lhs, GR32:$rhs,
                                     GR32:$true_value, GR32:$false_value,
                                     i16imm:$cc)> {
  let usesCustomInserter = 1;
}

// Convert a disposable 0/1 word into the carry flag.
def SETCARRY : C166Pseudo<(outs CARRY:$cout), (ins GR16:$value)> {
  let Defs = [PSW];
  let Size = 2;
}

let Defs = [PSW, C] in {
def ADDCarryValuerr : C166Pseudo<(outs GR16:$dst, GR16:$cout),
                                 (ins GR16:$lhs, GR16:$rhs)> {
  let Constraints = "$dst = $lhs";
  let isCommutable = 1;
  let Size = 6;
}

def ADDCCarryValuerr : C166Pseudo<(outs GR16:$dst, GR16:$cout),
                                  (ins GR16:$lhs, GR16:$rhs, CARRY:$cin)> {
  let Constraints = "$dst = $lhs";
  let Uses = [PSW];
  let isCommutable = 1;
  let Size = 6;
}

def SUBCarryValuerr : C166Pseudo<(outs GR16:$dst, GR16:$cout),
                                 (ins GR16:$lhs, GR16:$rhs)> {
  let Constraints = "$dst = $lhs";
  let Size = 6;
}

def SUBCCarryValuerr : C166Pseudo<(outs GR16:$dst, GR16:$cout),
                                  (ins GR16:$lhs, GR16:$rhs, CARRY:$cin)> {
  let Constraints = "$dst = $lhs";
  let Uses = [PSW];
  let Size = 6;
}
}

class C166RR<bits<8> Opcode, RegisterClass RC, string Mnemonic>
    : C166Inst16<Opcode, (outs RC:$dst), (ins RC:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<4> src;
  let Inst{15-12} = dst;
  let Inst{11-8} = src;
}

class C166ALURR<bits<8> Opcode, RegisterClass RC, string Mnemonic,
                SDPatternOperator Op>
    : C166Inst16<Opcode, (outs RC:$dst), (ins RC:$lhs, RC:$rhs),
                  !strconcat(Mnemonic, "\t$dst, $rhs")> {
  bits<4> dst;
  bits<4> rhs;
  let Inst{15-12} = dst;
  let Inst{11-8} = rhs;
  let Constraints = "$dst = $lhs";
  let Pattern = [(set RC:$dst, (Op RC:$lhs, RC:$rhs))];
}

class C166ALUri3<bits<8> Opcode, string Mnemonic, SDPatternOperator Op>
    : C166Inst16<Opcode, (outs GR16:$dst),
                  (ins GR16:$lhs, uimm3:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<3> src;
  let Inst{15-12} = dst;
  let Inst{11} = 0;
  let Inst{10-8} = src;
  let Constraints = "$dst = $lhs";
  let Pattern = [(set GR16:$dst, (Op GR16:$lhs, uimm3:$src))];
}

class C166ByteALUri3<bits<8> Opcode, string Mnemonic>
    : C166Inst16<Opcode, (outs GR8:$dst),
                  (ins GR8:$lhs, uimm3:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<3> src;
  let Inst{15-12} = dst;
  let Inst{11} = 0;
  let Inst{10-8} = src;
  let Constraints = "$dst = $lhs";
}

class C166ByteCompareri3<bits<8> Opcode, string Mnemonic>
    : C166Inst16<Opcode, (outs), (ins GR8:$dst, uimm3:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<3> src;
  let Inst{15-12} = dst;
  let Inst{11} = 0;
  let Inst{10-8} = src;
}

class C166ByteALUri8<bits<8> Opcode, string Mnemonic>
    : C166Inst32<Opcode, (outs directbytereg:$dst),
                  (ins directbytereg:$lhs, uimm8alu:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<8> dst;
  bits<8> src;
  let Inst{15-8} = dst;
  let Inst{15-12} = 0xf;
  let Inst{23-16} = src;
  // The last byte is unused; emit zero and accept any value when decoding.
  let Constraints = "$dst = $lhs";
  let DecoderMethod = "decodeByteALUri8";
}

class C166ByteCompareri8<bits<8> Opcode, string Mnemonic>
    : C166Inst32<Opcode, (outs),
                  (ins directbytereg:$dst, uimm8alu:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<8> dst;
  bits<8> src;
  let Inst{15-8} = dst;
  let Inst{15-12} = 0xf;
  let Inst{23-16} = src;
  let DecoderMethod = "decodeByteRegImmediate";
}

class C166ALUri16<bits<8> Opcode, string Mnemonic, SDPatternOperator Op>
    : C166Inst32<Opcode, (outs GR16:$dst),
                  (ins GR16:$lhs, imm16alu:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<16> src;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = dst;
  let Inst{31-16} = src;
  let Constraints = "$dst = $lhs";
  let Pattern = [(set GR16:$dst, (Op GR16:$lhs, imm16alu:$src))];
}

class C166ALUDirect<bits<8> Opcode, dag Outs, dag Ins, string Mnemonic,
                 string Decoder>
    : C166Inst32<Opcode, Outs, Ins,
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<16> src;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = dst;
  let Inst{31-16} = src;
  let DecoderMethod = Decoder;
}

class C166MemorySFR<bits<8> Opcode, string Mnemonic>
    : C166Inst32<Opcode, (outs),
                 (ins addressAbs16:$dst, sfrshort:$src),
                 !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<16> dst;
  bits<8> src;
  let Inst{15-8} = src;
  let Inst{31-16} = dst;
  let mayStore = 1;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeMemorySFR";
}

class C166LoopCompareri4<bits<8> Opcode, string Mnemonic>
    : C166Inst16<Opcode, (outs GR16:$dst),
                  (ins GR16:$lhs, uimm4:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<4> src;
  let Inst{15-12} = src;
  let Inst{11-8} = dst;
  let Constraints = "$dst = $lhs";
}

class C166LoopCompareLong<bits<8> Opcode, string Mnemonic, Operand Source>
    : C166Inst32<Opcode, (outs GR16:$dst),
                  (ins GR16:$lhs, Source:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<16> src;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = dst;
  let Inst{31-16} = src;
  let Constraints = "$dst = $lhs";
}

class C166Carryri3<bits<8> Opcode, string Mnemonic>
    : C166Inst16<Opcode, (outs GR16:$dst),
                  (ins GR16:$lhs, uimm3:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<3> src;
  let Inst{15-12} = dst;
  let Inst{11} = 0;
  let Inst{10-8} = src;
  let Constraints = "$dst = $lhs";
}

class C166Carryri16<bits<8> Opcode, string Mnemonic>
    : C166Inst32<Opcode, (outs GR16:$dst),
                  (ins GR16:$lhs, imm16alu:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<16> src;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = dst;
  let Inst{31-16} = src;
  let Constraints = "$dst = $lhs";
}

class C166RI4<bits<8> Opcode, RegisterClass RC, string Mnemonic>
    : C166Inst16<Opcode, (outs RC:$dst), (ins uimm4:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<4> dst;
  bits<4> src;
  let Inst{15-12} = src;
  let Inst{11-8} = dst;
}

class C166RM<bits<8> Opcode, string Mnemonic>
    : C166Inst16<Opcode, (outs GR16:$dst), (ins GR16:$base),
                  !strconcat(Mnemonic, "\t$dst, [$base]")> {
  bits<4> dst;
  bits<4> base;
  let Inst{15-12} = dst;
  let Inst{11-8} = base;
  let mayLoad = 1;
}

class C166ALUIndirect<bits<8> Opcode, dag Outs, dag Ins, string Mnemonic>
    : C166Inst16<Opcode, Outs, Ins,
                  !strconcat(Mnemonic, "\t$dst, [$base]")> {
  bits<4> dst;
  bits<2> base;
  let Inst{15-12} = dst;
  let Inst{11-10} = 0b10;
  let Inst{9-8} = base;
  let mayLoad = 1;
}

class C166ALUPostInc<bits<8> Opcode, dag Outs, dag Ins, string Mnemonic>
    : C166Inst16<Opcode, Outs, Ins,
                  !strconcat(Mnemonic, "\t$dst, [${base}+]")> {
  bits<4> dst;
  bits<2> base;
  let Inst{15-12} = dst;
  let Inst{11-10} = 0b11;
  let Inst{9-8} = base;
  let mayLoad = 1;
}

let Defs = [PSW, C] in {
// Numeric and symbolic direct-memory sources use the same instruction as
// named SFR sources, but do not model the address as an explicit register.
class C166ALUMemory<bits<8> Opcode, string Mnemonic, RegisterClass Reg = GR16>
    : C166ALUDirect<Opcode, (outs Reg:$dst),
                    (ins Reg:$lhs, addressAbs16:$src), Mnemonic,
                    !if(!eq(Reg, GR16), "decodeALUgsfr", "")> {
  let Constraints = "$dst = $lhs";
  let mayLoad = 1;
  let isAsmParserOnly = !eq(Reg, GR16);
}
def ADDgm : C166ALUMemory<0x02, "add">;
def SUBgm : C166ALUMemory<0x22, "sub">;
def XORgm : C166ALUMemory<0x52, "xor">;
def ANDgm : C166ALUMemory<0x62, "and">;
def ORgm : C166ALUMemory<0x72, "or">;
let Uses = [PSW, C] in {
def ADDCgm : C166ALUMemory<0x12, "addc">;
def SUBCgm : C166ALUMemory<0x32, "subc">;
}
def CMPgm : C166ALUDirect<0x42, (outs),
                         (ins GR16:$dst, addressAbs16:$src), "cmp",
                         "decodeCMPgsfr"> {
  let mayLoad = 1;
  let isAsmParserOnly = 1;
}

class C166SFRALU<bits<8> Opcode, string Mnemonic,
                  DAGOperand Source = immediate16, bit Compare = 0>
    : C166Inst32<Opcode, !if(Compare, (outs), (outs sfrshort:$dst)),
                 !if(Compare, (ins sfrshort:$dst, Source:$src),
                              (ins sfrshort:$lhs, Source:$src)),
                 !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<8> dst;
  bits<16> src;
  let Inst{15-8} = dst;
  let Inst{31-16} = src;
  let Constraints = !if(Compare, "", "$dst = $lhs");
  let hasSideEffects = 1;
  let DecoderMethod = !if(Compare, "decodeSFRLongOperand<false>",
                                  "decodeSFRLongOperand<true>");
}
def ADDsfri16 : C166SFRALU<0x06, "add">;
def SUBsfri16 : C166SFRALU<0x26, "sub">;
def XORsfri16 : C166SFRALU<0x56, "xor">;
def ANDsfri16 : C166SFRALU<0x66, "and">;
def ORsfri16 : C166SFRALU<0x76, "or">;
let Uses = [PSW, C] in {
def ADDCsfri16 : C166SFRALU<0x16, "addc">;
def SUBCsfri16 : C166SFRALU<0x36, "subc">;
}
def CMPsfri16 : C166SFRALU<0x46, "cmp"> {
  let OutOperandList = (outs);
  let InOperandList = (ins sfrshort:$dst, immediate16:$src);
  let Constraints = "";
  let DecoderMethod = "decodeSFRLongOperand<false>";
}

// Named SFR sources use the same direct-memory encoding, while keeping their
// register uses explicit. The memory forms remain the canonical decoder.
let isAsmParserOnly = 1 in {
def ADDsfrsfr : C166SFRALU<0x02, "add", sfrdirect>;
def SUBsfrsfr : C166SFRALU<0x22, "sub", sfrdirect>;
def XORsfrsfr : C166SFRALU<0x52, "xor", sfrdirect>;
def ANDsfrsfr : C166SFRALU<0x62, "and", sfrdirect>;
def ORsfrsfr : C166SFRALU<0x72, "or", sfrdirect>;
def CMPsfrsfr : C166SFRALU<0x42, "cmp", sfrdirect> {
  let OutOperandList = (outs);
  let InOperandList = (ins sfrshort:$dst, sfrdirect:$src);
  let Constraints = "";
}
let Uses = [PSW, C] in {
def ADDCsfrsfr : C166SFRALU<0x12, "addc", sfrdirect>;
def SUBCsfrsfr : C166SFRALU<0x32, "subc", sfrdirect>;
}
}

class C166ALUMemoryDest<bits<8> Opcode, string Mnemonic,
                        RegisterClass Reg = GR16>
    : C166Inst32<Opcode, (outs),
                 (ins addressAbs16:$dst, Reg:$src),
                 !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<16> dst;
  bits<4> src;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = src;
  let Inst{31-16} = dst;
  let mayLoad = 1;
  let mayStore = 1;
}
def ADDmg : C166ALUMemoryDest<0x04, "add">;
def SUBmg : C166ALUMemoryDest<0x24, "sub">;
def XORmg : C166ALUMemoryDest<0x54, "xor">;
def ANDmg : C166ALUMemoryDest<0x64, "and">;
def ORmg : C166ALUMemoryDest<0x74, "or">;
let Uses = [PSW, C] in {
def ADDCmg : C166ALUMemoryDest<0x14, "addc">;
def SUBCmg : C166ALUMemoryDest<0x34, "subc">;
}

let mayLoad = 1 in {
def ADDsfrm : C166SFRALU<0x02, "add", addressAbs16>;
def SUBsfrm : C166SFRALU<0x22, "sub", addressAbs16>;
def XORsfrm : C166SFRALU<0x52, "xor", addressAbs16>;
def ANDsfrm : C166SFRALU<0x62, "and", addressAbs16>;
def ORsfrm : C166SFRALU<0x72, "or", addressAbs16>;
def CMPsfrm : C166SFRALU<0x42, "cmp", addressAbs16> {
  let OutOperandList = (outs);
  let InOperandList = (ins sfrshort:$dst, addressAbs16:$src);
  let Constraints = "";
  let DecoderMethod = "decodeSFRLongOperand<false>";
}
def ADDmsfr : C166MemorySFR<0x04, "add">;
def SUBmsfr : C166MemorySFR<0x24, "sub">;
def XORmsfr : C166MemorySFR<0x54, "xor">;
def ANDmsfr : C166MemorySFR<0x64, "and">;
def ORmsfr : C166MemorySFR<0x74, "or">;
let Uses = [PSW, C] in {
def ADDCsfrm : C166SFRALU<0x12, "addc", addressAbs16>;
def SUBCsfrm : C166SFRALU<0x32, "subc", addressAbs16>;
def ADDCmsfr : C166MemorySFR<0x14, "addc">;
def SUBCmsfr : C166MemorySFR<0x34, "subc">;
}
}

multiclass C166ByteSFRALU<bits<8> Opcode, string Mnemonic, bit Compare = 0> {
  def sfri8 : C166SFRALU<!add(Opcode, 7), Mnemonic, immediate8, Compare> {
    // The fourth byte is unused, as in the GPR immediate form.
    let Inst{31-24} = ?;
    let DecoderMethod = !if(Compare, "decodeSFRLongOperand<false, 8>",
                                    "decodeSFRLongOperand<true, 8>");
  }
  let mayLoad = 1 in
  def sfrm : C166SFRALU<!add(Opcode, 3), Mnemonic, addressAbs16, Compare>;
  let isAsmParserOnly = 1 in
  def sfrsfr : C166SFRALU<!add(Opcode, 3), Mnemonic, sfrdirect, Compare>;
  if !not(Compare) then {
    let mayLoad = 1 in
    def msfr : C166MemorySFR<!add(Opcode, 5), Mnemonic>;
  }
}
defm ADDB : C166ByteSFRALU<0x00, "addb">;
defm SUBB : C166ByteSFRALU<0x20, "subb">;
defm CMPB : C166ByteSFRALU<0x40, "cmpb", 1>;
defm XORB : C166ByteSFRALU<0x50, "xorb">;
defm ANDB : C166ByteSFRALU<0x60, "andb">;
defm ORB : C166ByteSFRALU<0x70, "orb">;
let Uses = [PSW, C] in {
defm ADDCB : C166ByteSFRALU<0x10, "addcb">;
defm SUBCB : C166ByteSFRALU<0x30, "subcb">;
}

def ADDBgm : C166ALUMemory<0x03, "addb", GR8>;
def SUBBgm : C166ALUMemory<0x23, "subb", GR8>;
def XORBgm : C166ALUMemory<0x53, "xorb", GR8>;
def ANDBgm : C166ALUMemory<0x63, "andb", GR8>;
def ORBgm : C166ALUMemory<0x73, "orb", GR8>;
def CMPBgm : C166ALUDirect<0x43, (outs),
                          (ins GR8:$dst, addressAbs16:$src), "cmpb", ""> {
  let mayLoad = 1;
}
def ADDBmg : C166ALUMemoryDest<0x05, "addb", GR8>;
def SUBBmg : C166ALUMemoryDest<0x25, "subb", GR8>;
def XORBmg : C166ALUMemoryDest<0x55, "xorb", GR8>;
def ANDBmg : C166ALUMemoryDest<0x65, "andb", GR8>;
def ORBmg : C166ALUMemoryDest<0x75, "orb", GR8>;
multiclass C166ByteALUIndirect<bits<8> Opcode, string Mnemonic> {
  def rm : C166ALUIndirect<Opcode, (outs GR8:$dst),
                          (ins GR8:$lhs, GR16Indirect:$base), Mnemonic> {
    let Constraints = "$dst = $lhs";
  }
  def rmPostInc
      : C166ALUPostInc<Opcode, (outs GR8:$dst, GR16Indirect:$base_wb),
                       (ins GR8:$lhs, GR16Indirect:$base), Mnemonic> {
    let Constraints = "$dst = $lhs,$base_wb = $base";
  }
}
defm ADDB : C166ByteALUIndirect<0x09, "addb">;
defm SUBB : C166ByteALUIndirect<0x29, "subb">;
defm XORB : C166ByteALUIndirect<0x59, "xorb">;
defm ANDB : C166ByteALUIndirect<0x69, "andb">;
defm ORB : C166ByteALUIndirect<0x79, "orb">;
def CMPBrm : C166ALUIndirect<0x49, (outs),
                             (ins GR8:$dst, GR16Indirect:$base), "cmpb">;
def CMPBrmPostInc
    : C166ALUPostInc<0x49, (outs GR16Indirect:$base_wb),
                     (ins GR8:$dst, GR16Indirect:$base), "cmpb"> {
  let Constraints = "$base_wb = $base";
}

let Uses = [PSW, C] in {
def ADDCBgm : C166ALUMemory<0x13, "addcb", GR8>;
def SUBCBgm : C166ALUMemory<0x33, "subcb", GR8>;
def ADDCBmg : C166ALUMemoryDest<0x15, "addcb", GR8>;
def SUBCBmg : C166ALUMemoryDest<0x35, "subcb", GR8>;

multiclass C166ByteCarry<bits<8> RegisterOpcode, bits<8> CompactOpcode,
                         bits<8> ImmediateOpcode, string Mnemonic>
    : C166ByteALUIndirect<CompactOpcode, Mnemonic> {
  def rr : C166ALURR<RegisterOpcode, GR8, Mnemonic, null_frag> {
    let hasSideEffects = 0;
  }
  def ri3 : C166ByteALUri3<CompactOpcode, Mnemonic>;
  def ri8 : C166ByteALUri8<ImmediateOpcode, Mnemonic>;
}
defm ADDCB : C166ByteCarry<0x11, 0x19, 0x17, "addcb">;
defm SUBCB : C166ByteCarry<0x31, 0x39, 0x37, "subcb">;
}

def ADDrr : C166ALURR<0x00, GR16, "add", add> {
  let isCommutable = 1;
}
let Uses = [PSW, C] in
def ADDCrr : C166ALURR<0x10, GR16, "addc", null_frag> {
  let hasSideEffects = 0;
}
def ADDri3 : C166ALUri3<0x08, "add", add>;
def ADDri16 : C166ALUri16<0x06, "add", add>;
def ADDgsfr : C166ALUDirect<0x02, (outs GR16:$dst),
                         (ins GR16:$lhs, sfrdirect:$src), "add",
                         "decodeALUgsfr"> {
  let Constraints = "$dst = $lhs";
}
let Uses = [PSW, C] in {
def ADDCri3 : C166Carryri3<0x18, "addc">;
def ADDCri16 : C166Carryri16<0x16, "addc">;
def ADDCgsfr : C166ALUDirect<0x12, (outs GR16:$dst),
                          (ins GR16:$lhs, sfrdirect:$src), "addc",
                          "decodeALUgsfr"> {
  let Constraints = "$dst = $lhs";
}
}
def ADDBrr : C166ALURR<0x01, GR8, "addb", null_frag> {
  let isCommutable = 1;
}
def ADDBri3 : C166ByteALUri3<0x09, "addb">;
def ADDBri8 : C166ByteALUri8<0x07, "addb">;
def SUBrr : C166ALURR<0x20, GR16, "sub", sub>;
let Uses = [PSW, C] in
def SUBCrr : C166ALURR<0x30, GR16, "subc", null_frag> {
  let hasSideEffects = 0;
}
def SUBri3 : C166ALUri3<0x28, "sub", sub>;
def SUBri16 : C166ALUri16<0x26, "sub", sub>;
def SUBgsfr : C166ALUDirect<0x22, (outs GR16:$dst),
                         (ins GR16:$lhs, sfrdirect:$src), "sub",
                         "decodeALUgsfr"> {
  let Constraints = "$dst = $lhs";
}
let Uses = [PSW, C] in {
def SUBCri3 : C166Carryri3<0x38, "subc">;
def SUBCri16 : C166Carryri16<0x36, "subc">;
def SUBCgsfr : C166ALUDirect<0x32, (outs GR16:$dst),
                          (ins GR16:$lhs, sfrdirect:$src), "subc",
                          "decodeALUgsfr"> {
  let Constraints = "$dst = $lhs";
}
}
def SUBBrr : C166ALURR<0x21, GR8, "subb", null_frag>;
def SUBBri3 : C166ByteALUri3<0x29, "subb">;
def SUBBri8 : C166ByteALUri8<0x27, "subb">;
def NEG : C166Inst16<0x81, (outs GR16:$dst), (ins GR16:$src),
                     "neg\t$dst"> {
  bits<4> dst;
  let Inst{15-12} = dst;
  let Inst{11-8} = 0;
  let Constraints = "$dst = $src";
  let Pattern = [(set GR16:$dst, (ineg GR16:$src))];
}
def NEGB : C166Inst16<0xa1, (outs GR8:$dst), (ins GR8:$src),
                      "negb\t$dst"> {
  bits<4> dst;
  let Inst{15-12} = dst;
  let Inst{11-8} = 0;
  let Constraints = "$dst = $src";
}
def CPL : C166Inst16<0x91, (outs GR16:$dst), (ins GR16:$src),
                     "cpl\t$dst"> {
  bits<4> dst;
  let Inst{15-12} = dst;
  let Inst{11-8} = 0;
  let Constraints = "$dst = $src";
  let Pattern = [(set GR16:$dst, (xor GR16:$src, (i16 -1)))];
}
def CPLB : C166Inst16<0xb1, (outs GR8:$dst), (ins GR8:$src),
                      "cplb\t$dst"> {
  bits<4> dst;
  let Inst{15-12} = dst;
  let Inst{11-8} = 0;
  let Constraints = "$dst = $src";
}
let hasSideEffects = 0 in {
def ROLrr : C166ALURR<0x0c, GR16, "rol", null_frag>;
def RORrr : C166ALURR<0x2c, GR16, "ror", null_frag>;
class C166RotateImmediate<bits<8> Opcode, string Mnemonic>
    : C166Inst16<Opcode, (outs GR16:$dst),
                 (ins GR16:$lhs, uimm4:$amount),
                 !strconcat(Mnemonic, "\t$dst, $amount")> {
  bits<4> dst;
  bits<4> amount;
  let Inst{15-12} = amount;
  let Inst{11-8} = dst;
  let Constraints = "$dst = $lhs";
}
def ROLri4 : C166RotateImmediate<0x1c, "rol">;
def RORri4 : C166RotateImmediate<0x3c, "ror">;
}

def SHLrr : C166ALURR<0x4c, GR16, "shl", shl>;
def SHLri4 : C166Inst16<0x5c, (outs GR16:$dst),
                        (ins GR16:$lhs, uimm4:$amount),
                        "shl\t$dst, $amount"> {
  bits<4> dst;
  bits<4> amount;
  let Inst{15-12} = amount;
  let Inst{11-8} = dst;
  let Constraints = "$dst = $lhs";
  let Pattern = [(set GR16:$dst, (shl GR16:$lhs, uimm4:$amount))];
}
def SHRrr : C166ALURR<0x6c, GR16, "shr", srl>;
def SHRri4 : C166Inst16<0x7c, (outs GR16:$dst),
                        (ins GR16:$lhs, uimm4:$amount),
                        "shr\t$dst, $amount"> {
  bits<4> dst;
  bits<4> amount;
  let Inst{15-12} = amount;
  let Inst{11-8} = dst;
  let Constraints = "$dst = $lhs";
  let Pattern = [(set GR16:$dst, (srl GR16:$lhs, uimm4:$amount))];
}
def ASHRrr : C166ALURR<0xac, GR16, "ashr", sra>;
def ASHRri4 : C166Inst16<0xbc, (outs GR16:$dst),
                         (ins GR16:$lhs, uimm4:$amount),
                         "ashr\t$dst, $amount"> {
  bits<4> dst;
  bits<4> amount;
  let Inst{15-12} = amount;
  let Inst{11-8} = dst;
  let Constraints = "$dst = $lhs";
  let Pattern = [(set GR16:$dst, (sra GR16:$lhs, uimm4:$amount))];
}

def PRIORrr : C166RR<0x2b, GR16, "prior"> {
  let Pattern = [(set GR16:$dst, (ctlz_zero_poison GR16:$src))];
}
def CMPrr : C166RR<0x40, GR16, "cmp"> {
  let OutOperandList = (outs);
  let InOperandList = (ins GR16:$dst, GR16:$src);
}
def CMPgsfr : C166ALUDirect<0x42, (outs),
                         (ins GR16:$dst, sfrdirect:$src), "cmp",
                         "decodeCMPgsfr">;
def CMPri3 : C166Inst16<0x48, (outs), (ins GR16:$dst, uimm3:$src),
                        "cmp\t$dst, $src"> {
  bits<4> dst;
  bits<3> src;
  let Inst{15-12} = dst;
  let Inst{11} = 0;
  let Inst{10-8} = src;
}
def CMPD1ri4 : C166LoopCompareri4<0xa0, "cmpd1">;
def CMPD1ri16 : C166LoopCompareLong<0xa6, "cmpd1", imm16full>;
def CMPD2ri4 : C166LoopCompareri4<0xb0, "cmpd2">;
def CMPD2ri16 : C166LoopCompareLong<0xb6, "cmpd2", imm16full>;
def CMPI1ri4 : C166LoopCompareri4<0x80, "cmpi1">;
def CMPI1ri16 : C166LoopCompareLong<0x86, "cmpi1", imm16full>;
def CMPI2ri4 : C166LoopCompareri4<0x90, "cmpi2">;
def CMPI2ri16 : C166LoopCompareLong<0x96, "cmpi2", imm16full>;
let mayLoad = 1 in {
def CMPD1rm : C166LoopCompareLong<0xa2, "cmpd1", addressAbs16>;
def CMPD2rm : C166LoopCompareLong<0xb2, "cmpd2", addressAbs16>;
def CMPI1rm : C166LoopCompareLong<0x82, "cmpi1", addressAbs16>;
def CMPI2rm : C166LoopCompareLong<0x92, "cmpi2", addressAbs16>;
}
def CMPBrr : C166RR<0x41, GR8, "cmpb"> {
  let OutOperandList = (outs);
  let InOperandList = (ins GR8:$dst, GR8:$src);
}
def CMPBri3 : C166ByteCompareri3<0x49, "cmpb">;
def CMPBri8 : C166ByteCompareri8<0x47, "cmpb">;
def XORrr : C166ALURR<0x50, GR16, "xor", xor> {
  let isCommutable = 1;
}
def XORri3 : C166ALUri3<0x58, "xor", xor>;
def XORri16 : C166ALUri16<0x56, "xor", xor>;
def XORgsfr : C166ALUDirect<0x52, (outs GR16:$dst),
                         (ins GR16:$lhs, sfrdirect:$src), "xor",
                         "decodeALUgsfr"> {
  let Constraints = "$dst = $lhs";
}
def XORBrr : C166ALURR<0x51, GR8, "xorb", null_frag> {
  let isCommutable = 1;
}
def XORBri3 : C166ByteALUri3<0x59, "xorb">;
def XORBri8 : C166ByteALUri8<0x57, "xorb">;
def ANDrr : C166ALURR<0x60, GR16, "and", and> {
  let isCommutable = 1;
}
def ANDri3 : C166ALUri3<0x68, "and", and>;
def ANDri16 : C166ALUri16<0x66, "and", and>;
def ANDgsfr : C166ALUDirect<0x62, (outs GR16:$dst),
                         (ins GR16:$lhs, sfrdirect:$src), "and",
                         "decodeALUgsfr"> {
  let Constraints = "$dst = $lhs";
}
def ANDBrr : C166ALURR<0x61, GR8, "andb", null_frag> {
  let isCommutable = 1;
}
def ANDBri3 : C166ByteALUri3<0x69, "andb">;
def ANDBri8 : C166ByteALUri8<0x67, "andb">;
def ORrr : C166ALURR<0x70, GR16, "or", or> {
  let isCommutable = 1;
}
def ORri3 : C166ALUri3<0x78, "or", or>;
def ORri16 : C166ALUri16<0x76, "or", or>;
def ORgsfr : C166ALUDirect<0x72, (outs GR16:$dst),
                        (ins GR16:$lhs, sfrdirect:$src), "or",
                        "decodeALUgsfr"> {
  let Constraints = "$dst = $lhs";
}
def ORBrr : C166ALURR<0x71, GR8, "orb", null_frag> {
  let isCommutable = 1;
}
def ORBri3 : C166ByteALUri3<0x79, "orb">;
def ORBri8 : C166ByteALUri8<0x77, "orb">;
}

// Register-count shifts use only the low four bits of the count operand.
// Select an explicitly masked count directly instead of materializing an AND.
def : Pat<(shl GR16:$word, (and GR16:$amount, (i16 15))),
          (SHLrr GR16:$word, GR16:$amount)>;
def : Pat<(srl GR16:$word, (and GR16:$amount, (i16 15))),
          (SHRrr GR16:$word, GR16:$amount)>;
def : Pat<(sra GR16:$word, (and GR16:$amount, (i16 15))),
          (ASHRrr GR16:$word, GR16:$amount)>;

// The indirect ALU forms encode R0-R3 in a two-bit address field.  Keep the
// architectural forms available to the assembler and use separate forms below
// when carry is represented explicitly in Machine IR.
let Defs = [PSW, C] in {
def ADDrm : C166ALUIndirect<0x08, (outs GR16:$dst),
                            (ins GR16:$lhs, GR16Indirect:$base), "add"> {
  let Constraints = "$dst = $lhs";
}
let Uses = [PSW, C] in
def ADDCrm : C166ALUIndirect<0x18, (outs GR16:$dst),
                             (ins GR16:$lhs, GR16Indirect:$base), "addc"> {
  let Constraints = "$dst = $lhs";
}
def SUBrm : C166ALUIndirect<0x28, (outs GR16:$dst),
                            (ins GR16:$lhs, GR16Indirect:$base), "sub"> {
  let Constraints = "$dst = $lhs";
}
let Uses = [PSW, C] in
def SUBCrm : C166ALUIndirect<0x38, (outs GR16:$dst),
                             (ins GR16:$lhs, GR16Indirect:$base), "subc"> {
  let Constraints = "$dst = $lhs";
}
def CMPrm : C166ALUIndirect<0x48, (outs),
                            (ins GR16:$dst, GR16Indirect:$base), "cmp">;
def XORrm : C166ALUIndirect<0x58, (outs GR16:$dst),
                            (ins GR16:$lhs, GR16Indirect:$base), "xor"> {
  let Constraints = "$dst = $lhs";
}
def ANDrm : C166ALUIndirect<0x68, (outs GR16:$dst),
                            (ins GR16:$lhs, GR16Indirect:$base), "and"> {
  let Constraints = "$dst = $lhs";
}
def ORrm : C166ALUIndirect<0x78, (outs GR16:$dst),
                           (ins GR16:$lhs, GR16Indirect:$base), "or"> {
  let Constraints = "$dst = $lhs";
}

def ADDrmPostInc
    : C166ALUPostInc<0x08, (outs GR16:$dst, GR16Indirect:$base_wb),
                     (ins GR16:$lhs, GR16Indirect:$base), "add"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
}
def ADDCrmPostInc
    : C166ALUPostInc<0x18, (outs GR16:$dst, GR16Indirect:$base_wb),
                     (ins GR16:$lhs, GR16Indirect:$base), "addc"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
  let Uses = [PSW, C];
}
def SUBrmPostInc
    : C166ALUPostInc<0x28, (outs GR16:$dst, GR16Indirect:$base_wb),
                     (ins GR16:$lhs, GR16Indirect:$base), "sub"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
}
def SUBCrmPostInc
    : C166ALUPostInc<0x38, (outs GR16:$dst, GR16Indirect:$base_wb),
                     (ins GR16:$lhs, GR16Indirect:$base), "subc"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
  let Uses = [PSW, C];
}
def CMPrmPostInc
    : C166ALUPostInc<0x48, (outs GR16Indirect:$base_wb),
                     (ins GR16:$dst, GR16Indirect:$base), "cmp"> {
  let Constraints = "$base_wb = $base";
}
def XORrmPostInc
    : C166ALUPostInc<0x58, (outs GR16:$dst, GR16Indirect:$base_wb),
                     (ins GR16:$lhs, GR16Indirect:$base), "xor"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
}
def ANDrmPostInc
    : C166ALUPostInc<0x68, (outs GR16:$dst, GR16Indirect:$base_wb),
                     (ins GR16:$lhs, GR16Indirect:$base), "and"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
}
def ORrmPostInc
    : C166ALUPostInc<0x78, (outs GR16:$dst, GR16Indirect:$base_wb),
                     (ins GR16:$lhs, GR16Indirect:$base), "or"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
}
}

// Code-generation forms with explicit carry operands.  They use the same
// encodings as the architectural ALU instructions above, but make the PSW
// dependency visible to register allocation and scheduling.
class C166CarryOutri3<bits<8> Opcode, string Mnemonic>
    : C166Inst16<Opcode, (outs GR16:$dst, CARRY:$cout),
                  (ins GR16:$lhs, uimm3:$rhs),
                  !strconcat(Mnemonic, "\t$dst, $rhs")> {
  bits<4> dst;
  bits<3> rhs;
  let Inst{15-12} = dst;
  let Inst{11} = 0;
  let Inst{10-8} = rhs;
  let Constraints = "$dst = $lhs";
}

let isCodeGenOnly = 1, Defs = [PSW] in {
def ADDCarryri3 : C166CarryOutri3<0x08, "add"> {
  let isCommutable = 1;
}

def SUBCarryri3 : C166CarryOutri3<0x28, "sub">;

def ADDCarryrr : C166Inst16<0x00, (outs GR16:$dst, CARRY:$cout),
                            (ins GR16:$lhs, GR16:$rhs),
                            "add\t$dst, $rhs"> {
  bits<4> dst;
  bits<4> rhs;
  let Inst{15-12} = dst;
  let Inst{11-8} = rhs;
  let Constraints = "$dst = $lhs";
  let isCommutable = 1;
}

def ADDCCarryrr : C166Inst16<0x10, (outs GR16:$dst, CARRY:$cout),
                             (ins GR16:$lhs, GR16:$rhs, CARRY:$cin),
                             "addc\t$dst, $rhs"> {
  bits<4> dst;
  bits<4> rhs;
  let Inst{15-12} = dst;
  let Inst{11-8} = rhs;
  let Constraints = "$dst = $lhs";
  let isCommutable = 1;
  let Uses = [PSW];
}

def SUBCarryrr : C166Inst16<0x20, (outs GR16:$dst, CARRY:$cout),
                            (ins GR16:$lhs, GR16:$rhs),
                            "sub\t$dst, $rhs"> {
  bits<4> dst;
  bits<4> rhs;
  let Inst{15-12} = dst;
  let Inst{11-8} = rhs;
  let Constraints = "$dst = $lhs";
}

def SUBCCarryrr : C166Inst16<0x30, (outs GR16:$dst, CARRY:$cout),
                             (ins GR16:$lhs, GR16:$rhs, CARRY:$cin),
                             "subc\t$dst, $rhs"> {
  bits<4> dst;
  bits<4> rhs;
  let Inst{15-12} = dst;
  let Inst{11-8} = rhs;
  let Constraints = "$dst = $lhs";
  let Uses = [PSW];
}

def ADDCarryrm
    : C166ALUIndirect<0x08, (outs GR16:$dst, CARRY:$cout),
                      (ins GR16:$lhs, GR16Indirect:$base), "add"> {
  let Constraints = "$dst = $lhs";
}

def ADDCCarryrm
    : C166ALUIndirect<0x18, (outs GR16:$dst, CARRY:$cout),
                      (ins GR16:$lhs, GR16Indirect:$base, CARRY:$cin), "addc"> {
  let Constraints = "$dst = $lhs";
  let Uses = [PSW];
}

def SUBCarryrm
    : C166ALUIndirect<0x28, (outs GR16:$dst, CARRY:$cout),
                      (ins GR16:$lhs, GR16Indirect:$base), "sub"> {
  let Constraints = "$dst = $lhs";
}

def SUBCCarryrm
    : C166ALUIndirect<0x38, (outs GR16:$dst, CARRY:$cout),
                      (ins GR16:$lhs, GR16Indirect:$base, CARRY:$cin), "subc"> {
  let Constraints = "$dst = $lhs";
  let Uses = [PSW];
}

def ADDCarryrmPostInc
    : C166ALUPostInc<
          0x08, (outs GR16:$dst, CARRY:$cout, GR16Indirect:$base_wb),
          (ins GR16:$lhs, GR16Indirect:$base), "add"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
}

def ADDCCarryrmPostInc
    : C166ALUPostInc<
          0x18, (outs GR16:$dst, CARRY:$cout, GR16Indirect:$base_wb),
          (ins GR16:$lhs, GR16Indirect:$base, CARRY:$cin), "addc"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
  let Uses = [PSW];
}

def SUBCarryrmPostInc
    : C166ALUPostInc<
          0x28, (outs GR16:$dst, CARRY:$cout, GR16Indirect:$base_wb),
          (ins GR16:$lhs, GR16Indirect:$base), "sub"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
}

def SUBCCarryrmPostInc
    : C166ALUPostInc<
          0x38, (outs GR16:$dst, CARRY:$cout, GR16Indirect:$base_wb),
          (ins GR16:$lhs, GR16Indirect:$base, CARRY:$cin), "subc"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
  let Uses = [PSW];
}
}

// Carry-consuming forms used when the outgoing carry is dead.
let isCodeGenOnly = 1, Defs = [PSW, C], Uses = [PSW] in {
def ADDCCarryInrr : C166Inst16<0x10, (outs GR16:$dst),
                               (ins GR16:$lhs, GR16:$rhs, CARRY:$cin),
                               "addc\t$dst, $rhs"> {
  bits<4> dst;
  bits<4> rhs;
  let Inst{15-12} = dst;
  let Inst{11-8} = rhs;
  let Constraints = "$dst = $lhs";
  let isCommutable = 1;
}

def SUBCCarryInrr : C166Inst16<0x30, (outs GR16:$dst),
                               (ins GR16:$lhs, GR16:$rhs, CARRY:$cin),
                               "subc\t$dst, $rhs"> {
  bits<4> dst;
  bits<4> rhs;
  let Inst{15-12} = dst;
  let Inst{11-8} = rhs;
  let Constraints = "$dst = $lhs";
}

def ADDCCarryInrm
    : C166ALUIndirect<0x18, (outs GR16:$dst),
                      (ins GR16:$lhs, GR16Indirect:$base, CARRY:$cin), "addc"> {
  let Constraints = "$dst = $lhs";
}

def SUBCCarryInrm
    : C166ALUIndirect<0x38, (outs GR16:$dst),
                      (ins GR16:$lhs, GR16Indirect:$base, CARRY:$cin), "subc"> {
  let Constraints = "$dst = $lhs";
}

def ADDCCarryInrmPostInc
    : C166ALUPostInc<0x18, (outs GR16:$dst, GR16Indirect:$base_wb),
                     (ins GR16:$lhs, GR16Indirect:$base, CARRY:$cin), "addc"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
}

def SUBCCarryInrmPostInc
    : C166ALUPostInc<0x38, (outs GR16:$dst, GR16Indirect:$base_wb),
                     (ins GR16:$lhs, GR16Indirect:$base, CARRY:$cin), "subc"> {
  let Constraints = "$dst = $lhs,$base_wb = $base";
}
}

let Defs = [PSW] in {
def MOVrr : C166RR<0xf0, GR16, "mov">;
def MOVBrr : C166RR<0xf1, GR8, "movb">;
def MOVBZrr : C166Inst16<0xc0, (outs GR16:$dst),
                         (ins GR8:$src), "movbz\t$dst, $src"> {
  bits<4> dst;
  bits<4> src;
  let Inst{15-12} = src;
  let Inst{11-8} = dst;
}
def MOVBSrr : C166Inst16<0xd0, (outs GR16:$dst),
                         (ins GR8:$src), "movbs\t$dst, $src"> {
  bits<4> dst;
  bits<4> src;
  let Inst{15-12} = src;
  let Inst{11-8} = dst;
}
def MOVri4 : C166RI4<0xe0, GR16, "mov"> {
  let Pattern = [(set GR16:$dst, uimm4:$src)];
  let isAsCheapAsAMove = 1;
  let isReMaterializable = 1;
}
def MOVBri4 : C166RI4<0xe1, GR8, "movb">;
def MOVBri8 : C166Inst32<0xe7, (outs directbytereg:$dst),
                        (ins byteimmfull:$src), "movb\t$dst, $src"> {
  bits<8> dst;
  bits<8> src;
  let Inst{15-8} = dst;
  let Inst{15-12} = 0xf;
  let Inst{23-16} = src;
  let DecoderMethod = "decodeByteRegImmediate";
}
def MOVrm : C166RM<0xa8, "mov">;
}

let Uses = [MDC], Defs = [MDH, MDL, MDC, PSW, C] in {
def MULrr : C166Inst16<0x0b, (outs),
                        (ins GR16:$lhs, GR16:$rhs),
                        "mul\t$lhs, $rhs"> {
  bits<4> lhs;
  bits<4> rhs;
  let Inst{15-12} = lhs;
  let Inst{11-8} = rhs;
}

def MULUrr : C166Inst16<0x1b, (outs),
                         (ins GR16:$lhs, GR16:$rhs),
                         "mulu\t$lhs, $rhs"> {
  bits<4> lhs;
  bits<4> rhs;
  let Inst{15-12} = lhs;
  let Inst{11-8} = rhs;
}
}

class C166DIV<bits<8> Opcode, string Mnemonic>
    : C166Inst16<Opcode, (outs), (ins GR16:$src),
                  !strconcat(Mnemonic, "\t$src")> {
  bits<4> src;
  let Inst{15-12} = src;
  let Inst{11-8} = src;
  let Uses = [MDH, MDL, MDC];
  let Defs = [MDH, MDL, MDC, PSW, C];
  let DecoderMethod = "decodeDIV";
}

def DIVr : C166DIV<0x4b, "div">;
def DIVUr : C166DIV<0x5b, "divu">;
def DIVLr : C166DIV<0x6b, "divl">;
def DIVLUr : C166DIV<0x7b, "divlu">;

let isCodeGenOnly = 1, isPseudo = 1, Uses = [MDC],
    Defs = [MDH, MDL, MDC, PSW, C] in
def UDIVREM32_16 : C166Pseudo<(outs GR32:$dst),
                              (ins GR32:$dividend, GR16:$divisor)>;

let isCodeGenOnly = 1, isPseudo = 1, Uses = [MDC],
    Defs = [MDH, MDL, MDC, PSW, C] in
def UDIVREM32_16_FULL : C166Pseudo<(outs GR32:$quotient, GR16:$remainder),
                                   (ins GR32:$dividend, GR16:$divisor)> {
  let Constraints = "@earlyclobber $quotient";
}

def : Pat<(C166udivrem32by16 GR32:$dividend, GR16:$divisor),
          (UDIVREM32_16_FULL GR32:$dividend, GR16:$divisor)>;

let Defs = [PSW] in {
def MOVBrm : C166Inst16<0xa9, (outs GR8:$dst),
                        (ins GR16:$base), "movb\t$dst, [$base]"> {
  bits<4> dst;
  bits<4> base;
  let Inst{15-12} = dst;
  let Inst{11-8} = base;
  let mayLoad = 1;
}

def MOVBrmPostInc : C166Inst16<0x99, (outs GR8:$dst, GR16:$base_wb),
                               (ins GR16:$base),
                               "movb\t$dst, [${base}+]"> {
  bits<4> dst;
  bits<4> base;
  let Inst{15-12} = dst;
  let Inst{11-8} = base;
  let Constraints = "$base_wb = $base,@earlyclobber $dst";
  let mayLoad = 1;
}

def MOVmr : C166Inst16<0xb8, (outs),
                       (ins GR16:$base, GR16:$src),
                       "mov\t[$base], $src"> {
  bits<4> base;
  bits<4> src;
  let Inst{15-12} = src;
  let Inst{11-8} = base;
  let mayStore = 1;
}

def MOVmm : C166Inst16<0xc8, (outs),
                       (ins GR16:$dst_base, GR16:$src_base),
                       "mov\t[$dst_base], [$src_base]"> {
  bits<4> dst_base;
  bits<4> src_base;
  let Inst{15-12} = dst_base;
  let Inst{11-8} = src_base;
  let mayLoad = 1;
  let mayStore = 1;
}

def MOVBmm : C166Inst16<0xc9, (outs),
                        (ins GR16:$dst_base, GR16:$src_base),
                        "movb\t[$dst_base], [$src_base]"> {
  bits<4> dst_base;
  bits<4> src_base;
  let Inst{15-12} = dst_base;
  let Inst{11-8} = src_base;
  let mayLoad = 1;
  let mayStore = 1;
}

def MOVBmr : C166Inst16<0xb9, (outs),
                        (ins GR16:$base, GR8:$src),
                        "movb\t[$base], $src"> {
  bits<4> base;
  bits<4> src;
  let Inst{15-12} = src;
  let Inst{11-8} = base;
  let mayStore = 1;
}
}

class C166EXTRegister<bits<2> Mode, string Mnemonic>
    : C166Inst16<0xdc, (outs), (ins GR16:$value, sequence_count:$count),
                 !strconcat(Mnemonic, "\t$value, $count")> {
  bits<4> value;
  bits<2> count;
  let Inst{15-14} = Mode;
  let Inst{13-12} = count;
  let Inst{11-8} = value;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeEXTRegister";
}

def EXTPr : C166EXTRegister<0b01, "extp">;
def EXTSr : C166EXTRegister<0b00, "exts">;
def EXTPRr : C166EXTRegister<0b11, "extpr">;
def EXTSRr : C166EXTRegister<0b10, "extsr">;

let Defs = [PSW] in
class C166MOVPreDec<bits<8> Opcode, RegisterClass Reg, string Mnemonic>
    : C166Inst16<Opcode, (outs GR16:$base_wb),
                 (ins GR16:$base, Reg:$src),
                 !strconcat(Mnemonic, "\t[-$base], $src")> {
  bits<4> base;
  bits<4> src;
  let Inst{15-12} = src;
  let Inst{11-8} = base;
  let Constraints = "$base_wb = $base";
  let mayStore = 1;
  let Pattern = [];
}
def MOVmrPreDec : C166MOVPreDec<0x88, GR16, "mov">;
def MOVBmrPreDec : C166MOVPreDec<0x89, GR8, "movb">;

let Defs = [PSW] in
class C166MOVDirectIndirect<bits<8> Opcode, string Mnemonic, bit ToIndirect>
    : C166Inst32<Opcode, (outs),
                 (ins GR16:$base, addressAbs16:$address),
                 !strconcat(Mnemonic, !if(ToIndirect, "\t[$base], $address",
                                                     "\t$address, [$base]"))> {
  bits<4> base;
  bits<16> address;
  let Inst{15-12} = 0;
  let Inst{11-8} = base;
  let Inst{31-16} = address;
  let mayLoad = 1;
  let mayStore = 1;
}
def MOVindm : C166MOVDirectIndirect<0x84, "mov", 1>;
def MOVmind : C166MOVDirectIndirect<0x94, "mov", 0>;
def MOVBindm : C166MOVDirectIndirect<0xa4, "movb", 1>;
def MOVBmind : C166MOVDirectIndirect<0xb4, "movb", 0>;

let Defs = [PSW] in
def MOVrmPostInc : C166Inst16<0x98, (outs GR16:$dst, GR16:$base_wb),
                              (ins GR16:$base),
                              "mov\t$dst, [${base}+]"> {
  bits<4> dst;
  bits<4> base;
  let Inst{15-12} = dst;
  let Inst{11-8} = base;
  let Constraints = "$base_wb = $base";
  let mayLoad = 1;
  let Pattern = [];
}

let Defs = [PSW] in {
def MOVmmDstPostInc
    : C166Inst16<0xd8, (outs GR16:$dst_base_wb),
                  (ins GR16:$dst_base, GR16:$src_base),
                  "mov\t[${dst_base}+], [$src_base]"> {
  bits<4> dst_base;
  bits<4> src_base;
  let Inst{15-12} = dst_base;
  let Inst{11-8} = src_base;
  let Constraints = "$dst_base_wb = $dst_base";
  let mayLoad = 1;
  let mayStore = 1;
}

def MOVmmSrcPostInc
    : C166Inst16<0xe8, (outs GR16:$src_base_wb),
                  (ins GR16:$dst_base, GR16:$src_base),
                  "mov\t[$dst_base], [${src_base}+]"> {
  bits<4> dst_base;
  bits<4> src_base;
  let Inst{15-12} = dst_base;
  let Inst{11-8} = src_base;
  let Constraints = "$src_base_wb = $src_base";
  let mayLoad = 1;
  let mayStore = 1;
}

def MOVBmmDstPostInc
    : C166Inst16<0xd9, (outs GR16:$dst_base_wb),
                  (ins GR16:$dst_base, GR16:$src_base),
                  "movb\t[${dst_base}+], [$src_base]"> {
  bits<4> dst_base;
  bits<4> src_base;
  let Inst{15-12} = dst_base;
  let Inst{11-8} = src_base;
  let Constraints = "$dst_base_wb = $dst_base";
  let mayLoad = 1;
  let mayStore = 1;
}

def MOVBmmSrcPostInc
    : C166Inst16<0xe9, (outs GR16:$src_base_wb),
                  (ins GR16:$dst_base, GR16:$src_base),
                  "movb\t[$dst_base], [${src_base}+]"> {
  bits<4> dst_base;
  bits<4> src_base;
  let Inst{15-12} = dst_base;
  let Inst{11-8} = src_base;
  let Constraints = "$src_base_wb = $src_base";
  let mayLoad = 1;
  let mayStore = 1;
}
}

def NOP : C166Inst16<0xcc, (outs), (ins), "nop"> {
  let Inst{15-8} = 0;
}

class C166Sequence<bits<2> Mode, Operand Count, string Mnemonic>
    : C166Inst16<0xd1, (outs), (ins Count:$count),
                 !strconcat(Mnemonic, "\t$count")> {
  bits<2> count;
  let Inst{15-14} = Mode;
  let Inst{13-12} = count;
  let Inst{11-8} = 0;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeSequenceCount";
}

def ATOMIC : C166Sequence<0b00, atomic_count, "atomic">;
def EXTR : C166Sequence<0b10, sequence_count, "extr">;

// Direct bit operations encode the eight-bit word address in the second byte
// and the bit number in the high nibble of the opcode byte.
let Defs = [PSW, C], mayLoad = 1, mayStore = 1,
    hasSideEffects = 1 in {
def BCLR : C166Inst16<0x0e, (outs), (ins bit_address:$address),
                       "bclr\t$address"> {
  bits<12> address;
  let Inst{15-8} = address{11-4};
  let Inst{7-4} = address{3-0};
}

def BSET : C166Inst16<0x0f, (outs), (ins bit_address:$address),
                       "bset\t$address"> {
  bits<12> address;
  let Inst{15-8} = address{11-4};
  let Inst{7-4} = address{3-0};
}
}

// BFLDL encodes mask before data, while BFLDH swaps the two immediate bytes.
let Defs = [PSW, C], mayLoad = 1, mayStore = 1,
    hasSideEffects = 1 in {
def BFLDL : C166Inst32<0x0a, (outs),
                       (ins bit_offset:$address, uimm8:$mask, uimm8:$data),
                       "bfldl\t$address, $mask, $data"> {
  bits<8> address;
  bits<8> mask;
  bits<8> data;
  let Inst{15-8} = address;
  let Inst{23-16} = mask;
  let Inst{31-24} = data;
}

def BFLDH : C166Inst32<0x1a, (outs),
                       (ins bit_offset:$address, uimm8:$mask, uimm8:$data),
                       "bfldh\t$address, $mask, $data"> {
  bits<8> address;
  bits<8> mask;
  bits<8> data;
  let Inst{15-8} = address;
  let Inst{23-16} = data;
  let Inst{31-24} = mask;
}
}

class C166BitBinary<bits<8> Opcode, string Mnemonic>
    : C166Inst32<Opcode, (outs),
                  (ins bit_address:$dst, bit_address:$src),
                  !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<12> dst;
  bits<12> src;
  let Inst{15-8} = src{11-4};
  let Inst{23-16} = dst{11-4};
  let Inst{27-24} = dst{3-0};
  let Inst{31-28} = src{3-0};
}

let Defs = [PSW, C], mayLoad = 1, mayStore = 1,
    hasSideEffects = 1 in {
def BMOV : C166BitBinary<0x4a, "bmov">;
def BMOVN : C166BitBinary<0x3a, "bmovn">;
def BAND : C166BitBinary<0x6a, "band">;
def BOR : C166BitBinary<0x5a, "bor">;
def BXOR : C166BitBinary<0x7a, "bxor">;
}

let Defs = [PSW, C], mayLoad = 1, mayStore = 0,
    hasSideEffects = 1 in
def BCMP : C166BitBinary<0x2a, "bcmp">;

class C166BitBranch<bits<8> Opcode, string Mnemonic>
    : C166Inst32<Opcode, (outs),
                  (ins bit_address:$address, bitbrtarget:$target),
                  !strconcat(Mnemonic, "\t$address, $target")> {
  bits<12> address;
  bits<8> target;
  let Inst{15-8} = address{11-4};
  let Inst{23-16} = target;
  let Inst{27-24} = 0;
  let Inst{31-28} = address{3-0};
  let isBranch = 1;
  let isTerminator = 1;
  let mayLoad = 1;
  let hasSideEffects = 1;
}

def JB : C166BitBranch<0x8a, "jb">;
def JNB : C166BitBranch<0x9a, "jnb">;
let Defs = [PSW, C], mayStore = 1 in {
def JBC : C166BitBranch<0xaa, "jbc">;
def JNBS : C166BitBranch<0xba, "jnbs">;
}

let Defs = [SP, PSW], Uses = [SP] in
def PUSH : C166Inst16<0xec, (outs), (ins pushpopreg:$src), "push\t$src"> {
  bits<8> src;
  let Inst{15-8} = src;
  let mayStore = 1;
  let hasSideEffects = 1;
  let DecoderMethod = "decodePUSH";
}

let Defs = [SP, PSW], Uses = [SP] in
def POP : C166Inst16<0xfc, (outs pushpopreg:$dst), (ins), "pop\t$dst"> {
  bits<8> dst;
  let Inst{15-8} = dst;
  let mayLoad = 1;
  let hasSideEffects = 1;
  let DecoderMethod = "decodePUSH";
}

def RETS : C166Inst16<0xdb, (outs), (ins), "rets"> {
  let Inst{15-8} = 0;
  let isReturn = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let Pattern = [(C166ret)];
}

def RET : C166Inst16<0xcb, (outs), (ins), "ret"> {
  let Inst{15-8} = 0;
  let isReturn = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let Pattern = [(C166nearret)];
}

let Defs = [SP, PSW], Uses = [SP] in
def RETP : C166Inst16<0xeb, (outs pushpopreg:$dst), (ins), "retp\t$dst"> {
  bits<8> dst;
  let Inst{15-8} = dst;
  let isReturn = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let mayLoad = 1;
  let hasSideEffects = 1;
  let DecoderMethod = "decodePUSH";
}

let Defs = [SP, CSP], Uses = [SP, CSP, PSW] in
def TRAP : C166Inst16<0x9b, (outs), (ins uimm7:$vector),
                     "trap\t$vector"> {
  bits<7> vector;
  let Inst{15-9} = vector;
  let Inst{8} = 0;
  let isCall = 1;
  let mayStore = 1;
  let hasSideEffects = 1;
}

let Defs = [SP, CSP, PSW, C], Uses = [SP] in
def RETI : C166Inst16<0xfb, (outs), (ins), "reti"> {
  let Inst{15-8} = 0x88;
  let isReturn = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let Pattern = [(C166interruptret)];
}

let Defs = [SP], Uses = [SP] in
class C166SCXT<bits<8> Opcode, Operand Source>
    : C166Inst32<Opcode, (outs pushpopreg:$dst),
                 (ins pushpopreg:$old, Source:$src), "scxt\t$dst, $src"> {
  bits<8> dst;
  bits<16> src;
  let Inst{15-8} = dst;
  let Inst{31-16} = src;
  let Constraints = "$dst = $old";
  let mayStore = 1;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeSCXT";
}

def SCXTri16 : C166SCXT<0xc6, immediate16>;
def SCXTrm : C166SCXT<0xd6, memory16> {
  let mayLoad = 1;
}

class C166ProtectedInst<bits<8> Opcode, string Mnemonic>
    : C166Inst32<Opcode, (outs), (ins), Mnemonic> {
  let Inst{15-8} = !xor(Opcode, 0xff);
  let Inst{23-16} = Opcode;
  let Inst{31-24} = Opcode;
  let hasSideEffects = 1;
}

def DISWDT : C166ProtectedInst<0xa5, "diswdt">;
def EINIT : C166ProtectedInst<0xb5, "einit">;
def IDLE : C166ProtectedInst<0x87, "idle">;
def PWRDN : C166ProtectedInst<0x97, "pwrdn">;
def SRVWDT : C166ProtectedInst<0xa7, "srvwdt">;
def SRST : C166ProtectedInst<0xb7, "srst"> {
  let isTerminator = 1;
  let isBarrier = 1;
}

let Defs = [PSW] in
def MOVri16 : C166Inst32<0xe6, (outs GR16:$dst), (ins imm16full:$src),
                          "mov\t$dst, $src"> {
  bits<4> dst;
  bits<16> src;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = dst;
  let Inst{31-16} = src;
  let Pattern = [(set GR16:$dst, uimm16:$src)];
  let isAsCheapAsAMove = 1;
  let isReMaterializable = 1;
}

let Defs = [PSW, C] in
def CMPri16 : C166Inst32<0x46, (outs), (ins GR16:$dst, imm16alu:$src),
                         "cmp\t$dst, $src"> {
  bits<4> dst;
  bits<16> src;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = dst;
  let Inst{31-16} = src;
}

// The architectural `MOV reg,#data16` format uses an eight-bit short direct
// register address.  GPRs occupy F0h..FFh (MOVri16 above), while SFRs occupy
// their native 00h..EFh short addresses.  Keep the SFR form explicit so
// startup code can initialize CP, stack limits, DPPs and PSW without raw byte
// directives.
let Defs = [PSW] in
def MOVsfri16 : C166Inst32<0xe6, (outs sfrshort:$dst), (ins immediate16:$src),
                            "mov\t$dst, $src"> {
  bits<8> dst;
  bits<16> src;
  let Inst{15-8} = dst;
  let Inst{31-16} = src;
  let DecoderMethod = "decodeSFRLongOperand<false>";
  let hasSideEffects = 1;
}

class C166EXTImmediate<bits<2> Mode, Operand Source, string Mnemonic>
    : C166Inst32<0xd7, (outs), (ins Source:$value, sequence_count:$count),
                 !strconcat(Mnemonic, "\t$value, $count")> {
  bits<10> value;
  bits<2> count;
  let Inst{15-14} = Mode;
  let Inst{13-12} = count;
  let Inst{11-8} = 0;
  let Inst{25-16} = value;
  let Inst{31-26} = 0;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeEXTImmediate";
}

def EXTPp : C166EXTImmediate<0b01, addressPag10, "extp">;
def EXTPRp : C166EXTImmediate<0b11, addressPag10, "extpr">;
let Inst{25-24} = 0 in {
def EXTSs : C166EXTImmediate<0b00, extensionSegment8, "exts">;
def EXTSRs : C166EXTImmediate<0b10, extensionSegment8, "extsr">;
}

let Defs = [PSW] in {
def MOVBsfri8 : C166SFRALU<0xe7, "movb", immediate8> {
  let Inst{31-24} = ?;
  let DecoderMethod = "decodeSFRLongOperand<true, 8>";
}
def MOVBsfrm : C166SFRALU<0xf3, "movb", addressAbs16> {
  let mayLoad = 1;
}
def MOVBsfrsfr : C166SFRALU<0xf3, "movb", sfrdirect> {
  let isAsmParserOnly = 1;
}
def MOVBmsfr : C166MemorySFR<0xf7, "movb">;
def MOVBabsgd : C166Inst32<0xf3, (outs GR8:$dst),
                            (ins addressAbs16:$address),
                            "movb\t$dst, $address"> {
  bits<4> dst;
  bits<16> address;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = dst;
  let Inst{31-16} = address;
  let mayLoad = 1;
}

def MOVgd : C166Inst32<0xf2, (outs GR16:$dst),
                        (ins addressPof14:$address),
                        "mov\t$dst, $address"> {
  bits<4> dst;
  bits<14> address;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = dst;
  let Inst{29-16} = address;
  let Inst{31-30} = 0;
  let mayLoad = 1;
}

// Small-model data pointers contain the complete 16-bit direct address.  The
// top two bits select DPP0..DPP3 and therefore must not be forced to the POF14
// selector used by paged far accesses or by fixed-DPP near objects.
def MOVabsgd : C166Inst32<0xf2, (outs GR16:$dst),
                           (ins addressAbs16:$address),
                           "mov\t$dst, $address"> {
  bits<4> dst;
  bits<16> address;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = dst;
  let Inst{31-16} = address;
  let mayLoad = 1;
  let DecoderMethod = "decodeMOVgsfr";
  let isAsmParserOnly = 1;
}

// The short/long MOV form addresses an SFR through its full 16-bit direct
// address.  For example DPP1 is short register 01h and direct address FE02h,
// so `mov r15, dpp1` is encoded as F2 FF 02 FE.
def MOVgsfr : C166Inst32<0xf2, (outs GR16:$dst),
                          (ins sfrdirect:$src),
                          "mov\t$dst, $src"> {
  bits<4> dst;
  bits<16> src;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = dst;
  let Inst{31-16} = src;
  let DecoderMethod = "decodeMOVgsfr";
}

// The destination byte in the direct-to-direct MOV form names any direct
// register, not only a GPR.  Keep the SFR-to-SFR form distinct so its explicit
// operands model both architectural register accesses.
def MOVsfrsfr : C166Inst32<0xf2, (outs sfrshort:$dst),
                            (ins sfrdirect:$src),
                            "mov\t$dst, $src"> {
  bits<8> dst;
  bits<16> src;
  let Inst{15-8} = dst;
  let Inst{31-16} = src;
  let DecoderMethod = "decodeMOVsfrsfr";
}

class C166SFRMemoryLoad<bits<8> Opcode, string Mnemonic>
    : C166Inst32<Opcode, (outs sfrshort:$dst),
                 (ins addressAbs16:$src), !strconcat(Mnemonic, "\t$dst, $src")> {
  bits<8> dst;
  bits<16> src;
  let Inst{15-8} = dst;
  let Inst{31-16} = src;
  let mayLoad = 1;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeSFRLongOperand<false>";
}
def MOVsfrm : C166SFRMemoryLoad<0xf2, "mov"> {
  let isAsmParserOnly = 1;
}
def MOVBSsfrm : C166SFRMemoryLoad<0xd2, "movbs">;
def MOVBZsfrm : C166SFRMemoryLoad<0xc2, "movbz">;

class C166MOVExtendStore<bits<8> Opcode, string Mnemonic>
    : C166Inst32<Opcode, (outs), (ins addressAbs16:$address, GR8:$src),
                 !strconcat(Mnemonic, "\t$address, $src")> {
  bits<4> src;
  bits<16> address;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = src;
  let Inst{31-16} = address;
  let mayStore = 1;
}
def MOVBSmg : C166MOVExtendStore<0xd5, "movbs">;
def MOVBZmg : C166MOVExtendStore<0xc5, "movbz">;
def MOVBSmsfr : C166MemorySFR<0xd5, "movbs">;
def MOVBZmsfr : C166MemorySFR<0xc5, "movbz">;

def MOVmsfr : C166MemorySFR<0xf6, "mov">;

def MOVsfrg : C166Inst32<0xf6, (outs sfrdirect:$dst),
                          (ins GR16:$src),
                          "mov\t$dst, $src"> {
  bits<16> dst;
  bits<4> src;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = src;
  let Inst{31-16} = dst;
  let DecoderMethod = "decodeMOVsfrg";
}

// Alternate assembler/codegen form for the same architectural direct-16 MOV.
// Unlike MOVsfrg its destination may be a relocatable absolute address.  The
// canonical decoder remains MOVsfrg and prints a numeric address when it is
// not one of the known SFRs.
def MOVabsdg : C166Inst32<0xf6, (outs),
                           (ins addressAbs16:$address, GR16:$src),
                           "mov\t$address, $src"> {
  bits<16> address;
  bits<4> src;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = src;
  let Inst{31-16} = address;
  let mayStore = 1;
  let isAsmParserOnly = 1;
}

def MOVdg : C166Inst32<0xf6, (outs),
                        (ins addressPof14:$address, GR16:$src),
                        "mov\t$address, $src"> {
  bits<4> src;
  bits<14> address;
  let Inst{15-12} = 0xf;
  let Inst{11-8} = src;
  let Inst{29-16} = address;
  let Inst{31-30} = 0;
  let mayStore = 1;
}

def MOVBZgd : C166Inst32<0xc2, (outs directreg:$dst),
                         (ins addressPof14:$address),
                         "movbz\t$dst, $address"> {
  bits<8> dst;
  bits<14> address;
  let Inst{15-8} = dst;
  let Inst{15-12} = 0xf;
  let Inst{29-16} = address;
  let Inst{31-30} = 0;
  let mayLoad = 1;
  let DecoderMethod = "decodeMOVBZgd<14>";
}

def MOVBZabsgd : C166Inst32<0xc2, (outs directreg:$dst),
                            (ins addressAbs16:$address),
                            "movbz\t$dst, $address"> {
  bits<8> dst;
  bits<16> address;
  let Inst{15-8} = dst;
  let Inst{15-12} = 0xf;
  let Inst{31-16} = address;
  let mayLoad = 1;
  let DecoderMethod = "decodeMOVBZgd<16>";
}

def MOVBSgd : C166Inst32<0xd2, (outs directreg:$dst),
                         (ins addressPof14:$address),
                         "movbs\t$dst, $address"> {
  bits<8> dst;
  bits<14> address;
  let Inst{15-8} = dst;
  let Inst{15-12} = 0xf;
  let Inst{29-16} = address;
  let Inst{31-30} = 0;
  let mayLoad = 1;
  let DecoderMethod = "decodeMOVBZgd<14>";
}

def MOVBSabsgd : C166Inst32<0xd2, (outs directreg:$dst),
                            (ins addressAbs16:$address),
                            "movbs\t$dst, $address"> {
  bits<8> dst;
  bits<16> address;
  let Inst{15-8} = dst;
  let Inst{15-12} = 0xf;
  let Inst{31-16} = address;
  let mayLoad = 1;
  let DecoderMethod = "decodeMOVBZgd<16>";
}

def MOVBdg : C166Inst32<0xf7, (outs),
                        (ins addressPof14:$address, directbytereg:$src),
                        "movb\t$address, $src"> {
  bits<8> src;
  bits<14> address;
  let Inst{15-8} = src;
  let Inst{15-12} = 0xf;
  let Inst{29-16} = address;
  let Inst{31-30} = 0;
  let mayStore = 1;
  let DecoderMethod = "decodeMOVBdg<14>";
}

def MOVBabsdg : C166Inst32<0xf7, (outs),
                           (ins addressAbs16:$address,
                                directbytereg:$src),
                           "movb\t$address, $src"> {
  bits<8> src;
  bits<16> address;
  let Inst{15-8} = src;
  let Inst{15-12} = 0xf;
  let Inst{31-16} = address;
  let mayStore = 1;
  let DecoderMethod = "decodeMOVBdg<16>";
}

multiclass C166MOVgdDPP<bits<2> Selector, Operand AddressOperand,
                        string Suffix> {
  def NAME # Suffix : C166Inst32<0xf2, (outs GR16:$dst),
                                  (ins AddressOperand:$address),
                                  "mov\t$dst, $address"> {
    bits<4> dst;
    bits<14> address;
    let Inst{15-12} = 0xf;
    let Inst{11-8} = dst;
    let Inst{29-16} = address;
    let Inst{31-30} = Selector;
    let mayLoad = 1;
  }
}

multiclass C166MOVdgDPP<bits<2> Selector, Operand AddressOperand,
                        string Suffix> {
  def NAME # Suffix : C166Inst32<0xf6, (outs),
                                  (ins AddressOperand:$address, GR16:$src),
                                  "mov\t$address, $src"> {
    bits<4> src;
    bits<14> address;
    let Inst{15-12} = 0xf;
    let Inst{11-8} = src;
    let Inst{29-16} = address;
    let Inst{31-30} = Selector;
    let mayStore = 1;
  }
}

multiclass C166MOVBLoadDPP<bits<8> Opcode, bits<2> Selector,
                           Operand AddressOperand, string Suffix,
                           string Mnemonic> {
  def NAME # Suffix : C166Inst32<Opcode, (outs directreg:$dst),
                                  (ins AddressOperand:$address),
                                  !strconcat(Mnemonic, "\t$dst, $address")> {
    bits<8> dst;
    bits<14> address;
    let Inst{15-8} = dst;
    let Inst{15-12} = 0xf;
    let Inst{29-16} = address;
    let Inst{31-30} = Selector;
    let mayLoad = 1;
    let DecoderMethod = "decodeMOVBZgd<14>";
  }
}

multiclass C166MOVBStoreDPP<bits<2> Selector, Operand AddressOperand,
                            string Suffix> {
  def NAME # Suffix : C166Inst32<0xf7, (outs),
                                  (ins AddressOperand:$address,
                                       directbytereg:$src),
                                  "movb\t$address, $src"> {
    bits<8> src;
    bits<14> address;
    let Inst{15-8} = src;
    let Inst{15-12} = 0xf;
    let Inst{29-16} = address;
    let Inst{31-30} = Selector;
    let mayStore = 1;
    let DecoderMethod = "decodeMOVBdg<14>";
  }
}

defm MOVgd : C166MOVgdDPP<0b01, addressDPP1, "DPP1">;
defm MOVgd : C166MOVgdDPP<0b10, addressDPP2, "DPP2">;
defm MOVdg : C166MOVdgDPP<0b01, addressDPP1, "DPP1">;
defm MOVdg : C166MOVdgDPP<0b10, addressDPP2, "DPP2">;
defm MOVBZgd : C166MOVBLoadDPP<0xc2, 0b01, addressDPP1, "DPP1", "movbz">;
defm MOVBZgd : C166MOVBLoadDPP<0xc2, 0b10, addressDPP2, "DPP2", "movbz">;
defm MOVBSgd : C166MOVBLoadDPP<0xd2, 0b01, addressDPP1, "DPP1", "movbs">;
defm MOVBSgd : C166MOVBLoadDPP<0xd2, 0b10, addressDPP2, "DPP2", "movbs">;
defm MOVBdg : C166MOVBStoreDPP<0b01, addressDPP1, "DPP1">;
defm MOVBdg : C166MOVBStoreDPP<0b10, addressDPP2, "DPP2">;
}

let Defs = [PSW] in {
def MOVrm16 : C166Inst32<0xd4, (outs GR16:$dst),
                         (ins GR16:$base, displacement16:$disp),
                         "mov\t$dst, [$base + $disp]"> {
  bits<4> dst;
  bits<4> base;
  bits<16> disp;
  let Inst{15-12} = dst;
  let Inst{11-8} = base;
  let Inst{31-16} = disp;
  let mayLoad = 1;
}

def MOVmr16 : C166Inst32<0xc4, (outs),
                         (ins GR16:$base, displacement16:$disp, GR16:$src),
                         "mov\t[$base + $disp], $src"> {
  bits<4> base;
  bits<4> src;
  bits<16> disp;
  let Inst{15-12} = src;
  let Inst{11-8} = base;
  let Inst{31-16} = disp;
  let mayStore = 1;
}

def MOVBrm16 : C166Inst32<0xf4, (outs GR8:$dst),
                          (ins GR16:$base, displacement16:$disp),
                          "movb\t$dst, [$base + $disp]"> {
  bits<4> dst;
  bits<4> base;
  bits<16> disp;
  let Inst{15-12} = dst;
  let Inst{11-8} = base;
  let Inst{31-16} = disp;
  let mayLoad = 1;
}

def MOVBmr16 : C166Inst32<0xe4, (outs),
                          (ins GR16:$base, displacement16:$disp, GR8:$src),
                          "movb\t[$base + $disp], $src"> {
  bits<4> base;
  bits<4> src;
  bits<16> disp;
  let Inst{15-12} = src;
  let Inst{11-8} = base;
  let Inst{31-16} = disp;
  let mayStore = 1;
}
}

class C166JMPR<bits<8> Opcode, string CCName>
    : C166Inst16<Opcode, (outs), (ins brtarget:$target),
                  !strconcat("jmpr\t", CCName, ", $target")> {
  bits<8> target;
  let Inst{15-8} = target;
  let isBranch = 1;
  let isTerminator = 1;
}

def JMPR_UC  : C166JMPR<0x0d, "cc_uc">  { let isBarrier = 1; }
let Uses = [PSW] in {
def JMPR_NET : C166JMPR<0x1d, "cc_net">;
def JMPR_EQ  : C166JMPR<0x2d, "cc_eq">;
def JMPR_NE  : C166JMPR<0x3d, "cc_ne">;
def JMPR_V   : C166JMPR<0x4d, "cc_v">;
def JMPR_NV  : C166JMPR<0x5d, "cc_nv">;
def JMPR_N   : C166JMPR<0x6d, "cc_n">;
def JMPR_NN  : C166JMPR<0x7d, "cc_nn">;
def JMPR_SGT : C166JMPR<0xad, "cc_sgt">;
def JMPR_SLE : C166JMPR<0xbd, "cc_sle">;
def JMPR_SLT : C166JMPR<0xcd, "cc_slt">;
def JMPR_SGE : C166JMPR<0xdd, "cc_sge">;
}
let Uses = [PSW, C] in {
def JMPR_ULT : C166JMPR<0x8d, "cc_ult">;
def JMPR_UGE : C166JMPR<0x9d, "cc_uge">;
def JMPR_UGT : C166JMPR<0xed, "cc_ugt">;
def JMPR_ULE : C166JMPR<0xfd, "cc_ule">;
}

// MC relaxation reserves the complete replacement sequence only when a JMPR
// target is unresolved or outside the short range.  LLD then keeps the short
// branch or rewrites the slot to JMPS after final layout.
def PseudoJMPRRelax
    : C166Pseudo<(outs), (ins uimm8:$opcode, brtarget:$target)> {
  let Size = 6;
}

def PseudoJMPRRelaxUC : C166Pseudo<(outs), (ins brtarget:$target)> {
  let Size = 4;
}

// NET has no inverse condition; its replacement needs two short branches.
def PseudoJMPRRelaxNET
    : C166Pseudo<(outs), (ins uimm8:$opcode, brtarget:$target)> {
  let Size = 8;
}

def PseudoBitBranchRelax
    : C166Pseudo<(outs),
                  (ins uimm8:$opcode, bit_address:$address,
                       bitbrtarget:$target)> {
  let Size = 8;
}
def PseudoBitBranchWritebackRelax
    : C166Pseudo<(outs),
                  (ins uimm8:$opcode, bit_address:$address,
                       bitbrtarget:$target)> {
  let Size = 10;
}

def JMPS : C166Inst32<0xfa, (outs),
                      (ins address8:$segment, address16:$offset),
                      "jmps\t$segment, $offset"> {
  bits<8> segment;
  bits<16> offset;
  let Inst{15-8} = segment;
  let Inst{31-16} = offset;
  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
}

class C166JMPA<bits<4> Condition, string CCName>
    : C166Inst32<0xea, (outs), (ins codeAddress16:$offset),
                  !strconcat("jmpa\t", CCName, ", $offset")> {
  bits<16> offset;
  let Inst{15-12} = Condition;
  let Inst{11-8} = 0;
  let Inst{31-16} = offset;
  let isBranch = 1;
  let isTerminator = 1;
}

def JMPA : C166JMPA<0x0, "cc_uc"> {
  let isBarrier = 1;
}
let Uses = [PSW] in {
def JMPA_NET : C166JMPA<0x1, "cc_net">;
def JMPA_EQ  : C166JMPA<0x2, "cc_eq">;
def JMPA_NE  : C166JMPA<0x3, "cc_ne">;
def JMPA_V   : C166JMPA<0x4, "cc_v">;
def JMPA_NV  : C166JMPA<0x5, "cc_nv">;
def JMPA_N   : C166JMPA<0x6, "cc_n">;
def JMPA_NN  : C166JMPA<0x7, "cc_nn">;
def JMPA_SGT : C166JMPA<0xa, "cc_sgt">;
def JMPA_SLE : C166JMPA<0xb, "cc_sle">;
def JMPA_SLT : C166JMPA<0xc, "cc_slt">;
def JMPA_SGE : C166JMPA<0xd, "cc_sge">;
}
let Uses = [PSW, C] in {
def JMPA_ULT : C166JMPA<0x8, "cc_ult">;
def JMPA_UGE : C166JMPA<0x9, "cc_uge">;
def JMPA_UGT : C166JMPA<0xe, "cc_ugt">;
def JMPA_ULE : C166JMPA<0xf, "cc_ule">;
}

let isCall = 1, isTerminator = 1, isReturn = 1, isBarrier = 1,
    isCodeGenOnly = 1 in {
def TAILJMPS : C166Inst32<0xfa, (outs),
                          (ins address8:$segment, address16:$offset,
                               variable_ops),
                          "jmps\t$segment, $offset"> {
  bits<8> segment;
  bits<16> offset;
  let Inst{15-8} = segment;
  let Inst{31-16} = offset;
}

def TAILJMPA : C166Inst32<0xea, (outs),
                          (ins codeAddress16:$offset, variable_ops),
                          "jmpa\tcc_uc, $offset"> {
  bits<16> offset;
  let Inst{15-8} = 0;
  let Inst{31-16} = offset;
}
}

class C166JMPI<bits<4> Condition, string CCName>
    : C166Inst16<0x9c, (outs), (ins GR16:$offset),
                  "jmpi\t" # CCName # ", [$offset]"> {
  bits<4> offset;
  let Inst{15-12} = Condition;
  let Inst{11-8} = offset;
  let isBranch = 1;
  let isIndirectBranch = 1;
  let isTerminator = 1;
}

def JMPI : C166JMPI<0, "cc_uc"> { let isBarrier = 1; }

// Calls observe the system stack pointer. The call/return pair restores it, so
// model it as a dependency rather than a caller-visible definition. CALLS also
// observes CSP because it saves the current code segment in the return frame.
let Uses = [SP, CSP] in
def CALLS : C166Inst32<0xda, (outs),
                       (ins address8:$segment, address16:$offset,
                            variable_ops),
                       "calls\t$segment, $offset"> {
  bits<8> segment;
  bits<16> offset;
  let Inst{15-8} = segment;
  let Inst{31-16} = offset;
  let isCall = 1;
}

// Both forms use an IP-only return address; PCALL also saves its register.
let Uses = [SP] in
def CALLR : C166Inst16<0xbb, (outs), (ins brtarget:$target),
                       "callr\t$target"> {
  bits<8> target;
  let Inst{15-8} = target;
  let isCall = 1;
}

let Defs = [SP, PSW], Uses = [SP] in
def PCALL : C166Inst32<0xe2, (outs),
                       (ins pushpopreg:$src, codeAddress16:$offset),
                       "pcall\t$src, $offset"> {
  bits<8> src;
  bits<16> offset;
  let Inst{15-8} = src;
  let Inst{31-16} = offset;
  let isCall = 1;
  let mayStore = 1;
  let hasSideEffects = 1;
  let DecoderMethod = "decodePCALL";
}

class C166CALLA<bits<4> Condition, string CCName>
    : C166Inst32<0xca, (outs),
                       (ins codeAddress16:$offset, variable_ops),
                       "calla\t" # CCName # ", $offset"> {
  bits<16> offset;
  let Inst{15-12} = Condition;
  let Inst{11-8} = 0;
  let Inst{31-16} = offset;
  let isCall = 1;
}

class C166CALLI<bits<4> Condition, string CCName>
    : C166Inst16<0xab, (outs), (ins GR16:$offset, variable_ops),
                  "calli\t" # CCName # ", [$offset]"> {
  bits<4> offset;
  let Inst{15-12} = Condition;
  let Inst{11-8} = offset;
  let isCall = 1;
}

let Uses = [SP] in {
def CALLA : C166CALLA<0, "cc_uc">;
def CALLI : C166CALLI<0, "cc_uc">;
}

multiclass C166ConditionalTransfer<bits<4> Condition, string CCName,
                                   list<Register> ConditionUses = [PSW]> {
  let Uses = ConditionUses in
  def "JMPI_" # NAME : C166JMPI<Condition, CCName>;
  let Uses = !listconcat(ConditionUses, [SP]) in {
    def "CALLA_" # NAME : C166CALLA<Condition, CCName>;
    def "CALLI_" # NAME : C166CALLI<Condition, CCName>;
  }
}

defm NET : C166ConditionalTransfer<0x1, "cc_net">;
defm EQ : C166ConditionalTransfer<0x2, "cc_eq">;
defm NE : C166ConditionalTransfer<0x3, "cc_ne">;
defm V : C166ConditionalTransfer<0x4, "cc_v">;
defm NV : C166ConditionalTransfer<0x5, "cc_nv">;
defm N : C166ConditionalTransfer<0x6, "cc_n">;
defm NN : C166ConditionalTransfer<0x7, "cc_nn">;
defm ULT : C166ConditionalTransfer<0x8, "cc_ult", [PSW, C]>;
defm UGE : C166ConditionalTransfer<0x9, "cc_uge", [PSW, C]>;
defm SGT : C166ConditionalTransfer<0xa, "cc_sgt">;
defm SLE : C166ConditionalTransfer<0xb, "cc_sle">;
defm SLT : C166ConditionalTransfer<0xc, "cc_slt">;
defm SGE : C166ConditionalTransfer<0xd, "cc_sge">;
defm UGT : C166ConditionalTransfer<0xe, "cc_ugt", [PSW, C]>;
defm ULE : C166ConditionalTransfer<0xf, "cc_ule", [PSW, C]>;

multiclass C166ConditionAlias<string Alias, string Suffix> {
  def : InstAlias<"jmpr\t" # Alias # ", $target",
                  (!cast<Instruction>("JMPR_" # Suffix) brtarget:$target), 0>;
  def : InstAlias<"jmpa\t" # Alias # ", $offset",
                  (!cast<Instruction>("JMPA_" # Suffix) codeAddress16:$offset), 0>;
  def : InstAlias<"jmpi\t" # Alias # ", [$offset]",
                  (!cast<Instruction>("JMPI_" # Suffix) GR16:$offset), 0>;
  def : InstAlias<"calla\t" # Alias # ", $offset",
                  (!cast<Instruction>("CALLA_" # Suffix) codeAddress16:$offset), 0>;
  def : InstAlias<"calli\t" # Alias # ", [$offset]",
                  (!cast<Instruction>("CALLI_" # Suffix) GR16:$offset), 0>;
}

defm : C166ConditionAlias<"cc_z", "EQ">;
defm : C166ConditionAlias<"cc_nz", "NE">;
defm : C166ConditionAlias<"cc_c", "ULT">;
defm : C166ConditionAlias<"cc_nc", "UGE">;
