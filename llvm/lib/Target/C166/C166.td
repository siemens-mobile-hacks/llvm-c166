//===-- C166.td - Describe the C166 target machine --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

class C166Processor<string Name> : Processor<Name, NoItineraries, []>;

def : C166Processor<"generic">;
def : C166Processor<"c166">;

include "C166RegisterInfo.td"
include "C166CallingConv.td"
include "C166InstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<GR32>;

def C166InstrInfo : InstrInfo;

def C166AsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
}

def C166AsmParser : AsmParser;

def C166 : Target {
  let InstructionSet = C166InstrInfo;
  let AssemblyParsers = [C166AsmParser];
}
