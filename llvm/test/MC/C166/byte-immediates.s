; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS

; MOVB reg,#data8: E7 RR ## xx (canonical padding byte is zero).
movb rl0, #15
; CHECK: encoding: [0xe1,0xf0]
movb rl0, #16
; CHECK: encoding: [0xe7,0xf0,0x10,0x00]
; DIS: movb rl0, #16
movb rh7, #255
; CHECK: encoding: [0xe7,0xff,0xff,0x00]
; DIS: movb rh7, #255
movb rl0, #-128
; CHECK: encoding: [0xe7,0xf0,0x80,0x00]
; DIS: movb rl0, #128
movb rh7, #-1
; CHECK: encoding: [0xe7,0xff,0xff,0x00]
; DIS: movb rh7, #255
addb rl0, #-128
; CHECK: encoding: [0x07,0xf0,0x80,0x00]
; DIS: addb rl0, #128
cmpb rh7, #-1
; CHECK: encoding: [0x47,0xff,0xff,0x00]
; DIS: cmpb rh7, #255

movb rl0, #external+1
; CHECK: fixup A - offset: 2, value: external+1, kind: FK_Data_1
; DIS: R_C166_8 external+0x1
addb rh7, #external+2
; CHECK: fixup A - offset: 2, value: external+2, kind: FK_Data_1
; DIS: R_C166_8 external+0x2
movb rh7, #forward
; CHECK: fixup A - offset: 2, value: forward, kind: FK_Data_1
.set forward, -1
; DIS: movb rh7, #255
