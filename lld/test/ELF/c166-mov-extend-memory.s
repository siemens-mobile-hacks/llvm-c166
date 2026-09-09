# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=address=0x4000 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=address=65536 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC-COUNT-6: R_C166_16 address
# BYTES:      10000 d2070340 c2060340 d5ff0440 c5f00440
# BYTES-NEXT: 10010 d5070440 c5060440
# ERROR: relocation R_C166_16 out of range
.globl _start
_start:
movbs mdl, address+3
movbz mdh, address+3
movbs address+4, rh7
movbz address+4, rl0
movbs address+4, mdl
movbz address+4, mdh
