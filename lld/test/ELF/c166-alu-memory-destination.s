# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=data=0x4000 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o -Ttext=0x10000 --defsym=data=0x10000 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC: 0x2 R_C166_16 data 0x2
# RELOC: 0x6 R_C166_16 data 0x0
# RELOC: 0xA R_C166_16 data 0x0
# RELOC: 0xE R_C166_16 data 0x0
# RELOC: 0x12 R_C166_16 data 0x0
# RELOC: 0x16 R_C166_16 data 0x0
# RELOC: 0x1A R_C166_16 data 0x0
# BYTES:      10000 04f40240 14f40040 24f40040 34f40040
# BYTES-NEXT: 10010 54f40040 64f40040 74f40040
# ERROR: relocation R_C166_16 out of range
.globl _start
_start:
add data+2, r4
addc data, r4
sub data, r4
subc data, r4
xor data, r4
and data, r4
or data, r4
