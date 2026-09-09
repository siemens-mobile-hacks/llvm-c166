; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-readobj -r %t | FileCheck %s --check-prefix=RELOC

; A segment materialized in a word field must not overwrite the register byte.
mov r4, #seg(object)
; CHECK: encoding: [0xe6,0xf4,A,0x00]
; CHECK: fixup A - offset: 2, value: object, kind: fixup_c166_seg8
mov mdc, #seg(object)
; CHECK: encoding: [0xe6,0x87,A,0x00]
; CHECK: fixup A - offset: 2, value: object, kind: fixup_c166_seg8
scxt r4, #seg(object)
; CHECK: encoding: [0xc6,0xf4,A,0x00]
; CHECK: fixup A - offset: 2, value: object, kind: fixup_c166_seg8
add r4, #seg(object)
; CHECK: encoding: [0x06,0xf4,A,0x00]
; CHECK: fixup A - offset: 2, value: object, kind: fixup_c166_seg8
calls seg(object), 0x5678
; CHECK: encoding: [0xda,A,0x78,0x56]
; CHECK: fixup A - offset: 1, value: object, kind: fixup_c166_seg8
jmps seg(object), 0x5678
; CHECK: encoding: [0xfa,A,0x78,0x56]
; CHECK: fixup A - offset: 1, value: object, kind: fixup_c166_seg8

; RELOC:      0x2 R_C166_SEG8 object 0x0
; RELOC-NEXT: 0x6 R_C166_SEG8 object 0x0
; RELOC-NEXT: 0xA R_C166_SEG8 object 0x0
; RELOC-NEXT: 0xE R_C166_SEG8 object 0x0
; RELOC-NEXT: 0x11 R_C166_SEG8 object 0x0
; RELOC-NEXT: 0x15 R_C166_SEG8 object 0x0
