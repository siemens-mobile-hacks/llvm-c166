# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=address=0x4001 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=address=65534 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC-COUNT-4: R_C166_16 address 0x2
# BYTES: 10000 84020340 94030340 a40e0340 b40f0340
# ERROR: relocation R_C166_16 out of range
.globl _start
_start:
mov [r2], address+2
mov address+2, [r3]
movb [r14], address+2
movb address+2, [r15]
