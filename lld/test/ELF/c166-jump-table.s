; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: ld.lld --section-start=.text=0x100000 --section-start=.rodata=0x603ffc -e dispatch %t.o -o %t
; RUN: llvm-readobj --relocations --symbols %t | FileCheck %s --check-prefix=ELF
; RUN: llvm-objdump -d -s --section=.text --section=.rodata %t | FileCheck %s --check-prefix=DIS
; RUN: not ld.lld --section-start=.text=0x100000 --section-start=.rodata=0x603ffe -e dispatch %t.o -o %t.cross 2>&1 | FileCheck %s --check-prefix=CROSS
; RUN: not ld.lld --section-start=.text=0x10fff0 --section-start=.rodata=0x603ffc -e dispatch %t.o -o %t.code-cross 2>&1 | FileCheck %s --check-prefix=CODE-CROSS

.c166_model large

.text
.globl dispatch
.type dispatch,@function
.c166_function huge, dispatch
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

; ELF:      Relocations [
; ELF-NEXT: ]
; ELF:      Name: jump_table
; ELF-NEXT: Value: 0x603FFC
; ELF:      Size: 4

; DIS:      Contents of section .rodata:
; DIS-NEXT:  603ffc 0e001000
; DIS-LABEL: <dispatch>:
; DIS-NEXT:  100000: e6 f0 fc 3f  mov r0, #16380
; DIS-NEXT:  100004: e6 f1 80 01  mov r1, #384
; DIS-NEXT:  100008: dc 41        extp r1, #1
; DIS-NEXT:  10000a: a8 20        mov r2, [r0]
; DIS-NEXT:  10000c: 9c 02        jmpi cc_uc, [r2]
; DIS-LABEL: <case_zero>:
; DIS-NEXT:  10000e: db 00        rets
; DIS-LABEL: <case_one>:
; DIS-NEXT:  100010: db 00        rets

; CROSS: error: {{.*}}paged data symbol 'jump_table' of size 4 crosses a 16 KiB page boundary
; CODE-CROSS: error: huge function 'dispatch' range [0x10FFF0, 0x110002) crosses a 64 KiB code segment boundary
