; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -s %t | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -filetype=obj -o %t.roundtrip
; RUN: cmp %t %t.roundtrip

bset word . bit
bclr psw . (bit - 15)
bmov word . bit, 0 . bit
bmov 0 . bit, word . bit
jb word . bit, 1
jbc word . bit, external
.equ word, 255
.equ bit, 15

; CHECK: Contents of section .text:
; CHECK-NEXT: 0000 ffff0e88 4a00ffff 4aff00ff 8aff01f0
; CHECK-NEXT: 0010 aaff00f0 cc00cc00 cc00
