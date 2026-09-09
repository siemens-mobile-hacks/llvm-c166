; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -s %t | FileCheck %s --check-prefix=BYTES
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -filetype=obj -o %t.roundtrip
; RUN: cmp %t %t.roundtrip

bset low
bclr high
bmov low, high
bmov high, low
bfldl word_low, #1, #2
bfldh word_high, #1, #2
jb high, 1
jnb low, 2
jbc high, external
jnbs low, external
.equ low, 0
.equ high, 4095
.equ word_low, 0
.equ word_high, 255

; BYTES: Contents of section .text:
; BYTES-NEXT: 0000 0f00feff 4aff00f0 4a00ff0f 0a000102
; BYTES-NEXT: 0010 1aff0201 8aff01f0 9a000200 aaff00f0
; BYTES-NEXT: 0020 cc00cc00 cc00ba00 0000cc00 cc00cc00
