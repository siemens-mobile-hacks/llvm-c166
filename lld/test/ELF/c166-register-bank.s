; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC
; RUN: ld.lld -m c166elf -Ttext=0x180000 \
; RUN:   --section-start=.c166.near.bss=0x5800 -e _start %t.o -o %t
; RUN: llvm-readobj --relocations --symbols --sections %t | FileCheck %s --check-prefix=ELF
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
; C166-ABI: calls.interrupt_named_register_bank

.text
.globl _start
.type _start,@function
_start:
  mov bank, r0
  scxt cp, #bank
  nop
  pop cp
  reti

.section .c166.near.bss,"aw",@nobits
.weak bank
.type bank,@object
.size bank,32
bank:
  .space 32

; RELOC: 0x2 R_C166_16 bank 0x0
; RELOC: 0x6 R_C166_16 bank 0x0

; ELF:      Name: .c166.near.bss
; ELF:      Address: 0x5800
; ELF:      Size: 32
; ELF:      Relocations [
; ELF-NEXT: ]
; ELF:      Name: bank
; ELF:      Value: 0x5800
; ELF:      Size: 32
; ELF:      Binding: Weak

; DIS-LABEL: <_start>:
; DIS-NEXT: 180000: f6 f0 00 58  mov dpp1(6144), r0
; DIS-NEXT: 180004: c6 08 00 58  scxt cp, #22528
; DIS-NEXT: 180008: cc 00        nop
; DIS-NEXT: 18000a: fc 08        pop cp
; DIS-NEXT: 18000c: fb 88        reti
