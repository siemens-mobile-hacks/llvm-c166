# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld %t.o --section-start=.text=0 --defsym=value=255 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=HIGH
# RUN: ld.lld %t.o --section-start=.text=0 --defsym=value=-128 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=LOW
# RUN: not ld.lld %t.o --defsym=value=256 -o /dev/null 2>&1 | FileCheck %s --check-prefix=OVERFLOW
# RUN: not ld.lld %t.o --defsym=value=-129 -o /dev/null 2>&1 | FileCheck %s --check-prefix=OVERFLOW

# HIGH: 0000 e7f0ff00 07ffff00 47f4ff00
# LOW:  0000 e7f08000 07ff8000 47f48000
# OVERFLOW: relocation R_C166_8 out of range

.globl _start
_start:
movb rl0, #value
addb rh7, #value
cmpb rl2, #value
