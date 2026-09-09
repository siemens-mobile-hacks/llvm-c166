; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC
; RUN: ld.lld -Ttext=0x10200 -e _start %t.o \
; RUN:   --defsym=near_set=_start+64 \
; RUN:   --defsym=near_clear=_start-20 \
; RUN:   --defsym=far_set=_start+512 \
; RUN:   --defsym=far_clear=_start-512 -o %t
; RUN: llvm-objdump -d --section=.text %t | FileCheck %s --check-prefix=DIS
; RUN: llvm-objdump -d --section=.bitpc8 %t | FileCheck %s --check-prefix=DIRECT

.text
.globl _start
_start:
  jb r12.7, near_set
  jnb psw.11, near_clear
  jb r1.0, far_set
  jnb r15.15, far_clear

; RELOC:      Section {{.*}} .rela.text {
; RELOC-NEXT:   0x0 R_C166_PC8_RELAX near_set 0x0
; RELOC-NEXT:   0x8 R_C166_PC8_RELAX near_clear 0x0
; RELOC-NEXT:   0x10 R_C166_PC8_RELAX far_set 0x0
; RELOC-NEXT:   0x18 R_C166_PC8_RELAX far_clear 0x0
; RELOC-NEXT: }

; In-range branches keep their opcode and padding.
; DIS-LABEL: <_start>:
; DIS-NEXT:  10200: 8a fc 1e 70{{.*}}jb r12.7, 30
; DIS-NEXT:  10204: cc 00{{.*}}nop
; DIS-NEXT:  10206: cc 00{{.*}}nop
; DIS-NEXT:  10208: 9a 88 f0 b0{{.*}}jnb psw.11, 240
; DIS-NEXT:  1020c: cc 00{{.*}}nop
; DIS-NEXT:  1020e: cc 00{{.*}}nop

; Out-of-range branches invert the bit test and skip a segmented jump.
; DIS-NEXT:  10210: 9a f1 02 00{{.*}}jnb r1.0, 2
; DIS-NEXT:  10214: fa 01 00 04{{.*}}jmps 1, 1024
; DIS-NEXT:  10218: 8a ff 02 f0{{.*}}jb r15.15, 2
; DIS-NEXT:  1021c: fa 01 00 00{{.*}}jmps 1, 0

; Exercise the non-relaxing relocation used for an explicitly constructed
; four-byte bit branch.
.section .bitpc8,"ax",@progbits
direct_branch:
  .byte 0x8a, 0xfc, 0, 0x70
  .reloc direct_branch + 2, R_C166_BIT_PC8, direct_target
  .space 4
direct_target:
  nop

; DIRECT-LABEL: <direct_branch>:
; DIRECT-NEXT: {{[0-9a-f]+}}: 8a fc 02 70{{.*}}jb r12.7, 2
; DIRECT-LABEL: <direct_target>:
; DIRECT-NEXT: {{[0-9a-f]+}}: cc 00{{.*}}nop
