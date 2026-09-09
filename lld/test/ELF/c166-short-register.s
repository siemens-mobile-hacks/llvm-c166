# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=address=0x4000 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=address=65535 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC-COUNT-4: R_C166_16 address
# BYTES: 10000 f2bd0240 f6bd0440 02bd0240 c6bd0640
# ERROR: relocation R_C166_16 out of range
.globl _start
_start:
mov sfr(0xbd), address+2
mov address+4, sfr(0xbd)
add sfr(0xbd), address+2
scxt sfr(0xbd), #address+6
