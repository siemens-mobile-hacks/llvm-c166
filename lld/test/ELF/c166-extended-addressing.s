# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld %t.o --section-start=.text=0 --defsym=object=0x123456 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s

# CHECK: 0000 d7d04800 d7101200 d7a01200
.globl _start
_start:
extpr #pag(object), #2
exts #seg(object), #2
extsr #seg(object), #3
