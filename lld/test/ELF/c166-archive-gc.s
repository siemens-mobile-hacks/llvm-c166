# REQUIRES: c166
# RUN: split-file %s %t
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=0 %t/main.s -o %t.main.l.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=0 %t/member.s -o %t.member.l.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=0 %t/unused.s -o %t.unused.l.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=0 %t/strong.s -o %t.strong.l.o
# RUN: llvm-ar crs %t.l.a %t.member.l.o %t.unused.l.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=1 %t/main.s -o %t.main.m.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=1 %t/member.s -o %t.member.m.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=1 %t/unused.s -o %t.unused.m.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=1 %t/strong.s -o %t.strong.m.o
# RUN: llvm-ar crs %t.m.a %t.member.m.o %t.unused.m.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=2 %t/main.s -o %t.main.s.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=2 %t/member.s -o %t.member.s.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=2 %t/unused.s -o %t.unused.s.o
# RUN: llvm-mc -triple=c166 -filetype=obj -I %t --defsym=MODEL=2 %t/strong.s -o %t.strong.s.o
# RUN: llvm-ar crs %t.s.a %t.member.s.o %t.unused.s.o
# RUN: ld.lld --gc-sections -T %t/layout.ld --defsym=TEXT_ADDR=0x1000 --defsym=DATA_ADDR=0x2000 %t.main.l.o %t.l.a %t.strong.l.o -o %t.l
# RUN: ld.lld --gc-sections -T %t/layout.ld --defsym=TEXT_ADDR=0x1000 --defsym=DATA_ADDR=0x2000 %t.main.m.o %t.m.a %t.strong.m.o -o %t.m
# RUN: ld.lld --gc-sections -T %t/layout.ld --defsym=TEXT_ADDR=0x1000 --defsym=DATA_ADDR=0x2000 %t.main.s.o %t.s.a %t.strong.s.o -o %t.s
# RUN: llvm-objdump -s -j .rodata -j .data %t.l %t.m %t.s | FileCheck %s --check-prefix=FIRST --match-full-lines
# RUN: llvm-objdump -s -j .data %t.l %t.m %t.s | FileCheck %s --check-prefix=DATA1 --implicit-check-not=1111
# RUN: llvm-nm --print-size %t.l %t.m %t.s | FileCheck %s --check-prefix=SYMS --implicit-check-not=dead --implicit-check-not=unused --implicit-check-not=missing
# RUN: ld.lld --gc-sections -T %t/layout.ld --defsym=TEXT_ADDR=0x170000 --defsym=DATA_ADDR=0x4000 %t.main.l.o %t.l.a %t.strong.l.o -o %t.l2
# RUN: ld.lld --gc-sections -T %t/layout.ld --defsym=TEXT_ADDR=0x7000 --defsym=DATA_ADDR=0x4000 %t.main.m.o %t.m.a %t.strong.m.o -o %t.m2
# RUN: ld.lld --gc-sections -T %t/layout.ld --defsym=TEXT_ADDR=0x170000 --defsym=DATA_ADDR=0x4000 %t.main.s.o %t.s.a %t.strong.s.o -o %t.s2
# RUN: llvm-objdump -s -j .rodata -j .data %t.l2 %t.m2 %t.s2 | FileCheck %s --check-prefix=SECOND --match-full-lines
# RUN: llvm-objdump -s -j .data %t.l2 %t.m2 %t.s2 | FileCheck %s --check-prefix=DATA2 --implicit-check-not=1111
# RUN: llvm-objdump -d %t.l %t.m %t.s | FileCheck %s --check-prefix=CODE1
# RUN: llvm-objdump -d %t.l2 %t.m2 %t.s2 | FileCheck %s --check-prefix=CODE2

## The weak undefined reference must not extract unused.o. An undefined
## reference in the extracted member's dead section must disappear with GC.
## COMMON has the larger size/alignment from member.o; override is strong.
# FIRST-COUNT-3: {{ *}}2000 44200000 20200000 00000000 {{.*}}
# SECOND-COUNT-3: {{ *}}4000 44400000 20400000 00000000 {{.*}}
# DATA1-COUNT-3: 2020 2222
# DATA2-COUNT-3: 4020 2222
# SYMS-COUNT-3: 00002044 00000008 B common
# CODE1: 1004: da 00 0a 10{{.*}}calls
# CODE1: 0000100a <worker>:
# CODE1: 1004: ca 00 0a 10{{.*}}calla
# CODE1: 0000100a <worker>:
# CODE1: 1004: da 00 0a 10{{.*}}calls
# CODE1: 0000100a <worker>:
# CODE2: 170004: da 17 0a 00{{.*}}calls
# CODE2: 0017000a <worker>:
# CODE2: 7004: ca 00 0a 70{{.*}}calla
# CODE2: 0000700a <worker>:
# CODE2: 170004: da 17 0a 00{{.*}}calls
# CODE2: 0017000a <worker>:

#--- model.inc
.if MODEL == 0
.c166_model large
.elseif MODEL == 1
.c166_model medium
.else
.c166_model small
.endif
.macro return
.if MODEL == 1
  ret
.else
  rets
.endif
.endm

#--- main.s
.include "model.inc"
.section .text.start,"ax",@progbits
.globl _start
_start:
  mov r4, #sof(table)
.if MODEL == 1
  calla cc_uc, worker
.else
  calls seg(worker), sof(worker)
.endif
  return
.section .rodata.roots,"a",@progbits
table:
  .long common, override, optional
.comm common, 4, 2
.weak optional

#--- member.s
.include "model.inc"
.section .text.worker,"ax",@progbits
.globl worker
worker:
  mov r4, #7
  return
.comm common, 8, 4
.section .data.weak,"aw",@progbits
.weak override
override:
  .short 0x1111
.section .data.dead,"aw",@progbits
.globl dead
dead:
  .long missing_dead

#--- unused.s
.include "model.inc"
.section .data.unused,"aw",@progbits
.globl optional, unused
optional:
unused:
  .long missing_unselected

#--- strong.s
.include "model.inc"
.section .data.strong,"aw",@progbits
.globl override
override:
  .short 0x2222

#--- layout.ld
ENTRY(_start)
SECTIONS {
  .text TEXT_ADDR : { *(.text.start) *(.text.worker) }
  .rodata DATA_ADDR : { *(.rodata.roots) }
  .data DATA_ADDR + 0x20 : { *(.data.strong) *(.data.weak) }
  .bss DATA_ADDR + 0x40 (NOLOAD) : { BYTE(0) *(COMMON) }
}
