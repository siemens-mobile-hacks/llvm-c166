; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: ld.lld --section-start=.text=0x180000 --section-start=.c166.near.text=0x188000 -e _start %t.o -o %t
; RUN: llvm-readobj --relocations --symbols %t | FileCheck %s --check-prefix=ELF
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
; RUN: not ld.lld --section-start=.text=0x180000 --section-start=.c166.near.text=0x190000 -e _start %t.o -o %t.cross 2>&1 | FileCheck %s --check-prefix=CROSS

.text
.globl _start
.type _start,@function
_start:
  calla cc_uc, cof(_near_callee)
  jmpa cc_uc, cof(_near_branch)
  rets

.section .c166.near.text,"ax",@progbits
.globl _near_callee
.type _near_callee,@function
_near_callee:
  ret
.globl _near_branch
.type _near_branch,@function
_near_branch:
  ret

; ELF:      Relocations [
; ELF-NEXT: ]
; ELF:      Name: _near_callee
; ELF:      Value: 0x188000
; ELF:      Name: _near_branch
; ELF:      Value: 0x188002

; DIS-LABEL: <_start>:
; DIS-NEXT:  180000: ca 00 00 80  calla cc_uc, 32768
; DIS-NEXT:  180004: ea 00 02 80  jmpa cc_uc, 32770
; DIS-NEXT:  180008: db 00        rets
; DIS-LABEL: <_near_callee>:
; DIS-NEXT:  188000: cb 00        ret

; CROSS: error: {{.*}}same-segment code relocation to '_near_callee' crosses a 64 KiB code boundary
