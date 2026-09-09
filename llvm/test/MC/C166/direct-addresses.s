; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

; Direct addresses retain both DPP selector bits, unlike pof expressions.
mov r4, 0x3fff
; CHECK: encoding: [0xf2,0xf4,0xff,0x3f]
mov r4, 0x4000
; CHECK: encoding: [0xf2,0xf4,0x00,0x40]
; DIS: mov r4, dpp1(0)
mov r4, 0x8000
; CHECK: encoding: [0xf2,0xf4,0x00,0x80]
mov r4, 0xc000
; CHECK: encoding: [0xf2,0xf4,0x00,0xc0]
; DIS: mov r4, 49152
mov r4, 0xffff
; CHECK: encoding: [0xf2,0xf4,0xff,0xff]
; DIS: mov r4, 65535
mov 0x4000, r4
; CHECK: encoding: [0xf6,0xf4,0x00,0x40]
; DIS: mov dpp1(0), r4
movbz r4, 0x8000
; CHECK: encoding: [0xc2,0xf4,0x00,0x80]
; DIS: movbz r4, dpp2(0)
movbs r4, 0xc000
; CHECK: encoding: [0xd2,0xf4,0x00,0xc0]
; DIS: movbs r4, 49152
movb 0xffff, rl2
; CHECK: encoding: [0xf7,0xf4,0xff,0xff]
; DIS: movb 65535, rl2

.equ address, 0x4000
mov r4, address+2
; CHECK: encoding: [0xf2,0xf4,0x02,0x40]
mov r4, pof(0x414000)
; CHECK: encoding: [0xf2,0xf4,A,0b00AAAAAA]
; CHECK: fixup A - offset: 2, value: 4276224, kind: fixup_c166_pof14
