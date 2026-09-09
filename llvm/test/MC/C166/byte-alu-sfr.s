; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

.irp op, addb, addcb, subb, subcb, cmpb, xorb, andb, orb
  \op mdl, #0
  \op mdl, 0x4003
  \op mdl, mdh
.endr
; CHECK: encoding: [0x07,0x07,0x00,0x00]
; CHECK: encoding: [0x03,0x07,0x03,0x40]
; CHECK: encoding: [0x03,0x07,0x0c,0xfe]
; CHECK: encoding: [0x17,0x07,0x00,0x00]
; CHECK: encoding: [0x13,0x07,0x03,0x40]
; CHECK: encoding: [0x13,0x07,0x0c,0xfe]
; CHECK: encoding: [0x27,0x07,0x00,0x00]
; CHECK: encoding: [0x23,0x07,0x03,0x40]
; CHECK: encoding: [0x23,0x07,0x0c,0xfe]
; CHECK: encoding: [0x37,0x07,0x00,0x00]
; CHECK: encoding: [0x33,0x07,0x03,0x40]
; CHECK: encoding: [0x33,0x07,0x0c,0xfe]
; CHECK: encoding: [0x47,0x07,0x00,0x00]
; CHECK: encoding: [0x43,0x07,0x03,0x40]
; CHECK: encoding: [0x43,0x07,0x0c,0xfe]
; CHECK: encoding: [0x57,0x07,0x00,0x00]
; CHECK: encoding: [0x53,0x07,0x03,0x40]
; CHECK: encoding: [0x53,0x07,0x0c,0xfe]
; CHECK: encoding: [0x67,0x07,0x00,0x00]
; CHECK: encoding: [0x63,0x07,0x03,0x40]
; CHECK: encoding: [0x63,0x07,0x0c,0xfe]
; CHECK: encoding: [0x77,0x07,0x00,0x00]
; CHECK: encoding: [0x73,0x07,0x03,0x40]
; CHECK: encoding: [0x73,0x07,0x0c,0xfe]
; DIS: addb mdl, #0
; DIS-NEXT: addb mdl, 16387
; DIS-NEXT: addb mdl, 65036
; DIS-NEXT: addcb mdl, #0
; DIS-NEXT: addcb mdl, 16387
; DIS-NEXT: addcb mdl, 65036
; DIS-NEXT: subb mdl, #0
; DIS-NEXT: subb mdl, 16387
; DIS-NEXT: subb mdl, 65036
; DIS-NEXT: subcb mdl, #0
; DIS-NEXT: subcb mdl, 16387
; DIS-NEXT: subcb mdl, 65036
; DIS-NEXT: cmpb mdl, #0
; DIS-NEXT: cmpb mdl, 16387
; DIS-NEXT: cmpb mdl, 65036
; DIS-NEXT: xorb mdl, #0
; DIS-NEXT: xorb mdl, 16387
; DIS-NEXT: xorb mdl, 65036
; DIS-NEXT: andb mdl, #0
; DIS-NEXT: andb mdl, 16387
; DIS-NEXT: andb mdl, 65036
; DIS-NEXT: orb mdl, #0
; DIS-NEXT: orb mdl, 16387
; DIS-NEXT: orb mdl, 65036

.irp op, addb, addcb, subb, subcb, xorb, andb, orb
  \op 0xffff, mdl
.endr
; CHECK: encoding: [0x05,0x07,0xff,0xff]
; CHECK: encoding: [0x15,0x07,0xff,0xff]
; CHECK: encoding: [0x25,0x07,0xff,0xff]
; CHECK: encoding: [0x35,0x07,0xff,0xff]
; CHECK: encoding: [0x55,0x07,0xff,0xff]
; CHECK: encoding: [0x65,0x07,0xff,0xff]
; CHECK: encoding: [0x75,0x07,0xff,0xff]
; DIS-NEXT: addb 65535, mdl
; DIS-NEXT: addcb 65535, mdl
; DIS-NEXT: subb 65535, mdl
; DIS-NEXT: subcb 65535, mdl
; DIS-NEXT: xorb 65535, mdl
; DIS-NEXT: andb 65535, mdl
; DIS-NEXT: orb 65535, mdl

addb mdh, #-128
; CHECK: encoding: [0x07,0x06,0x80,0x00]
; DIS-NEXT: addb mdh, #128
cmpb mdl, #255
; CHECK: encoding: [0x47,0x07,0xff,0x00]
; DIS-NEXT: cmpb mdl, #255
; The unused fourth byte must not become part of the immediate.
.byte 0x07, 0x07, 0x01, 0xaa
.byte 0x47, 0x07, 0xff, 0x55
; DIS-NEXT: addb mdl, #1
; DIS-NEXT: cmpb mdl, #255
