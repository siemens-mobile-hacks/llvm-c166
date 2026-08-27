//===-- C166CallingConv.td - C166 calling convention -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// The ABI observes the low byte of R4 (RL4) for a char return. Use R4
// as its LLVM carrier: the ABI emits a word MOV for a char identity
// function, and the upper byte is unspecified at the ABI boundary.
def RetCC_C166 : CallingConv<[
  CCIfType<[i8],  CCPromoteToType<i16>>,
  CCIfType<[i16], CCAssignToReg<[R4]>>,
  CCIfType<[i32], CCAssignToReg<[R5R4]>>
]>;
