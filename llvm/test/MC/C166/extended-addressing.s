; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -mcpu=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -mcpu=generic -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS

extr #1
; CHECK: encoding: [0xd1,0x80]
; DIS: extr #1
extr #4
; CHECK: encoding: [0xd1,0xb0]
; DIS: extr #4
extpr r0, #1
; CHECK: encoding: [0xdc,0xc0]
; DIS: extpr r0, #1
extpr r15, #4
; CHECK: encoding: [0xdc,0xff]
; DIS: extpr r15, #4
extsr r0, #1
; CHECK: encoding: [0xdc,0x80]
; DIS: extsr r0, #1
extsr r15, #4
; CHECK: encoding: [0xdc,0xbf]
; DIS: extsr r15, #4
extpr #0, #1
; CHECK: encoding: [0xd7,0xc0,0x00,0x00]
; DIS: extpr 0, #1
extpr #1023, #4
; CHECK: encoding: [0xd7,0xf0,0xff,0x03]
; DIS: extpr 1023, #4
exts #0, #1
; CHECK: encoding: [0xd7,0x00,0x00,0x00]
; DIS: exts 0, #1
exts #255, #4
; CHECK: encoding: [0xd7,0x30,0xff,0x00]
; DIS: exts 255, #4
extsr #0, #1
; CHECK: encoding: [0xd7,0x80,0x00,0x00]
; DIS: extsr 0, #1
extsr #255, #4
; CHECK: encoding: [0xd7,0xb0,0xff,0x00]
; DIS: extsr 255, #4

extpr #pag(object), #2
; CHECK: fixup A - offset: 2, value: object, kind: fixup_c166_pag10
; DIS: R_C166_PAG10 object
exts #seg(object), #2
; CHECK: encoding: [0xd7,0x10,A,0x00]
; CHECK: fixup A - offset: 2, value: object, kind: fixup_c166_seg8
; DIS: R_C166_SEG8 object

; Both sequence instructions encode count minus one in bits 13:12.
.irp count, 1, 2, 3, 4
  atomic #\count
  extr #\count
.endr
; CHECK: atomic #1{{.*}}encoding: [0xd1,0x00]
; CHECK: extr #1{{.*}}encoding: [0xd1,0x80]
; CHECK: atomic #2{{.*}}encoding: [0xd1,0x10]
; CHECK: extr #2{{.*}}encoding: [0xd1,0x90]
; CHECK: atomic #3{{.*}}encoding: [0xd1,0x20]
; CHECK: extr #3{{.*}}encoding: [0xd1,0xa0]
; CHECK: atomic #4{{.*}}encoding: [0xd1,0x30]
; CHECK: extr #4{{.*}}encoding: [0xd1,0xb0]
; DIS: atomic #1
; DIS-NEXT: extr #1
; DIS-NEXT: atomic #2
; DIS-NEXT: extr #2
; DIS-NEXT: atomic #3
; DIS-NEXT: extr #3
; DIS-NEXT: atomic #4
; DIS-NEXT: extr #4
extsr #seg(object), #3
; CHECK: encoding: [0xd7,0xa0,A,0x00]
; CHECK: fixup A - offset: 2, value: object, kind: fixup_c166_seg8
; DIS: R_C166_SEG8 object
