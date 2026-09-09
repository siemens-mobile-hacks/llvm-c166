; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS

addcb rl0, rh7
; CHECK: encoding: [0x11,0x0f]
; DIS: addcb rl0, rh7
subcb rh7, rl0
; CHECK: encoding: [0x31,0xf0]
; DIS: subcb rh7, rl0
addcb rl2, #0
; CHECK: encoding: [0x19,0x40]
; DIS: addcb rl2, #0
subcb rh2, #7
; CHECK: encoding: [0x39,0x57]
; DIS: subcb rh2, #7
addcb rl2, #8
; CHECK: encoding: [0x17,0xf4,0x08,0x00]
; DIS: addcb rl2, #8
subcb rh2, #-128
; CHECK: encoding: [0x37,0xf5,0x80,0x00]
; DIS: subcb rh2, #128
addcb rh7, #255
; CHECK: encoding: [0x17,0xff,0xff,0x00]
; DIS: addcb rh7, #255
subcb rl0, #-1
; CHECK: encoding: [0x37,0xf0,0xff,0x00]
; DIS: subcb rl0, #255
addcb rl2, [r0]
; CHECK: encoding: [0x19,0x48]
; DIS: addcb rl2, [r0]
subcb rh2, [r3]
; CHECK: encoding: [0x39,0x5b]
; DIS: subcb rh2, [r3]
addcb rl2, [r3+]
; CHECK: encoding: [0x19,0x4f]
; DIS: addcb rl2, [r3+]
subcb rh2, [r0+]
; CHECK: encoding: [0x39,0x5c]
; DIS: subcb rh2, [r0+]
addcb rl2, #value+1
; CHECK: fixup A - offset: 2, value: value+1, kind: FK_Data_1
; DIS: addcb rl2, #0
; DIS-NEXT: R_C166_8 value+0x1
