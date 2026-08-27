; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s -o - | llvm-readobj --sections --symbols --relocations - | FileCheck %s

.c166_model large

.text
.globl dispatch
.type dispatch,@function
dispatch:
  mov r0, #pof(jump_table)
  mov r1, #pag(jump_table)
  extp r1, #1
  mov r2, [r0]
  jmpi cc_uc, [r2]

case_zero:
  rets
case_one:
  rets
.size dispatch, .-dispatch

.section .rodata,"a",@progbits
.p2align 1
.type jump_table,@object
.c166_data far, jump_table
jump_table:
  .short sof(case_zero)
  .short sof(case_one)
.size jump_table, .-jump_table

; CHECK:      Name: .rodata
; CHECK:      Relocations [
; CHECK:        Section {{.*}} .rela.text {
; CHECK-NEXT:     0x2 R_C166_POF14 jump_table 0x0
; CHECK-NEXT:     0x6 R_C166_PAG10 jump_table 0x0
; CHECK:        Section {{.*}} .rela.rodata {
; CHECK-NEXT:     0x0 R_C166_SOF16 .text 0xE
; CHECK-NEXT:     0x2 R_C166_SOF16 .text 0x10
; CHECK:      Name: jump_table
; CHECK:      Size: 4
; CHECK:      Binding: Local
; CHECK:      Type: Object
; CHECK:      Other [ (0x60)
; CHECK-NEXT:   STO_C166_DATA_FAR
