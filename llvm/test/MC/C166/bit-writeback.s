; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS
jbc r0.0, 0
; CHECK: encoding: [0xaa,0xf0,0x00,0x00]
; DIS: jbc r0.0, 0
jnbs psw.15, 255
; CHECK: encoding: [0xba,0x88,0xff,0xf0]
; DIS-NEXT: jnbs psw.15, 255
jbc r15.15, 128
; CHECK: encoding: [0xaa,0xff,0x80,0xf0]
; DIS-NEXT: jbc r15.15, 128
jnbs 0 . 7, 127
; CHECK: encoding: [0xba,0x00,0x7f,0x70]
; DIS-NEXT: jnbs 0 . 7, 127
jbc r4.5, external
; DIS: jbc r4.5, 0
; DIS-NEXT: R_C166_PC8_RELAX external
; DIS-NEXT: nop
; DIS-NEXT: nop
; DIS-NEXT: nop
