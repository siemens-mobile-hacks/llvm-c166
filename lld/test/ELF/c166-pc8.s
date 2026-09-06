; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %S/Inputs/c166-pc8-target.s -o %t.target.o
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %S/Inputs/c166-pc8-far.s -o %t.far.o
; RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC
; RUN: ld.lld -Ttext=0x10000 -e _start %t.o %t.target.o -o %t.near
; RUN: llvm-objdump -d --section=.text %t.near | FileCheck %s --check-prefix=NEAR
; RUN: ld.lld -Ttext=0x10000 -e _start %t.o %t.far.o -o %t.far
; RUN: llvm-objdump -d --section=.text %t.far | FileCheck %s --check-prefix=FAR

; Undefined short branches reserve fixed-size slots.  At final layout LLD
; either keeps the JMPR and its NOP padding or rewrites the slot without moving
; any following code.  Cover every condition inversion used by the far form.
.text
.globl _start
.type _start,@function
_start:
  jmpr cc_eq, target
  jmpr cc_ne, target
  jmpr cc_ult, target
  jmpr cc_uge, target
  jmpr cc_sgt, target
  jmpr cc_sle, target
  jmpr cc_slt, target
  jmpr cc_sge, target
  jmpr cc_ugt, target
  jmpr cc_ule, target
  jmpr cc_uc, target
  nop

; RELOC:      Section {{.*}} .rela.text {
; RELOC-NEXT:   0x0 R_C166_PC8_RELAX target 0x0
; RELOC-NEXT:   0x6 R_C166_PC8_RELAX target 0x0
; RELOC-NEXT:   0xC R_C166_PC8_RELAX target 0x0
; RELOC-NEXT:   0x12 R_C166_PC8_RELAX target 0x0
; RELOC-NEXT:   0x18 R_C166_PC8_RELAX target 0x0
; RELOC-NEXT:   0x1E R_C166_PC8_RELAX target 0x0
; RELOC-NEXT:   0x24 R_C166_PC8_RELAX target 0x0
; RELOC-NEXT:   0x2A R_C166_PC8_RELAX target 0x0
; RELOC-NEXT:   0x30 R_C166_PC8_RELAX target 0x0
; RELOC-NEXT:   0x36 R_C166_PC8_RELAX target 0x0
; RELOC-NEXT:   0x3C R_C166_PC8_RELAX target 0x0
; RELOC-NEXT: }

; NEAR-LABEL: <_start>:
; NEAR-NEXT:  10000: 2d 20{{.*}}jmpr cc_eq, 32
; NEAR-NEXT:  10002: cc 00{{.*}}nop
; NEAR-NEXT:  10004: cc 00{{.*}}nop
; NEAR-NEXT:  10006: 3d 1d{{.*}}jmpr cc_ne, 29
; NEAR:       1003c: 0d 02{{.*}}jmpr cc_uc, 2
; NEAR-NEXT:  1003e: cc 00{{.*}}nop
; NEAR-NEXT:  10040: cc 00{{.*}}nop
; NEAR-LABEL: <target>:
; NEAR-NEXT:  10042: db 00{{.*}}rets

; FAR-LABEL: <_start>:
; FAR-NEXT:  10000: 3d 02{{.*}}jmpr cc_ne, 2
; FAR-NEXT:  10002: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  10006: 2d 02{{.*}}jmpr cc_eq, 2
; FAR-NEXT:  10008: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  1000c: 9d 02{{.*}}jmpr cc_uge, 2
; FAR-NEXT:  1000e: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  10012: 8d 02{{.*}}jmpr cc_ult, 2
; FAR-NEXT:  10014: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  10018: bd 02{{.*}}jmpr cc_sle, 2
; FAR-NEXT:  1001a: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  1001e: ad 02{{.*}}jmpr cc_sgt, 2
; FAR-NEXT:  10020: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  10024: dd 02{{.*}}jmpr cc_sge, 2
; FAR-NEXT:  10026: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  1002a: cd 02{{.*}}jmpr cc_slt, 2
; FAR-NEXT:  1002c: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  10030: fd 02{{.*}}jmpr cc_ule, 2
; FAR-NEXT:  10032: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  10036: ed 02{{.*}}jmpr cc_ugt, 2
; FAR-NEXT:  10038: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  1003c: fa 01 42 02{{.*}}jmps 1, 578
; FAR-NEXT:  10040: cc 00{{.*}}nop
; FAR-LABEL: <target>:
; FAR-NEXT:  10242: db 00{{.*}}rets
