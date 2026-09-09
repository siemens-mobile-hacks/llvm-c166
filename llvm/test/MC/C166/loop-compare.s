; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
.irp op, cmpd1, cmpd2, cmpi1, cmpi2
  \op r4, #9
  \op r15, #15
  \op r0, #16
  \op r4, 0xffff
.endr
; CHECK: encoding: [0xa0,0x94]
; CHECK: encoding: [0xa0,0xff]
; CHECK: encoding: [0xa6,0xf0,0x10,0x00]
; CHECK: encoding: [0xa2,0xf4,0xff,0xff]
; CHECK: encoding: [0xb0,0x94]
; CHECK: encoding: [0xb0,0xff]
; CHECK: encoding: [0xb6,0xf0,0x10,0x00]
; CHECK: encoding: [0xb2,0xf4,0xff,0xff]
; CHECK: encoding: [0x80,0x94]
; CHECK: encoding: [0x80,0xff]
; CHECK: encoding: [0x86,0xf0,0x10,0x00]
; CHECK: encoding: [0x82,0xf4,0xff,0xff]
; CHECK: encoding: [0x90,0x94]
; CHECK: encoding: [0x90,0xff]
; CHECK: encoding: [0x96,0xf0,0x10,0x00]
; CHECK: encoding: [0x92,0xf4,0xff,0xff]
; DIS: cmpd1 r4, #9
; DIS-NEXT: cmpd1 r15, #15
; DIS-NEXT: cmpd1 r0, #16
; DIS-NEXT: cmpd1 r4, 65535
; DIS-NEXT: cmpd2 r4, #9
; DIS-NEXT: cmpd2 r15, #15
; DIS-NEXT: cmpd2 r0, #16
; DIS-NEXT: cmpd2 r4, 65535
; DIS-NEXT: cmpi1 r4, #9
; DIS-NEXT: cmpi1 r15, #15
; DIS-NEXT: cmpi1 r0, #16
; DIS-NEXT: cmpi1 r4, 65535
; DIS-NEXT: cmpi2 r4, #9
; DIS-NEXT: cmpi2 r15, #15
; DIS-NEXT: cmpi2 r0, #16
; DIS-NEXT: cmpi2 r4, 65535

cmpd1 r14, #0
; CHECK: cmpd1 r14, #0{{.*}}encoding: [0xa0,0x0e]
; DIS: a0 0e{{.*}}cmpd1 r14, #0
cmpd1 r1, #0x1234
; CHECK: cmpd1 r1, #4660{{.*}}encoding: [0xa6,0xf1,0x34,0x12]
; DIS: a6 f1 34 12{{.*}}cmpd1 r1, #4660
cmpd2 r2, #3
; CHECK: cmpd2 r2, #3{{.*}}encoding: [0xb0,0x32]
; DIS: b0 32{{.*}}cmpd2 r2, #3
cmpd2 r3, #0x5678
; CHECK: cmpd2 r3, #22136{{.*}}encoding: [0xb6,0xf3,0x78,0x56]
; DIS: b6 f3 78 56{{.*}}cmpd2 r3, #22136
cmpi1 r4, #5
; CHECK: cmpi1 r4, #5{{.*}}encoding: [0x80,0x54]
; DIS: 80 54{{.*}}cmpi1 r4, #5
cmpi1 r5, #0xabcd
; CHECK: cmpi1 r5, #43981{{.*}}encoding: [0x86,0xf5,0xcd,0xab]
; DIS: 86 f5 cd ab{{.*}}cmpi1 r5, #43981
cmpi2 r6, #7
; CHECK: cmpi2 r6, #7{{.*}}encoding: [0x90,0x76]
; DIS: 90 76{{.*}}cmpi2 r6, #7
cmpi2 r7, #0x9abc
; CHECK: cmpi2 r7, #39612{{.*}}encoding: [0x96,0xf7,0xbc,0x9a]
; DIS: 96 f7 bc 9a{{.*}}cmpi2 r7, #39612

; Forward data4-sized values use the full immediate encoding. Signed data
; and unsigned direct addresses must keep their distinct field semantics.
.section .text.forward,"ax",@progbits
.irp op, cmpd1, cmpd2, cmpi1, cmpi2
  \op r0, #small_value
  \op r15, #signed_value
  \op r0, zero_address
.endr
.set small_value, 15
.set signed_value, -32768
.set zero_address, 0
; DIS: a6 f0 0f 00{{.*}}cmpd1 r0, #15
; DIS-NEXT: a6 ff 00 80{{.*}}cmpd1 r15, #32768
; DIS-NEXT: a2 f0 00 00{{.*}}cmpd1 r0, 0
; DIS: b6 f0 0f 00{{.*}}cmpd2 r0, #15
; DIS-NEXT: b6 ff 00 80{{.*}}cmpd2 r15, #32768
; DIS-NEXT: b2 f0 00 00{{.*}}cmpd2 r0, 0
; DIS: 86 f0 0f 00{{.*}}cmpi1 r0, #15
; DIS-NEXT: 86 ff 00 80{{.*}}cmpi1 r15, #32768
; DIS-NEXT: 82 f0 00 00{{.*}}cmpi1 r0, 0
; DIS: 96 f0 0f 00{{.*}}cmpi2 r0, #15
; DIS-NEXT: 96 ff 00 80{{.*}}cmpi2 r15, #32768
; DIS-NEXT: 92 f0 00 00{{.*}}cmpi2 r0, 0
