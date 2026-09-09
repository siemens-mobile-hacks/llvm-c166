; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
movb rl0, 0x4003
; CHECK: encoding: [0xf3,0xf0,0x03,0x40]
; DIS: movb rl0, 16387
movb rh7, 0xffff
; CHECK: encoding: [0xf3,0xff,0xff,0xff]
; DIS-NEXT: movb rh7, 65535
movb mdl, #0
; CHECK: encoding: [0xe7,0x07,0x00,0x00]
; DIS-NEXT: movb mdl, #0
movb mdh, #-128
; CHECK: encoding: [0xe7,0x06,0x80,0x00]
; DIS-NEXT: movb mdh, #128
movb mdl, #255
; CHECK: encoding: [0xe7,0x07,0xff,0x00]
; DIS-NEXT: movb mdl, #255
movb mdl, 0x4003
; CHECK: encoding: [0xf3,0x07,0x03,0x40]
; DIS-NEXT: movb mdl, 16387
movb mdl, mdh
; CHECK: encoding: [0xf3,0x07,0x0c,0xfe]
; DIS-NEXT: movb mdl, 65036
movb 0xffff, mdh
; CHECK: encoding: [0xf7,0x06,0xff,0xff]
; DIS-NEXT: movb 65535, mdh
.byte 0xe7, 0x07, 0x01, 0xaa
; DIS-NEXT: movb mdl, #1
