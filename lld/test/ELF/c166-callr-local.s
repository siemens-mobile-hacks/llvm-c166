# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o --section-start=.text=0x100 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --section-start=.text=0xfffe -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR

# RELOC: R_C166_PC8
# BYTES: 0100 bb00cb00
# ERROR: relative code relocation crosses a 64 KiB code boundary
.globl _start
_start:
callr .Lcallee
.Lcallee:
ret
