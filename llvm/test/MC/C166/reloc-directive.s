; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s -o - | llvm-readobj --relocations - | FileCheck %s

; Keep every LLVM-owned C166 relocation name available to .reloc.  Besides
; enabling hand-written object tests, this makes it possible to exercise LLD
; boundary cases which do not have a corresponding instruction yet.

.text
.globl relocations
relocations:
  .space 32

.reloc relocations + 0, R_C166_NONE, target
.reloc relocations + 1, R_C166_8, target + 1
.reloc relocations + 2, R_C166_16, target + 2
.reloc relocations + 4, R_C166_32, target + 3
.reloc relocations + 8, R_C166_SEG8, target + 4
.reloc relocations + 9, R_C166_SOF16, target + 5
.reloc relocations + 11, R_C166_PAG10, target + 6
.reloc relocations + 13, R_C166_POF14, target + 7
.reloc relocations + 15, R_C166_PC8, target + 8
.reloc relocations + 16, R_C166_PC16, target + 9
.reloc relocations + 18, R_C166_DPP1_16, target + 10
.reloc relocations + 20, R_C166_DPP2_16, target + 11
.reloc relocations + 22, R_C166_COF16, target + 12
.reloc relocations + 24, R_C166_PAGED32, target + 13
.reloc relocations + 28, R_C166_SEG24, target + 14

; CHECK:      Relocations [
; CHECK-NEXT:   Section {{.*}} .rela.text {
; CHECK-NEXT:     0x0 R_C166_NONE target 0x0
; CHECK-NEXT:     0x1 R_C166_8 target 0x1
; CHECK-NEXT:     0x2 R_C166_16 target 0x2
; CHECK-NEXT:     0x4 R_C166_32 target 0x3
; CHECK-NEXT:     0x8 R_C166_SEG8 target 0x4
; CHECK-NEXT:     0x9 R_C166_SOF16 target 0x5
; CHECK-NEXT:     0xB R_C166_PAG10 target 0x6
; CHECK-NEXT:     0xD R_C166_POF14 target 0x7
; CHECK-NEXT:     0xF R_C166_PC8 target 0x8
; CHECK-NEXT:     0x10 R_C166_PC16 target 0x9
; CHECK-NEXT:     0x12 R_C166_DPP1_16 target 0xA
; CHECK-NEXT:     0x14 R_C166_DPP2_16 target 0xB
; CHECK-NEXT:     0x16 R_C166_COF16 target 0xC
; CHECK-NEXT:     0x18 R_C166_PAGED32 target 0xD
; CHECK-NEXT:     0x1C R_C166_SEG24 target 0xE
; CHECK-NEXT:   }
; CHECK-NEXT: ]

.globl target
