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

def UImm8AsmOperand : AsmOperandClass {
  let Name = "UImm8";
  let DiagnosticType = "InvalidUImm8";
  let RenderMethod = "addImmOperands";
}

def UImm16AsmOperand : AsmOperandClass {
  let Name = "UImm16";
  let DiagnosticType = "InvalidUImm16";
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

def uimm8 : Operand<i16> {
  let ParserMatchClass = UImm8AsmOperand;
  let PrintMethod = "printImmediate";
}

def uimm16 : Operand<i16>, ImmLeaf<i16, [{ return isUInt<16>(Imm); }]> {
  let ParserMatchClass = UImm16AsmOperand;
  let PrintMethod = "printImmediate";
}

def imm16full : Operand<i16>, ImmLeaf<i16, [{ return isUInt<16>(Imm); }]> {
  let ParserMatchClass = UImm16LargeAsmOperand;
  let PrintMethod = "printImmediate16";
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
}

def directreg : RegisterOperand<GR16> {
  let EncoderMethod = "getDirectRegOpValue";
}

def pushpopreg : RegisterOperand<Direct16> {
  let EncoderMethod = "getDirectRegOpValue";
}

def directbytereg : RegisterOperand<GR8> {
  let EncoderMethod = "getDirectByteRegOpValue";
}

def sfrdirect : RegisterOperand<SFR16> {
  let EncoderMethod = "getSFRAddressOpValue";
}

def sfrshort : RegisterOperand<SFR16> {
  let EncoderMethod = "getSFRShortOpValue";
}

def address8 : Operand<i16> {
  let ParserMatchClass = Seg8AsmOperand;
  let PrintMethod = "printAddress";
}

def address16 : Operand<i16> {
  let ParserMatchClass = Sof16AsmOperand;
  let PrintMethod = "printAddress";
}

def codeAddress16 : Operand<i16> {
  let ParserMatchClass = Cof16AsmOperand;
  let PrintMethod = "printAddress";
}

def addressPag10 : Operand<i16> {
  let ParserMatchClass = Pag10AsmOperand;
  let PrintMethod = "printAddress";
}

def addressPof14 : Operand<i16> {
  let ParserMatchClass = Pof14AsmOperand;
  let PrintMethod = "printAddress";
}

def addressAbs16 : Operand<i16> {
  let ParserMatchClass = Abs16AsmOperand;
  let PrintMethod = "printAddress";
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
  let Defs = [R0, PSW];
  let Uses = [R0];
}

def ALLOCSP : C166Pseudo<(outs),
                         (ins uimm16:$amount, uimm16:$dynamic_offset)> {
  let Pattern = [(C166allocsp uimm16:$amount, uimm16:$dynamic_offset)];
  let hasSideEffects = 1;
  let Defs = [R0, PSW];
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
  let Defs = [PSW];
}

def SUB32rr : C166Pseudo<(outs GR32:$dst),
                         (ins GR32:$lhs, GR32:$rhs)> {
  let Constraints = "$dst = $lhs";
  let Pattern = [(set GR32:$dst, (sub GR32:$lhs, GR32:$rhs))];
  let Defs = [PSW];
}

def SHL32ri4 : C166Pseudo<(outs GR32:$dst),
                          (ins GR32:$lhs, uimm4:$amount)> {
  let Constraints = "$dst = $lhs";
  let Defs = [PSW];
}

def SEXT8rr : C166Pseudo<(outs GR16Byte:$dst),
                         (ins GR16Byte:$src)> {
  let Defs = [PSW];
}

// Keep extensions as one tuple operation through register allocation.
// Building the pair with REG_SEQUENCE lets the overlapping GR32 register
// class coalesce the source into the pair and spill it before the high word
// has been defined.
let Defs = [PSW] in {
def ZEXT16_32 : C166Pseudo<(outs GR32:$dst), (ins GR16:$src)>;
def SEXT16_32 : C166Pseudo<(outs GR32:$dst), (ins GR16:$src)>;

def FARADD32 : C166Pseudo<(outs GR32:$dst),
                          (ins GR32:$base, GR16:$offset)> {
  let Constraints = "$dst = $base";
}

def FARADD32i : C166Pseudo<(outs GR32:$dst),
                           (ins GR32:$base, i16imm:$offset)> {
  let Constraints = "$dst = $base";
}

let mayLoad = 1 in
def FARLOAD16 : C166Pseudo<(outs GR16:$dst), (ins GR32:$address)>;

let mayLoad = 1,
    Constraints = "$address_wb = $address,@earlyclobber $dst" in
def FARLOAD16_POSTINC : C166Pseudo<(outs GR16:$dst, GR32:$address_wb),
                                   (ins GR32:$address)>;

let mayStore = 1 in
def FARSTORE16 : C166Pseudo<(outs),
                            (ins GR32:$address, GR16:$value)>;

let mayLoad = 1 in {
def FARLOAD8Z : C166Pseudo<(outs GR16Byte:$dst), (ins GR32:$address)>;
def FARLOAD8S : C166Pseudo<(outs GR16Byte:$dst), (ins GR32:$address)>;
def FRAMELOAD8Z : C166Pseudo<(outs GR16Byte:$dst),
                             (ins i32imm:$fi, i16imm:$disp)>;
def FRAMELOAD8S : C166Pseudo<(outs GR16Byte:$dst),
                             (ins i32imm:$fi, i16imm:$disp)>;
def NEARLOAD8Z : C166Pseudo<(outs GR16Byte:$dst),
                            (ins GR16:$base, i16imm:$disp)>;
def NEARLOAD8S : C166Pseudo<(outs GR16Byte:$dst),
                            (ins GR16:$base, i16imm:$disp)>;
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
                           (ins GR32:$address, GR16Byte:$value)>;

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
def FARLOAD32 : C166Pseudo<(outs GR32:$dst), (ins GR32:$address)>;

let mayStore = 1 in
def FARSTORE32 : C166Pseudo<(outs),
                            (ins GR32:$address, GR32:$value)>;

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
def CONST32 : C166Pseudo<(outs GR32:$dst), (ins i32imm:$value)>;

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

let Defs = [PSW] in {
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
}

def CMPBRi : C166Pseudo<(outs),
                        (ins GR16:$lhs, i16imm:$rhs, i16imm:$cc,
                             brtarget:$target)> {
  let isBranch = 1;
  let isTerminator = 1;
}

def TEST32BR : C166Pseudo<(outs),
                          (ins GR16:$lhs, GR16:$rhs, i16imm:$cc,
                               brtarget:$target)> {
  let isBranch = 1;
  let isTerminator = 1;
  let Defs = [R1, PSW];
}

def CMP32BR : C166Pseudo<(outs),
                         (ins GR32:$lhs, GR32:$rhs, i16imm:$cc,
                              brtarget:$target)> {
  let isBranch = 1;
  let isTerminator = 1;
  let usesCustomInserter = 1;
}

let Uses = [PSW] in
def FLAGSBR : C166Pseudo<(outs), (ins i16imm:$cc, brtarget:$target)> {
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

let Defs = [PSW] in {
def ADDrr : C166ALURR<0x00, GR16, "add", add>;
let Uses = [PSW] in
def ADDCrr : C166ALURR<0x10, GR16, "addc", null_frag> {
  let hasSideEffects = 0;
}
def ADDri3 : C166ALUri3<0x08, "add", add>;
def ADDri16 : C166ALUri16<0x06, "add", add>;
let Uses = [PSW] in {
def ADDCri3 : C166Carryri3<0x18, "addc">;
def ADDCri16 : C166Carryri16<0x16, "addc">;
}
def ADDBrr : C166RR<0x01, GR8, "addb">;
def SUBrr : C166ALURR<0x20, GR16, "sub", sub>;
let Uses = [PSW] in
def SUBCrr : C166ALURR<0x30, GR16, "subc", null_frag> {
  let hasSideEffects = 0;
}
def SUBri3 : C166ALUri3<0x28, "sub", sub>;
def SUBri16 : C166ALUri16<0x26, "sub", sub>;
let Uses = [PSW] in {
def SUBCri3 : C166Carryri3<0x38, "subc">;
def SUBCri16 : C166Carryri16<0x36, "subc">;
}
def SUBBrr : C166RR<0x21, GR8, "subb">;
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
def CMPrr : C166RR<0x40, GR16, "cmp"> {
  let OutOperandList = (outs);
  let InOperandList = (ins GR16:$dst, GR16:$src);
}
def CMPri3 : C166Inst16<0x48, (outs), (ins GR16:$dst, uimm3:$src),
                        "cmp\t$dst, $src"> {
  bits<4> dst;
  bits<3> src;
  let Inst{15-12} = dst;
  let Inst{11} = 0;
  let Inst{10-8} = src;
}
def CMPBrr : C166RR<0x41, GR8, "cmpb"> {
  let OutOperandList = (outs);
  let InOperandList = (ins GR8:$dst, GR8:$src);
}
def XORrr : C166ALURR<0x50, GR16, "xor", xor>;
def XORri3 : C166ALUri3<0x58, "xor", xor>;
def XORri16 : C166ALUri16<0x56, "xor", xor>;
def XORBrr : C166RR<0x51, GR8, "xorb">;
def ANDrr : C166ALURR<0x60, GR16, "and", and>;
def ANDri3 : C166ALUri3<0x68, "and", and>;
def ANDri16 : C166ALUri16<0x66, "and", and>;
def ANDBrr : C166RR<0x61, GR8, "andb">;
def ORrr : C166ALURR<0x70, GR16, "or", or>;
def ORri3 : C166ALUri3<0x78, "or", or>;
def ORri16 : C166ALUri16<0x76, "or", or>;
def ORBrr : C166RR<0x71, GR8, "orb">;
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
def MOVrm : C166RM<0xa8, "mov">;
}

let Uses = [MDC], Defs = [MDH, MDL, MDC, PSW] in {
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
  let Defs = [MDH, MDL, MDC, PSW];
}

def DIVr : C166DIV<0x4b, "div">;
def DIVUr : C166DIV<0x5b, "divu">;

let Defs = [PSW] in {
def MOVBrm : C166Inst16<0xa9, (outs GR8:$dst),
                        (ins GR16:$base), "movb\t$dst, [$base]"> {
  bits<4> dst;
  bits<4> base;
  let Inst{15-12} = dst;
  let Inst{11-8} = base;
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

def EXTPr : C166Inst16<0xdc, (outs),
                       (ins GR16:$page, sequence_count:$count),
                       "extp\t$page, $count"> {
  bits<4> page;
  bits<2> count;
  let Inst{15-14} = 0b01;
  let Inst{13-12} = count;
  let Inst{11-8} = page;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeEXTP1r";
}

def EXTSr : C166Inst16<0xdc, (outs),
                       (ins GR16:$segment, sequence_count:$count),
                       "exts\t$segment, $count"> {
  bits<4> segment;
  bits<2> count;
  let Inst{15-14} = 0b00;
  let Inst{13-12} = count;
  let Inst{11-8} = segment;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeEXTS1r";
}

let Defs = [PSW] in
def MOVmrPreDec : C166Inst16<0x88, (outs GR16:$base_wb),
                             (ins GR16:$base, GR16:$src),
                             "mov\t[-$base], $src"> {
  bits<4> base;
  bits<4> src;
  let Inst{15-12} = src;
  let Inst{11-8} = base;
  let Constraints = "$base_wb = $base";
  let mayStore = 1;
  let Pattern = [];
}

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

def NOP : C166Inst16<0xcc, (outs), (ins), "nop"> {
  let Inst{15-8} = 0;
}

def ATOMIC : C166Inst16<0xd1, (outs), (ins atomic_count:$count),
                         "atomic\t$count"> {
  bits<2> count;
  let Inst{15-14} = 0;
  let Inst{13-12} = count;
  let Inst{11-8} = 0;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeATOMIC";
}

// Direct bit operations encode the eight-bit word address in the second byte
// and the bit number in the high nibble of the opcode byte.
let Defs = [PSW], hasSideEffects = 1 in {
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

let Defs = [SP, CSP, PSW], Uses = [SP] in
def RETI : C166Inst16<0xfb, (outs), (ins), "reti"> {
  let Inst{15-8} = 0x88;
  let isReturn = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let Pattern = [(C166interruptret)];
}

// Save the interrupted multiply/divide state and initialize MDC before the
// handler executes any MD-unit instruction or ordinary C call. The ABI
// The optimized interrupt frame uses exactly `SCXT MDC,#10h`.
let Defs = [SP], Uses = [SP] in
def SCXTri16 : C166Inst32<0xc6, (outs pushpopreg:$dst),
                          (ins pushpopreg:$old, uimm16:$src),
                          "scxt\t$dst, $src"> {
  bits<8> dst;
  bits<16> src;
  let Inst{15-8} = dst;
  let Inst{31-16} = src;
  let Constraints = "$dst = $old";
  let mayStore = 1;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeSCXTri16";
}

// C166 Family Instruction Set Manual, DISWDT format A5 5A A5 A5.
def DISWDT : C166Inst32<0xa5, (outs), (ins), "diswdt"> {
  let Inst{15-8} = 0x5a;
  let Inst{23-16} = 0xa5;
  let Inst{31-24} = 0xa5;
  let hasSideEffects = 1;
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

let Defs = [PSW] in
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
def MOVsfri16 : C166Inst32<0xe6, (outs sfrshort:$dst), (ins uimm16:$src),
                            "mov\t$dst, $src"> {
  bits<8> dst;
  bits<16> src;
  let Inst{15-8} = dst;
  let Inst{31-16} = src;
  let DecoderMethod = "decodeMOVsfri16";
  let hasSideEffects = 1;
}

def EXTPp : C166Inst32<0xd7, (outs),
                       (ins addressPag10:$page, sequence_count:$count),
                       "extp\t$page, $count"> {
  bits<10> page;
  bits<2> count;
  let Inst{15-14} = 0b01;
  let Inst{13-12} = count;
  let Inst{11-8} = 0;
  let Inst{25-16} = page;
  let Inst{31-26} = 0;
  let hasSideEffects = 1;
  let DecoderMethod = "decodeEXTP1p";
}

let Defs = [PSW] in {
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
  let Inst{29-16} = address;
  let Inst{31-30} = 0;
  let mayLoad = 1;
  let DecoderMethod = "decodeMOVBZgd";
}

def MOVBZabsgd : C166Inst32<0xc2, (outs directreg:$dst),
                            (ins addressAbs16:$address),
                            "movbz\t$dst, $address"> {
  bits<8> dst;
  bits<16> address;
  let Inst{15-8} = dst;
  let Inst{31-16} = address;
  let mayLoad = 1;
  let DecoderMethod = "decodeMOVBZgd";
  let isAsmParserOnly = 1;
}

def MOVBSgd : C166Inst32<0xd2, (outs directreg:$dst),
                         (ins addressPof14:$address),
                         "movbs\t$dst, $address"> {
  bits<8> dst;
  bits<14> address;
  let Inst{15-8} = dst;
  let Inst{29-16} = address;
  let Inst{31-30} = 0;
  let mayLoad = 1;
  let DecoderMethod = "decodeMOVBSgd";
}

def MOVBSabsgd : C166Inst32<0xd2, (outs directreg:$dst),
                            (ins addressAbs16:$address),
                            "movbs\t$dst, $address"> {
  bits<8> dst;
  bits<16> address;
  let Inst{15-8} = dst;
  let Inst{31-16} = address;
  let mayLoad = 1;
  let DecoderMethod = "decodeMOVBSgd";
  let isAsmParserOnly = 1;
}

def MOVBdg : C166Inst32<0xf7, (outs),
                        (ins addressPof14:$address, directbytereg:$src),
                        "movb\t$address, $src"> {
  bits<8> src;
  bits<14> address;
  let Inst{15-8} = src;
  let Inst{29-16} = address;
  let Inst{31-30} = 0;
  let mayStore = 1;
  let DecoderMethod = "decodeMOVBdg";
}

def MOVBabsdg : C166Inst32<0xf7, (outs),
                           (ins addressAbs16:$address,
                                directbytereg:$src),
                           "movb\t$address, $src"> {
  bits<8> src;
  bits<16> address;
  let Inst{15-8} = src;
  let Inst{31-16} = address;
  let mayStore = 1;
  let DecoderMethod = "decodeMOVBdg";
  let isAsmParserOnly = 1;
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
    let Inst{29-16} = address;
    let Inst{31-30} = Selector;
    let mayLoad = 1;
    let DecoderMethod = "decodeMOVBZgd";
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
    let Inst{29-16} = address;
    let Inst{31-30} = Selector;
    let mayStore = 1;
    let DecoderMethod = "decodeMOVBdg";
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
                         (ins GR16:$base, uimm16:$disp),
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
                         (ins GR16:$base, uimm16:$disp, GR16:$src),
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
                          (ins GR16:$base, uimm16:$disp),
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
                          (ins GR16:$base, uimm16:$disp, GR8:$src),
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
def JMPR_EQ  : C166JMPR<0x2d, "cc_eq">;
def JMPR_NE  : C166JMPR<0x3d, "cc_ne">;
def JMPR_ULT : C166JMPR<0x8d, "cc_ult">;
def JMPR_UGE : C166JMPR<0x9d, "cc_uge">;
def JMPR_SGT : C166JMPR<0xad, "cc_sgt">;
def JMPR_SLE : C166JMPR<0xbd, "cc_sle">;
def JMPR_SLT : C166JMPR<0xcd, "cc_slt">;
def JMPR_SGE : C166JMPR<0xdd, "cc_sge">;
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

def JMPA : C166Inst32<0xea, (outs), (ins codeAddress16:$offset),
                      "jmpa\tcc_uc, $offset"> {
  bits<16> offset;
  let Inst{15-8} = 0;
  let Inst{31-16} = offset;
  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
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

def JMPI : C166Inst16<0x9c, (outs), (ins GR16:$offset),
                      "jmpi\tcc_uc, [$offset]"> {
  bits<4> offset;
  let Inst{15-12} = 0;
  let Inst{11-8} = offset;
  let isBranch = 1;
  let isIndirectBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
}

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

def CALLA : C166Inst32<0xca, (outs),
                       (ins codeAddress16:$offset, variable_ops),
                       "calla\tcc_uc, $offset"> {
  bits<16> offset;
  let Inst{15-8} = 0;
  let Inst{31-16} = offset;
  let isCall = 1;
}

def CALLI : C166Inst16<0xab, (outs), (ins GR16:$offset, variable_ops),
                       "calli\tcc_uc, [$offset]"> {
  bits<4> offset;
  let Inst{15-12} = 0;
  let Inst{11-8} = offset;
  let isCall = 1;
}
