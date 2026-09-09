; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -s %t | FileCheck %s --check-prefix=DATA
; RUN: llvm-readobj --sections --symbols --relocations %t | FileCheck %s --check-prefix=ELF

.section .rodata,"a",@progbits
.p2align 1
.global words
.type words,@object
words:
.equ value, 0x1234
.word value, -1
.short 0x5678
.size words, .-words
.long external+4

.section .text,"ax",@progbits
.macro load reg, value
mov \reg, #\value
.endm
.rept 2
load r4, 3
.endr
.if value == 0x1234
nop
.else
.error "wrong constant value"
.endif

.section .bss,"aw",@nobits
.p2align 1
.local buffer
.type buffer,@object
buffer:
.zero 12
.size buffer, .-buffer

; DATA: Contents of section .text:
; DATA-NEXT: 0000 e034e034 cc00
; DATA: Contents of section .rodata:
; DATA-NEXT: 0000 3412ffff 78560000 0000
; ELF: Name: .bss
; ELF: Type: SHT_NOBITS
; ELF: Size: 12
; ELF: R_C166_32 external 0x4
; ELF: Name: buffer
; ELF: Size: 12
; ELF: Binding: Local
; ELF: Name: words
; ELF: Size: 6
; ELF: Binding: Global
