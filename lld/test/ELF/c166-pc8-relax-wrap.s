# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld %t.o --section-start=.text=0xffe0 --defsym=target=0 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=WRAP
# RUN: ld.lld %t.o --section-start=.text=0xffe0 --defsym=target=0x10000 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=FAR

## Same-segment IP wrap preserves short forms and their padding.
# WRAP:      ffe0 0d0fcc00 2d0dcc00 cc008a88 0930cc00
# WRAP-NEXT: fff0 cc00
## Equally close in linear address space, but in the next code segment.
# FAR:      ffe0 fa010000 3d02fa01 00009a88 0230fa01
# FAR-NEXT: fff0 0000

.globl _start
_start:
jmpr cc_uc, target
jmpr cc_eq, target
jb psw.3, target
