//===-- C166RegisterInfo.td - C166 register definitions ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

class C166Reg<string Name, bits<16> Encoding, int DwarfNumber>
    : Register<Name> {
  let Namespace = "C166";
  let HWEncoding = Encoding;
  let DwarfNumbers = [DwarfNumber];
}

class C166RegWithSubRegs<string Name, bits<16> Encoding,
                         list<Register> SubRegs, int DwarfNumber>
    : RegisterWithSubRegs<Name, SubRegs> {
  let Namespace = "C166";
  let HWEncoding = Encoding;
  let DwarfNumbers = [DwarfNumber];
}

def sub_lo8 : SubRegIndex<8, 0>;
def sub_hi8 : SubRegIndex<8, 8>;
def sub_lo16 : SubRegIndex<16, 0>;
def sub_hi16 : SubRegIndex<16, 16>;

// The ELF-DWARF ABI assigns numbers only to the 16-bit R0-R15 registers.
// Leave byte lanes unnumbered so LLVM describes them as pieces of their
// numbered word super-register instead of emitting non-ABI register numbers.
def RL0 : C166Reg<"rl0", 0, -1>;
def RH0 : C166Reg<"rh0", 1, -1>;
def RL1 : C166Reg<"rl1", 2, -1>;
def RH1 : C166Reg<"rh1", 3, -1>;
def RL2 : C166Reg<"rl2", 4, -1>;
def RH2 : C166Reg<"rh2", 5, -1>;
def RL3 : C166Reg<"rl3", 6, -1>;
def RH3 : C166Reg<"rh3", 7, -1>;
def RL4 : C166Reg<"rl4", 8, -1>;
def RH4 : C166Reg<"rh4", 9, -1>;
def RL5 : C166Reg<"rl5", 10, -1>;
def RH5 : C166Reg<"rh5", 11, -1>;
def RL6 : C166Reg<"rl6", 12, -1>;
def RH6 : C166Reg<"rh6", 13, -1>;
def RL7 : C166Reg<"rl7", 14, -1>;
def RH7 : C166Reg<"rh7", 15, -1>;

let SubRegIndices = [sub_lo8, sub_hi8] in {
def R0 : C166RegWithSubRegs<"r0", 0, [RL0, RH0], 0>;
def R1 : C166RegWithSubRegs<"r1", 1, [RL1, RH1], 1>;
def R2 : C166RegWithSubRegs<"r2", 2, [RL2, RH2], 2>;
def R3 : C166RegWithSubRegs<"r3", 3, [RL3, RH3], 3>;
def R4 : C166RegWithSubRegs<"r4", 4, [RL4, RH4], 4>;
def R5 : C166RegWithSubRegs<"r5", 5, [RL5, RH5], 5>;
def R6 : C166RegWithSubRegs<"r6", 6, [RL6, RH6], 6>;
def R7 : C166RegWithSubRegs<"r7", 7, [RL7, RH7], 7>;
}

def R8  : C166Reg<"r8", 8, 8>;
def R9  : C166Reg<"r9", 9, 9>;
def R10 : C166Reg<"r10", 10, 10>;
def R11 : C166Reg<"r11", 11, 11>;
def R12 : C166Reg<"r12", 12, 12>;
def R13 : C166Reg<"r13", 13, 13>;
def R14 : C166Reg<"r14", 14, 14>;
def R15 : C166Reg<"r15", 15, 15>;

// Direct SFR word addresses from the architectural C166 SFR map.  The DWARF
// numbers follow the C166 ELF-DWARF ABI rather than the SFR
// encodings; in particular, the system-stack pointer is register 289.
def DPP0 : C166Reg<"dpp0", 0x00, 305>;
def DPP1 : C166Reg<"dpp1", 0x01, 306>;
def DPP2 : C166Reg<"dpp2", 0x02, 307>;
def DPP3 : C166Reg<"dpp3", 0x03, 308>;
def CSP  : C166Reg<"csp",  0x04, 303>;
def MDH  : C166Reg<"mdh",  0x06, -1>;
def MDL  : C166Reg<"mdl",  0x07, -1>;
def CP   : C166Reg<"cp",   0x08, -1>;
def SP   : C166Reg<"sp",   0x09, 289>;
def STKOV : C166Reg<"stkov", 0x0a, -1>;
def STKUN : C166Reg<"stkun", 0x0b, -1>;
def MDC  : C166Reg<"mdc",  0x87, -1>;
def PSW  : C166Reg<"psw",  0x88, -1>;

// Allocation-only view of PSW.C.  It deliberately does not alias PSW: many
// instructions update the other condition flags while preserving carry.
def C : C166Reg<"c", 0, -1> {
  let isArtificial = 1;
}

