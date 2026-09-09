; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS
mov mdc, 0x4000
; CHECK: encoding: [0xf2,0x87,0x00,0x40]
; DIS: mov mdc, 16384
mov 0x4000, mdc
; CHECK: encoding: [0xf6,0x87,0x00,0x40]
; DIS: mov 16384, mdc
mov dpp0, 0
; CHECK: encoding: [0xf2,0x00,0x00,0x00]
; DIS: mov dpp0, 0
mov 0xffff, psw
; CHECK: encoding: [0xf6,0x88,0xff,0xff]
; DIS: mov 65535, psw
mov sp, 0x8000
; CHECK: encoding: [0xf2,0x09,0x00,0x80]
; DIS: mov sp, 32768
mov 0xc000, cp
; CHECK: encoding: [0xf6,0x08,0x00,0xc0]
; DIS: mov 49152, cp
mov mdc, #256
; CHECK: encoding: [0xe6,0x87,0x00,0x01]
mov mdc, 256
; CHECK: encoding: [0xf2,0x87,0x00,0x01]
mov mdc, input+2
; CHECK: fixup A - offset: 2, value: input+2, kind: fixup_c166_address16
; DIS: mov mdc, 0
; DIS-NEXT: R_C166_16 input+0x2
mov output+4, mdc
; CHECK: fixup A - offset: 2, value: output+4, kind: fixup_c166_address16
; DIS: mov 0, mdc
; DIS-NEXT: R_C166_16 output+0x4
