# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=value=254 --defsym=address=0x4001 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=value=255 --defsym=address=0x4001 -o /dev/null 2>&1 | FileCheck %s --check-prefix=IMMERR
# RUN: not ld.lld %t.o --defsym=value=254 --defsym=address=65534 -o /dev/null 2>&1 | FileCheck %s --check-prefix=MEMERR
# RELOC: R_C166_8 value 0x1
# RELOC-COUNT-3: R_C166_16 address 0x2
# BYTES: 10000 e707ff00 f3ff0340 f3070340 f7070340
# IMMERR: relocation R_C166_8 out of range
# MEMERR: relocation R_C166_16 out of range
.globl _start
_start:
movb mdl, #value+1
movb rh7, address+2
movb mdl, address+2
movb address+2, mdl
