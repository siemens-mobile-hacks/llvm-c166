# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=value=0x80 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=value=0x100 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC: 0x2 R_C166_8 value 0x1
# RELOC: 0x6 R_C166_8 value 0x0
# BYTES: 10000 17f48100 37f58000
# ERROR: relocation R_C166_8 out of range
.globl _start
_start:
addcb rl2, #value+1
subcb rh2, #value
