; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS

; Constants contain short word addresses, not full byte addresses.
.equ PORT, 0x88
.set BIT, 3
.equ REGISTER, 0xf4
bset PORT.3
; CHECK: encoding: [0x3f,0x88]
; DIS: bset psw.3
bset PORT.BIT
; CHECK: encoding: [0x3f,0x88]
bclr PORT . (BIT + 1)
; CHECK: encoding: [0x4e,0x88]
bset REGISTER.BIT
; CHECK: encoding: [0x3f,0xf4]
; DIS: bset r4.3
bmov psw.11, PORT.BIT
; CHECK: encoding: [0x4a,0x88,0x88,0x3b]

; A dotted symbol outside a bit operand must not be converted to a bit address.
mov r4, PORT.BIT
; CHECK: mov r4, PORT.BIT
; CHECK: fixup A - offset: 2, value: PORT.BIT, kind: fixup_c166_address16
; DIS: R_C166_16 PORT.BIT
jb PORT.BIT, PORT.BIT
; CHECK: jb psw.3, PORT.BIT
; CHECK: value: PORT.BIT

; An explicitly defined dotted constant takes precedence over splitting it.
.equ TARGET, 0x88
.equ INDEX, 3
.equ TARGET.INDEX, 0xf42
bset TARGET.INDEX
; CHECK: encoding: [0x2f,0xf4]
; DIS: bset r4.2
bset psw.010
; CHECK: encoding: [0xaf,0x88]

; Constant expressions must preserve both boundary fields independently.
.equ FIRST_WORD, 0
.equ LAST_WORD, 255
.equ FIRST_BIT, 0
.equ LAST_BIT, 15
bset FIRST_WORD . FIRST_BIT
; CHECK: encoding: [0x0f,0x00]
bclr LAST_WORD . LAST_BIT
; CHECK: encoding: [0xfe,0xff]
bmov FIRST_WORD.FIRST_BIT, LAST_WORD.LAST_BIT
; CHECK: encoding: [0x4a,0xff,0x00,0xf0]
bmov LAST_WORD.LAST_BIT, FIRST_WORD.FIRST_BIT
; CHECK: encoding: [0x4a,0x00,0xff,0x0f]
bfldl FIRST_WORD, #0, #255
; CHECK: encoding: [0x0a,0x00,0x00,0xff]
bfldh LAST_WORD, #255, #0
; CHECK: encoding: [0x1a,0xff,0x00,0xff]

; A constant containing a period is still an ordinary symbol in either field.
.equ ALIAS.WORD, 0xff
.equ ALIAS.BIT, 15
bset ALIAS.WORD . ALIAS.BIT
; CHECK: encoding: [0xff,0xff]
