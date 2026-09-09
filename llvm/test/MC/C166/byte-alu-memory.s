; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS

addb rl0, 0
; CHECK: encoding: [0x03,0xf0,0x00,0x00]
; DIS: addb rl0, 0
addcb rh7, 0x3fff
; CHECK: encoding: [0x13,0xff,0xff,0x3f]
; DIS: addcb rh7, 16383
subb rl2, 0x4000
; CHECK: encoding: [0x23,0xf4,0x00,0x40]
; DIS: subb rl2, 16384
subcb rh2, 0x8001
; CHECK: encoding: [0x33,0xf5,0x01,0x80]
; DIS: subcb rh2, 32769
cmpb rl2, 0xc000
; CHECK: encoding: [0x43,0xf4,0x00,0xc0]
; DIS: cmpb rl2, 49152
xorb rl2, 0xffff
; CHECK: encoding: [0x53,0xf4,0xff,0xff]
; DIS: xorb rl2, 65535
andb rl2, 3
; CHECK: encoding: [0x63,0xf4,0x03,0x00]
; DIS: andb rl2, 3
orb rl2, 256
; CHECK: encoding: [0x73,0xf4,0x00,0x01]
; DIS: orb rl2, 256
addb 0, rl0
; CHECK: encoding: [0x05,0xf0,0x00,0x00]
; DIS: addb 0, rl0
addcb 0x3fff, rh7
; CHECK: encoding: [0x15,0xff,0xff,0x3f]
; DIS: addcb 16383, rh7
subb 0x4000, rl2
; CHECK: encoding: [0x25,0xf4,0x00,0x40]
; DIS: subb 16384, rl2
subcb 0x8001, rh2
; CHECK: encoding: [0x35,0xf5,0x01,0x80]
; DIS: subcb 32769, rh2
xorb 0xc000, rl2
; CHECK: encoding: [0x55,0xf4,0x00,0xc0]
; DIS: xorb 49152, rl2
andb 0xffff, rl2
; CHECK: encoding: [0x65,0xf4,0xff,0xff]
; DIS: andb 65535, rl2
orb 256, rl2
; CHECK: encoding: [0x75,0xf4,0x00,0x01]
; DIS: orb 256, rl2
