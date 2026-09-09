; RUN: llvm-mc -triple=c166-none-elf -show-encoding %s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s -o %t.o
; RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
; RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS
; C166-ABI: calls.interrupt_named_register_bank

mov bank, r0
; ASM: mov bank, r0{{.*}}encoding: [0xf6,0xf0,A,A]
; ASM: fixup A - offset: 2, value: bank, kind: fixup_c166_address16
; DIS: f6 f0 00 00{{.*}}mov 0, r0

scxt cp, #bank
; ASM: scxt cp, #bank{{.*}}encoding: [0xc6,0x08,A,A]
; ASM: fixup A - offset: 2, value: bank, kind: FK_Data_2
; DIS: c6 08 00 00{{.*}}scxt cp, #0

; Relocatable direct-address loads and byte stores use the full 16-bit
; absolute forms. These are also the forms used for Small-model globals.
mov r0, bank
; ASM: mov r0, bank{{.*}}encoding: [0xf2,0xf0,A,A]
; ASM: fixup A - offset: 2, value: bank, kind: fixup_c166_address16
; DIS: f2 f0 00 00{{.*}}mov r0, 0

movbz r0, bank
; ASM: movbz r0, bank{{.*}}encoding: [0xc2,0xf0,A,A]
; ASM: fixup A - offset: 2, value: bank, kind: fixup_c166_address16
; DIS: c2 f0 00 00{{.*}}movbz r0, 0

movbs r0, bank
; ASM: movbs r0, bank{{.*}}encoding: [0xd2,0xf0,A,A]
; ASM: fixup A - offset: 2, value: bank, kind: fixup_c166_address16
; DIS: d2 f0 00 00{{.*}}movbs r0, 0

movb bank, rl0
; ASM: movb bank, rl0{{.*}}encoding: [0xf7,0xf0,A,A]
; ASM: fixup A - offset: 2, value: bank, kind: fixup_c166_address16
; DIS: f7 f0 00 00{{.*}}movb 0, rl0

; RELOC: 0x2 R_C166_16 bank 0x0
; RELOC: 0x6 R_C166_16 bank 0x0
; RELOC: 0xA R_C166_16 bank 0x0
; RELOC: 0xE R_C166_16 bank 0x0
; RELOC: 0x12 R_C166_16 bank 0x0
; RELOC: 0x16 R_C166_16 bank 0x0
