# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=value=8 --defsym=address=0x4001 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=value=65535 --defsym=address=0x4001 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RUN: not ld.lld %t.o --defsym=value=8 --defsym=address=65534 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC-COUNT-4: R_C166_16 value 0x1
# RELOC-COUNT-4: R_C166_16 address 0x2
# BYTES:      10000 a6f40900 b6f40900 86f40900 96f40900
# BYTES-NEXT: 10010 a2f40340 b2f40340 82f40340 92f40340
# ERROR: relocation R_C166_16 out of range
.globl _start
_start:
.irp op, cmpd1, cmpd2, cmpi1, cmpi2
  \op r4, #value+1
.endr
.irp op, cmpd1, cmpd2, cmpi1, cmpi2
  \op r4, address+2
.endr
