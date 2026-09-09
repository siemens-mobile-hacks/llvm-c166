; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
.irp op, add, addc, sub, subc, cmp, xor, and, or
  \op mdl, mdh
.endr
; CHECK: encoding: [0x02,0x07,0x0c,0xfe]
; CHECK: encoding: [0x12,0x07,0x0c,0xfe]
; CHECK: encoding: [0x22,0x07,0x0c,0xfe]
; CHECK: encoding: [0x32,0x07,0x0c,0xfe]
; CHECK: encoding: [0x42,0x07,0x0c,0xfe]
; CHECK: encoding: [0x52,0x07,0x0c,0xfe]
; CHECK: encoding: [0x62,0x07,0x0c,0xfe]
; CHECK: encoding: [0x72,0x07,0x0c,0xfe]
; The canonical memory form prints the full direct address of MDH.
; DIS: add mdl, 65036
; DIS: addc mdl, 65036
; DIS: sub mdl, 65036
; DIS: subc mdl, 65036
; DIS: cmp mdl, 65036
; DIS: xor mdl, 65036
; DIS: and mdl, 65036
; DIS: or mdl, 65036
