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
# RELOC: 0x1E R_C166_16 data 0x0
# RELOC: 0x22 R_C166_16 data 0xFFFFFFFE
# BYTES:      10000 02f40240 12f40040 22f40040 32f40040
# BYTES-NEXT: 10010 42f40040 52f40040 62f40040 72f40040
# BYTES-NEXT: 10020 02f4fe3f
# ERROR: relocation R_C166_16 out of range
.globl _start
_start:
add r4, data+2
addc r4, data
sub r4, data
subc r4, data
cmp r4, data
xor r4, data
and r4, data
or r4, data
add r4, data-2
