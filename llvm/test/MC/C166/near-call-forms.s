; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS

callr 0
; CHECK: encoding: [0xbb,0x00]
; DIS: callr 0
callr 128
; CHECK: encoding: [0xbb,0x80]
; DIS: callr 128
pcall r0, 0x1234
; CHECK: encoding: [0xe2,0xf0,0x34,0x12]
; DIS: pcall r0, 4660
pcall mdc, 0
; CHECK: encoding: [0xe2,0x87,0x00,0x00]
; DIS: pcall mdc, 0
retp r15
; CHECK: encoding: [0xeb,0xff]
; DIS: retp r15
retp mdc
; CHECK: encoding: [0xeb,0x87]
; DIS: retp mdc
callr external
; CHECK: fixup A - offset: 1, value: external, kind: fixup_c166_pc8
; DIS: R_C166_PC8 external
pcall r4, cof(external)
; CHECK: fixup A - offset: 2, value: external, kind: fixup_c166_cof16
; DIS: R_C166_COF16 external
