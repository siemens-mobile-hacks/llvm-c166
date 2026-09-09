# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld %t.o --section-start=.text=0 --defsym=object=0x123456 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s

## All immediate segments occupy byte 2; CALLS/JMPS segments occupy byte 1.
# CHECK:      Contents of section .text:
# CHECK-NEXT: 0000 e6f41200 e6871200 c6f41200 06f41200
# CHECK-NEXT: 0010 da127856 fa127856

.globl _start
_start:
mov r4, #seg(object)
mov mdc, #seg(object)
scxt r4, #seg(object)
add r4, #seg(object)
calls seg(object), 0x5678
jmps seg(object), 0x5678
