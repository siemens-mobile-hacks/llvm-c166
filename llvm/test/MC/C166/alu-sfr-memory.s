; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
.irp op, add, addc, sub, subc, cmp, xor, and, or
  \op mdl, 0x4000
.endr
; CHECK: encoding: [0x02,0x07,0x00,0x40]
; CHECK: encoding: [0x12,0x07,0x00,0x40]
; CHECK: encoding: [0x22,0x07,0x00,0x40]
; CHECK: encoding: [0x32,0x07,0x00,0x40]
; CHECK: encoding: [0x42,0x07,0x00,0x40]
; CHECK: encoding: [0x52,0x07,0x00,0x40]
; CHECK: encoding: [0x62,0x07,0x00,0x40]
; CHECK: encoding: [0x72,0x07,0x00,0x40]
; DIS: add mdl, 16384
; DIS: addc mdl, 16384
; DIS: sub mdl, 16384
; DIS: subc mdl, 16384
; DIS: cmp mdl, 16384
; DIS: xor mdl, 16384
; DIS: and mdl, 16384
; DIS: or mdl, 16384
.irp op, add, addc, sub, subc, xor, and, or
  \op 0xffff, mdh
.endr
; CHECK: encoding: [0x04,0x06,0xff,0xff]
; CHECK: encoding: [0x14,0x06,0xff,0xff]
; CHECK: encoding: [0x24,0x06,0xff,0xff]
; CHECK: encoding: [0x34,0x06,0xff,0xff]
; CHECK: encoding: [0x54,0x06,0xff,0xff]
; CHECK: encoding: [0x64,0x06,0xff,0xff]
; CHECK: encoding: [0x74,0x06,0xff,0xff]
; DIS: add 65535, mdh
; DIS: addc 65535, mdh
; DIS: sub 65535, mdh
; DIS: subc 65535, mdh
; DIS: xor 65535, mdh
; DIS: and 65535, mdh
; DIS: or 65535, mdh
