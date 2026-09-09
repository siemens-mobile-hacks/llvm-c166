# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=value=254 --defsym=address=0x4001 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=value=255 --defsym=address=0x4001 -o /dev/null 2>&1 | FileCheck %s --check-prefix=IMMERR
# RUN: not ld.lld %t.o --defsym=value=254 --defsym=address=65534 -o /dev/null 2>&1 | FileCheck %s --check-prefix=MEMERR
# RELOC-COUNT-8: R_C166_8 value 0x1
# RELOC-COUNT-15: R_C166_16 address 0x2
# BYTES:      10000 0707ff00 1707ff00 2707ff00 3707ff00
# BYTES-NEXT: 10010 4707ff00 5707ff00 6707ff00 7707ff00
# BYTES-NEXT: 10020 03070340 13070340 23070340 33070340
# BYTES-NEXT: 10030 43070340 53070340 63070340 73070340
# BYTES-NEXT: 10040 05070340 15070340 25070340 35070340
# BYTES-NEXT: 10050 55070340 65070340 75070340
# IMMERR: relocation R_C166_8 out of range
# MEMERR: relocation R_C166_16 out of range
.globl _start
_start:
.irp op, addb, addcb, subb, subcb, cmpb, xorb, andb, orb
  \op mdl, #value+1
.endr
.irp op, addb, addcb, subb, subcb, cmpb, xorb, andb, orb
  \op mdl, address+2
.endr
.irp op, addb, addcb, subb, subcb, xorb, andb, orb
  \op address+2, mdl
.endr