// DWARF call-frame information describes the 32-bit linear CSP:IP return
// address through virtual register 301.  It is not an allocatable CPU
// register and exists only so MC can put the ABI register number in the CIE.
def RA : C166Reg<"ra", 0, 301> {
  let isArtificial = 1;
}

def GR8 : RegisterClass<"C166", [i8], 8,
                        (add RL0, RH0, RL1, RH1, RL2, RH2, RL3, RH3,
                             RL4, RH4, RL5, RH5, RL6, RH6, RL7, RH7)>;

def GR16 : RegisterClass<"C166", [i16], 16,
                         (add R0, R1, R2, R3, R4, R5, R6, R7,
                              R8, R9, R10, R11, R12, R13, R14, R15)>;

// The compact indirect ALU encodings have a two-bit address-register field.
def GR16Indirect : RegisterClass<"C166", [i16], 16,
                                 (add R0, R1, R2, R3)>;

// Word registers whose low byte is directly addressable as RL0-RL7.
def GR16Byte : RegisterClass<"C166", [i16], 16,
                             (add R0, R1, R2, R3, R4, R5, R6, R7)>;

def SFR16 : RegisterClass<"C166", [i16], 16,
                          (add DPP0, DPP1, DPP2, DPP3, CSP, MDH, MDL, CP, SP,
                               STKOV, STKUN, MDC, PSW)>;

// Keep a carry chain in the hardware flag instead of materializing a
// general-register boolean between adjacent operations.
def CARRY : RegisterClass<"C166", [i16], 16, (add C)> {
  let CopyCost = -1;
}

def Direct16 : RegisterClass<"C166", [i16], 16, (add GR16, SFR16)>;

let SubRegIndices = [sub_lo16, sub_hi16], CoveredBySubRegs = 1 in {
// Likewise, a 32-bit value in a register pair is represented by two numbered
// Rn pieces.  The pair itself is an LLVM allocation unit, not an ABI register.
def R1R0   : C166RegWithSubRegs<"r1r0",   0, [R0, R1], -1>;
def R2R1   : C166RegWithSubRegs<"r2r1",   1, [R1, R2], -1>;
def R3R2   : C166RegWithSubRegs<"r3r2",   2, [R2, R3], -1>;
def R4R3   : C166RegWithSubRegs<"r4r3",   3, [R3, R4], -1>;
def R5R4   : C166RegWithSubRegs<"r5r4",   4, [R4, R5], -1>;
def R6R5   : C166RegWithSubRegs<"r6r5",   5, [R5, R6], -1>;
def R7R6   : C166RegWithSubRegs<"r7r6",   6, [R6, R7], -1>;
def R8R7   : C166RegWithSubRegs<"r8r7",   7, [R7, R8], -1>;
def R9R8   : C166RegWithSubRegs<"r9r8",   8, [R8, R9], -1>;
def R10R9  : C166RegWithSubRegs<"r10r9",  9, [R9, R10], -1>;
def R11R10 : C166RegWithSubRegs<"r11r10", 10, [R10, R11], -1>;
def R12R11 : C166RegWithSubRegs<"r12r11", 11, [R11, R12], -1>;
def R13R12 : C166RegWithSubRegs<"r13r12", 12, [R12, R13], -1>;
def R14R13 : C166RegWithSubRegs<"r14r13", 13, [R13, R14], -1>;
def R15R14 : C166RegWithSubRegs<"r15r14", 14, [R14, R15], -1>;
}

def GR32 : RegisterClass<"C166", [i32], 16,
                         (add R1R0, R2R1, R3R2, R4R3, R5R4,
                              R6R5, R7R6, R8R7, R9R8, R10R9,
                              R11R10, R12R11, R13R12, R14R13, R15R14)>;

// C166 ABI register variables. All other general registers are
// caller-saved; R0 is the user stack pointer and is reserved separately.
def CSR_C166 : CalleeSavedRegs<(add R6, R7, R8, R9)>;
def CSR_C166_CallPreserved : CalleeSavedRegs<(add R0, R6, R7, R8, R9)>;
def CSR_C166_Interrupt
    : CalleeSavedRegs<(add R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11,
                          R12, R13, R14, R15, DPP0, DPP2, MDC, MDH, MDL)>;
// A named global register bank preserves all GPRs by changing CP.  Only the
// non-banked state used by the handler must still be saved explicitly.
def CSR_C166_InterruptBank
    : CalleeSavedRegs<(add MDC, DPP0, DPP2, MDH, MDL)>;
def CSR_C166_InterruptCallPreserved
    : CalleeSavedRegs<(add R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10,
                          R11, R12, R13, R14, R15, DPP0, DPP1, DPP2, DPP3,
                          MDC, MDH, MDL)>;
