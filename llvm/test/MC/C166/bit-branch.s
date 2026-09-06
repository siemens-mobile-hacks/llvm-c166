; RUN: llvm-mc -triple=c166-none-elf -show-encoding %s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=OBJ
; RUN: llvm-readobj --relocations %t | FileCheck %s --check-prefix=RELOC

.text
.globl bit_branches
bit_branches:
  jb r12.7, 127
  jnb psw.11, 128

; ASM: jb r12.7, 127{{.*}}encoding: [0x8a,0xfc,0x7f,0x70]
; ASM: jnb psw.11, 128{{.*}}encoding: [0x9a,0x88,0x80,0xb0]
; OBJ: 8a fc 7f 70{{.*}}jb r12.7, 127
; OBJ: 9a 88 80 b0{{.*}}jnb psw.11, 128

.globl external_bit_branches
external_bit_branches:
  jb r0.0, external_target
  jnb r15.15, external_target

; ASM: jb r0.0, external_target{{.*}}encoding: [0x8a,0xf0,A,0x00]
; ASM: fixup A - offset: 2, value: external_target, kind: fixup_c166_bit_pc8
; ASM: jnb r15.15, external_target{{.*}}encoding: [0x9a,0xff,A,0xf0]
; ASM: fixup A - offset: 2, value: external_target, kind: fixup_c166_bit_pc8

; Undefined targets use fixed eight-byte relaxation slots.
; OBJ-LABEL: <external_bit_branches>:
; OBJ-NEXT: 8: 8a f0 00 00{{.*}}jb r0.0, 0
; OBJ-NEXT: {{[ ]*}}8: R_C166_PC8_RELAX{{[ \t]+}}external_target
; OBJ-NEXT: c: cc 00{{.*}}nop
; OBJ-NEXT: e: cc 00{{.*}}nop
; OBJ-NEXT: 10: 9a ff 00 f0{{.*}}jnb r15.15, 0
; OBJ-NEXT: {{[ ]*}}10: R_C166_PC8_RELAX{{[ \t]+}}external_target
; OBJ-NEXT: 14: cc 00{{.*}}nop
; OBJ-NEXT: 16: cc 00{{.*}}nop

; RELOC:      Section {{.*}} .rela.text {
; RELOC-NEXT:   0x8 R_C166_PC8_RELAX external_target 0x0
; RELOC-NEXT:   0x10 R_C166_PC8_RELAX external_target 0x0
; RELOC-NEXT: }
