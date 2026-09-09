# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld %t.o -Ttext=0x100 --defsym=target=0x120 --defsym=near_target=0x120 -o %t.near
# RUN: llvm-objdump -s -j .text %t.near | FileCheck %s --check-prefix=NEAR
# RUN: ld.lld %t.o -Ttext=0x100 --defsym=target=0x120000 --defsym=near_target=0x120 -o %t.far
# RUN: llvm-objdump -s -j .text %t.far | FileCheck %s --check-prefix=FAR

## External targets reserve six-byte slots; both conditions must invert when
## expanding to a segmented jump. Local targets stay two bytes.
# NEAR:      0100 4d0fcc00 cc005d0c cc00cc00 4d015dfe
# NEAR-NEXT: 0110 ea402001 ea502001
# FAR:      0100 5d02fa12 00004d02 fa120000 4d015dfe
# FAR-NEXT: 0110 ea402001 ea502001
.globl _start
_start:
jmpr cc_v, target
jmpr cc_nv, target
.Lback:
jmpr cc_v, .Lforward
jmpr cc_nv, .Lback
.Lforward:
jmpa cc_v, near_target
jmpa cc_nv, near_target
