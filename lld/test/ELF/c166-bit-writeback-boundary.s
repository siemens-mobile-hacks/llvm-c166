# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld %t.o -Ttext=0xfff6 --defsym=target=0x20400 -o %t
# RUN: not ld.lld %t.o -Ttext=0xfff8 --defsym=target=0x20400 -o /dev/null 2>&1 | FileCheck %s --check-prefix=BOUNDARY
# RUN: not ld.lld %t.o --defsym=target=0x20401 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ALIGN
# RUN: not ld.lld %t.o --defsym=target=0x1000000 -o /dev/null 2>&1 | FileCheck %s --check-prefix=RANGE
# BOUNDARY: relative branch replacement crosses a 64 KiB code boundary
# ALIGN: R_C166_PC8_RELAX target is not word-aligned
# RANGE: relocation R_C166_PC8_RELAX out of range
.globl _start
_start:
jbc r4.5, target
