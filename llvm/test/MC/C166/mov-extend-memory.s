; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
movbs mdl, 0x4003
; CHECK: encoding: [0xd2,0x07,0x03,0x40]
; DIS: movbs mdl, 16387
movbz mdh, 0xffff
; CHECK: encoding: [0xc2,0x06,0xff,0xff]
; DIS-NEXT: movbz mdh, 65535
movbs 0x4004, rh7
; CHECK: encoding: [0xd5,0xff,0x04,0x40]
; DIS-NEXT: movbs 16388, rh7
movbz 0xfffe, rl0
; CHECK: encoding: [0xc5,0xf0,0xfe,0xff]
; DIS-NEXT: movbz 65534, rl0
movbs 0x4004, mdl
; CHECK: encoding: [0xd5,0x07,0x04,0x40]
; DIS-NEXT: movbs 16388, mdl
movbz 0xfffe, mdh
; CHECK: encoding: [0xc5,0x06,0xfe,0xff]
; DIS-NEXT: movbz 65534, mdh

movbs r0, rh7
; CHECK: encoding: [0xd0,0xf0]
; DIS-NEXT: movbs r0, rh7
movbz r15, rl0
; CHECK: encoding: [0xc0,0x0f]
; DIS-NEXT: movbz r15, rl0
movbs r7, rh7
; CHECK: encoding: [0xd0,0xf7]
; DIS-NEXT: movbs r7, rh7
movbz r0, rl0
; CHECK: encoding: [0xc0,0x00]
; DIS-NEXT: movbz r0, rl0
movbs r15, 65535
; CHECK: encoding: [0xd2,0xff,0xff,0xff]
; DIS-NEXT: movbs r15, 65535
movbz r0, 0
; CHECK: encoding: [0xc2,0xf0,0x00,0x00]
; DIS-NEXT: movbz r0, 0
