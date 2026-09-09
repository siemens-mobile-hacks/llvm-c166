; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -s --section=.text --section=.text.forward %t | FileCheck %s

; The same field limits apply before and after resolving forward constants.
mov r4, #maximum
scxt r4, maximum
mov r4, [r5 + maximum]
.set maximum, 65535
mov r4, #sof(0xffffff)
mov r4, #pof(0xffffff)
mov r4, #dpp1(0xffffff)
mov r4, #dpp2(0xffffff)
.short sof(0x123456)
.long paged(0x123456)

; CHECK:      Contents of section .text:
; CHECK-NEXT: 0000 e6f4ffff d6f4ffff d445ffff e6f4ffff
; CHECK-NEXT: 0010 e6f4ff3f e6f4ff7f e6f4ffbf 56345634
; CHECK-NEXT: 0020 4800

; Forward constants use full-width encodings even when their eventual values
; fit compact forms. Signed immediates and displacements retain their bits.
.section .text.forward,"ax",@progbits
mov r4, #signed_word
scxt r4, #signed_word
mov r4, [r5 + signed_word]
movb rl2, #signed_byte
addb rh7, #signed_byte
mov r4, #compact_word
movb rl2, #compact_byte
add r4, #compact_alu
addb rh7, #compact_alu
.set signed_word, -32768
.set signed_byte, -128
.set compact_word, 15
.set compact_byte, 15
.set compact_alu, 7

; CHECK:      Contents of section .text.forward:
; CHECK-NEXT: 0000 e6f40080 c6f40080 d4450080 e7f48000
; CHECK-NEXT: 0010 07ff8000 e6f40f00 e7f40f00 06f40700
; CHECK-NEXT: 0020 07ff0700
