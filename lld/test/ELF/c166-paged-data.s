; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %S/Inputs/c166-paged-data.s -o %t.data.o
; RUN: ld.lld -Ttext=0x12340 --section-start=.data=0x1c234 -e _start %t.o %t.data.o -o %t
; RUN: llvm-readobj --relocations --symbols %t | FileCheck %s --check-prefix=ELF
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
; RUN: not ld.lld -Ttext=0x12340 --section-start=.data=0x1fffe -e _start %t.o %t.data.o -o %t.cross 2>&1 | FileCheck %s --check-prefix=CROSS

.text
.globl _start
.type _start,@function
_start:
  extp pag(_far_data), #1
  mov r4, pof(_far_data)
  rets

; ELF:      Relocations [
; ELF-NEXT: ]
; ELF:      Name: _far_data
; ELF:      Value: 0x1C234

; DIS-LABEL: <_start>:
; DIS-NEXT:  12340: d7 40 07 00  extp 7, #1
; DIS-NEXT:  12344: f2 f4 34 02  mov r4, 564
; DIS-NEXT:  12348: db 00        rets

; CROSS: error: {{.*}}paged data symbol '_far_data' of size 4 crosses a 16 KiB page boundary
