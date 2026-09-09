# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=data=0x4000 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=data=0x10000 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC: 0x2 R_C166_16 data 0x2
# RELOC: 0x6 R_C166_16 data 0x4
# BYTES: 10000 f2870240 f6870440
# ERROR: relocation R_C166_16 out of range
.globl _start
_start:
mov mdc, data+2
mov data+4, mdc
