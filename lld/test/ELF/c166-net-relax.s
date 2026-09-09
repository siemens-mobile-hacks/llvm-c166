# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld %t.o -Ttext=0x100 --defsym=target=0x200 -o %t.max
# RUN: llvm-objdump -s -j .text %t.max | FileCheck %s --check-prefix=MAX
# RUN: ld.lld %t.o -Ttext=0x100 --defsym=target=2 -o %t.min
# RUN: llvm-objdump -s -j .text %t.min | FileCheck %s --check-prefix=MIN
# RUN: ld.lld %t.o -Ttext=0x100 --defsym=target=0x123456 -o %t.far
# RUN: llvm-objdump -d %t.far | FileCheck %s --check-prefix=FAR
# RUN: ld.lld %t.o -Ttext=0xfff8 --defsym=target=0 -o %t.wrap
# RUN: llvm-objdump -s -j .text %t.wrap | FileCheck %s --check-prefix=WRAP
# RUN: ld.lld %t.o -Ttext=0xfff8 --defsym=target=0x10000 -o %t.cross
# RUN: llvm-objdump -s -j .text %t.cross | FileCheck %s --check-prefix=CROSS
# RUN: not ld.lld %t.o -Ttext=0xfffa --defsym=target=0x123456 -o /dev/null 2>&1 | FileCheck %s --check-prefix=BOUNDARY
# RUN: not ld.lld %t.o -Ttext=0x100 --defsym=target=3 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ALIGN

# MAX: 0100 1d7fcc00 cc00cc00
# MIN: 0100 1d80cc00 cc00cc00
## NET is (Z|E)==0. Testing it once and skipping with UC preserves both flags.
# FAR:      100: 1d 01{{.*}}jmpr cc_net, 1
# FAR-NEXT: 102: 0d 02{{.*}}jmpr cc_uc, 2
# FAR-NEXT: 104: fa 12 56 34{{.*}}jmps 18, 13398
# WRAP: fff8 1d03cc00 cc00cc00
# CROSS: fff8 1d010d02 fa010000
# BOUNDARY: relative branch replacement crosses a 64 KiB code boundary
# ALIGN: R_C166_PC8_RELAX target is not word-aligned
.globl _start
_start:
jmpr cc_net, target
