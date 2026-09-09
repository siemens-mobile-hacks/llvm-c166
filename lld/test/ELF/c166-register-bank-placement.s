# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld %t.o --section-start=.c166.regbank=0xf200 -o %t.low
# RUN: ld.lld %t.o --section-start=.c166.regbank=0xfde0 -o %t.high
# RUN: not ld.lld %t.o --section-start=.c166.regbank=0x5800 -o /dev/null 2>&1 | FileCheck %s
# RUN: not ld.lld %t.o --section-start=.c166.regbank=0xf000 -o /dev/null 2>&1 | FileCheck %s
# RUN: not ld.lld %t.o --section-start=.c166.regbank=0xfde2 -o /dev/null 2>&1 | FileCheck %s
# RUN: not ld.lld %t.o --section-start=.c166.regbank=0xf201 -o /dev/null 2>&1 | FileCheck %s
# CHECK: C166 register bank section must be word-aligned within [0xF200, 0xFE00)

.globl _start
_start:
  nop
.section .c166.regbank,"aw",@nobits
.zero 32
