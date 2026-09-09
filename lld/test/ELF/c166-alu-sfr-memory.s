# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=data=0x4000 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=data=0xffff -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC-COUNT-15: R_C166_16 data 0x2
# BYTES:      10000 02070240 12070240 22070240 32070240
# BYTES-NEXT: 10010 42070240 52070240 62070240 72070240
# BYTES-NEXT: 10020 04060240 14060240 24060240 34060240
# BYTES-NEXT: 10030 54060240 64060240 74060240
# ERROR: relocation R_C166_16 out of range
.globl _start
_start:
.irp op, add, addc, sub, subc, cmp, xor, and, or
  \op mdl, data+2
.endr
.irp op, add, addc, sub, subc, xor, and, or
  \op data+2, mdh
.endr
