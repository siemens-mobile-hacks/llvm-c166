# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=data=0x4000 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=data=0xffff -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC-COUNT-15: R_C166_16 data 0x1
# BYTES:      10000 03f50140 13f50140 23f50140 33f50140
# BYTES-NEXT: 10010 43f50140 53f50140 63f50140 73f50140
# BYTES-NEXT: 10020 05f50140 15f50140 25f50140 35f50140
# BYTES-NEXT: 10030 55f50140 65f50140 75f50140
# ERROR: relocation R_C166_16 out of range
.globl _start
_start:
.irp op, addb, addcb, subb, subcb, cmpb, xorb, andb, orb
  \op rh2, data+1
.endr
.irp op, addb, addcb, subb, subcb, xorb, andb, orb
  \op data+1, rh2
.endr
