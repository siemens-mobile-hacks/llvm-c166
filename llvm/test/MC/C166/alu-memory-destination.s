; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS

add 0, r0
; CHECK: encoding: [0x04,0xf0,0x00,0x00]
; DIS: add 0, r0
addc 0x3fff, r15
; CHECK: encoding: [0x14,0xff,0xff,0x3f]
; DIS: addc 16383, r15
sub 0x4000, r4
; CHECK: encoding: [0x24,0xf4,0x00,0x40]
; DIS: sub 16384, r4
subc 0x8000, r4
; CHECK: encoding: [0x34,0xf4,0x00,0x80]
; DIS: subc 32768, r4
xor 0xc000, r4
; CHECK: encoding: [0x54,0xf4,0x00,0xc0]
; DIS: xor 49152, r4
and 0xffff, r4
; CHECK: encoding: [0x64,0xf4,0xff,0xff]
; DIS: and 65535, r4
or 0x100, r4
; CHECK: encoding: [0x74,0xf4,0x00,0x01]
; DIS: or 256, r4
add operand+2, r4
; CHECK: fixup A - offset: 2, value: operand+2, kind: fixup_c166_address16
; DIS: add 0, r4
; DIS-NEXT: R_C166_16 operand+0x2
