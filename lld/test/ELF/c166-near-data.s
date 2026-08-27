; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %S/Inputs/c166-near-data.s -o %t.data.o
; RUN: ld.lld -Ttext=0x180000 --section-start=.c166.near.data=0x5000 --section-start=.c166.xnear.data=0x6000 -e _start %t.o %t.data.o -o %t
; RUN: llvm-readobj --relocations --symbols %t | FileCheck %s --check-prefix=ELF
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
; RUN: not ld.lld -Ttext=0x180000 --section-start=.c166.near.data=0x7ffe --section-start=.c166.xnear.data=0x6000 -e _start %t.o %t.data.o -o %t.cross 2>&1 | FileCheck %s --check-prefix=CROSS

.text
.globl _start
.type _start,@function
_start:
  mov r4, dpp2(_near_data)
  mov dpp2(_near_data), r12
  mov r5, dpp1(_xnear_data)
  mov dpp1(_xnear_data), r13
  mov r6, #dpp2(_near_data)
  mov r7, #dpp1(_xnear_data)
  rets

; ELF:      Relocations [
; ELF-NEXT: ]
; ELF:      Name: _near_data
; ELF:      Value: 0x5000
; ELF:      Name: _xnear_data
; ELF:      Value: 0x6000

; DIS-LABEL: <_start>:
; DIS-NEXT:  180000: f2 f4 00 90  mov r4, dpp2(4096)
; DIS-NEXT:  180004: f6 fc 00 90  mov dpp2(4096), r12
; DIS-NEXT:  180008: f2 f5 00 60  mov r5, dpp1(8192)
; DIS-NEXT:  18000c: f6 fd 00 60  mov dpp1(8192), r13
; DIS-NEXT:  180010: e6 f6 00 90  mov r6, #36864
; DIS-NEXT:  180014: e6 f7 00 60  mov r7, #24576
; DIS-NEXT:  180018: db 00        rets

; CROSS: error: {{.*}}near data symbol '_near_data' of size 4 crosses a 16 KiB DPP page boundary
