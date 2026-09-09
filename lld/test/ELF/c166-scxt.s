# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=value=0x7ffe --defsym=address=0xfffd -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=value=65534 --defsym=address=0 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RUN: not ld.lld %t.o --defsym=value=0 --defsym=address=65534 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR

# RELOC: 0x2 R_C166_16 value 0x2
# RELOC: 0x6 R_C166_16 value 0x2
# RELOC: 0xA R_C166_16 address 0x2
# RELOC: 0xE R_C166_16 address 0x2
# BYTES: 10000 c6f00080 c6080080 d6ffffff d607ffff
# ERROR: relocation R_C166_16 out of range

.globl _start
_start:
scxt r0, #value+2
scxt cp, #value+2
scxt r15, address+2
scxt mdl, address+2
