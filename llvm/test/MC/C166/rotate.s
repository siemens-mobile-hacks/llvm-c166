; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
rol r0, r15
; CHECK: encoding: [0x0c,0x0f]
; DIS: rol r0, r15
ror r15, r0
; CHECK: encoding: [0x2c,0xf0]
; DIS: ror r15, r0
rol r2, #0
; CHECK: encoding: [0x1c,0x02]
; DIS: rol r2, #0
rol r2, #8
; CHECK: encoding: [0x1c,0x82]
; DIS: rol r2, #8
ror r4, #8
; CHECK: encoding: [0x3c,0x84]
; DIS: ror r4, #8
ror r15, #15
; CHECK: encoding: [0x3c,0xff]
; DIS: ror r15, #15
