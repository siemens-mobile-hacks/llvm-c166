# REQUIRES: c166
# RUN: split-file %s %t
# RUN: llvm-mc -triple=c166 -filetype=obj %t/uc.s -o %t/uc.o
# RUN: llvm-mc -triple=c166 -filetype=obj %t/cond.s -o %t/cond.o
# RUN: llvm-mc -triple=c166 -filetype=obj %t/bit.s -o %t/bit.o
# RUN: ld.lld %t/uc.o -Ttext=0xfffc --defsym=target=0x120000 -o %t/uc
# RUN: ld.lld %t/cond.o -Ttext=0xfffa --defsym=target=0x120000 -o %t/cond
# RUN: ld.lld %t/bit.o -Ttext=0xfff8 --defsym=target=0x120000 -o %t/bit
# RUN: not ld.lld %t/uc.o -Ttext=0xfffe --defsym=target=0x120000 -o /dev/null 2>&1 | FileCheck %s
# RUN: not ld.lld %t/cond.o -Ttext=0xfffc --defsym=target=0x120000 -o /dev/null 2>&1 | FileCheck %s
# RUN: not ld.lld %t/bit.o -Ttext=0xfffa --defsym=target=0x120000 -o /dev/null 2>&1 | FileCheck %s

## A replacement sequence must fit in the current segment: IP does not carry
## into CSP when fetching its next instruction or extension word.
# CHECK: relative branch replacement crosses a 64 KiB code boundary

#--- uc.s
.globl _start
_start:
jmpr cc_uc, target
#--- cond.s
.globl _start
_start:
jmpr cc_eq, target
#--- bit.s
.globl _start
_start:
jb psw.3, target
