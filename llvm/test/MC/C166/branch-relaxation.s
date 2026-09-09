; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s -o %t.o
; RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC
; RUN: llvm-objdump -dr --section=.text %t.o | FileCheck %s --check-prefix=EXTERNAL
; RUN: llvm-objdump -dr --section=.text.forward_limit %t.o | FileCheck %s --check-prefix=FLIMIT
; RUN: llvm-objdump -dr --section=.text.backward_limit %t.o | FileCheck %s --check-prefix=BLIMIT
; RUN: llvm-objdump -dr --section=.text.forward_far %t.o | FileCheck %s --check-prefix=FFAR
; RUN: llvm-objdump -dr --section=.text.backward_far %t.o | FileCheck %s --check-prefix=BFAR

; Same-section branches remain two bytes at both representable boundaries.
; Their deltas are left to the linker so it can check the final code segment.
.section .text.forward_limit,"ax",@progbits
.globl forward_limit
forward_limit:
  jmpr cc_eq, forward_limit_target
  .space 254
forward_limit_target:
  nop

; FLIMIT-LABEL: <forward_limit>:
; FLIMIT-NEXT: 0: 2d 00{{.*}}jmpr cc_eq, 0
; FLIMIT-NEXT: {{[ ]*}}1: R_C166_PC8{{[ \t]+}}.text.forward_limit+0x100
; FLIMIT-LABEL: <forward_limit_target>:
; FLIMIT-NEXT: 100: cc 00{{.*}}nop

.section .text.backward_limit,"ax",@progbits
backward_limit_target:
  nop
  .space 252
.globl backward_limit
backward_limit:
  jmpr cc_ne, backward_limit_target

; BLIMIT-LABEL: <backward_limit_target>:
; BLIMIT-NEXT: 0: cc 00{{.*}}nop
; BLIMIT-LABEL: <backward_limit>:
; BLIMIT-NEXT: fe: 3d 00{{.*}}jmpr cc_ne, 0
; BLIMIT-NEXT: {{[ ]*}}ff: R_C166_PC8{{[ \t]+}}.text.backward_limit

; A local branch outside the short range gets a fixed replacement slot and a
; linker relocation.  Conditional slots contain two NOP words; unconditional
; slots need one.
.section .text.forward_far,"ax",@progbits
.globl forward_far
forward_far:
  jmpr cc_ult, forward_far_target
  .space 256
forward_far_target:
  nop

; FFAR-LABEL: <forward_far>:
; FFAR-NEXT: 0: 8d 00{{.*}}jmpr cc_ult, 0
; FFAR-NEXT: {{[ ]*}}0: R_C166_PC8_RELAX{{[ \t]+}}.text.forward_far+0x106
; FFAR-NEXT: 2: cc 00{{.*}}nop
; FFAR-NEXT: 4: cc 00{{.*}}nop
; FFAR-LABEL: <forward_far_target>:
; FFAR-NEXT: 106: cc 00{{.*}}nop

.section .text.backward_far,"ax",@progbits
backward_far_target:
  nop
  .space 254
.globl backward_far
backward_far:
  jmpr cc_uc, backward_far_target

; BFAR-LABEL: <backward_far_target>:
; BFAR-NEXT: 0: cc 00{{.*}}nop
; BFAR-LABEL: <backward_far>:
; BFAR-NEXT: 100: 0d 00{{.*}}jmpr cc_uc, 0
; BFAR-NEXT: {{[ ]*}}100: R_C166_PC8_RELAX{{[ \t]+}}.text.backward_far
; BFAR-NEXT: 102: cc 00{{.*}}nop

; Undefined targets always reserve a slot because only the linker knows their
; final distance.
.text
.globl external_branches
external_branches:
  jmpr cc_sgt, external_conditional
  jmpr cc_uc, external_unconditional

; EXTERNAL-LABEL: <external_branches>:
; EXTERNAL-NEXT: 0: ad 00{{.*}}jmpr cc_sgt, 0
; EXTERNAL-NEXT: {{[ ]*}}0: R_C166_PC8_RELAX{{[ \t]+}}external_conditional
; EXTERNAL-NEXT: 2: cc 00{{.*}}nop
; EXTERNAL-NEXT: 4: cc 00{{.*}}nop
; EXTERNAL-NEXT: 6: 0d 00{{.*}}jmpr cc_uc, 0
; EXTERNAL-NEXT: {{[ ]*}}6: R_C166_PC8_RELAX{{[ \t]+}}external_unconditional
; EXTERNAL-NEXT: 8: cc 00{{.*}}nop

; RELOC:      Section {{.*}} .rela.text {
; RELOC-NEXT:   0x0 R_C166_PC8_RELAX external_conditional 0x0
; RELOC-NEXT:   0x6 R_C166_PC8_RELAX external_unconditional 0x0
; RELOC-NEXT: }
; RELOC:      Section {{.*}} .rela.text.forward_far {
; RELOC-NEXT:   0x0 R_C166_PC8_RELAX .text.forward_far 0x106
; RELOC-NEXT: }
; RELOC:      Section {{.*}} .rela.text.backward_far {
; RELOC-NEXT:   0x100 R_C166_PC8_RELAX .text.backward_far 0x0
; RELOC-NEXT: }
