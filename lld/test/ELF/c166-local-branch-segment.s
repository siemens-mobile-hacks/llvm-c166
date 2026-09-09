# REQUIRES: c166
# RUN: split-file %s %t
# RUN: llvm-mc -triple=c166 -filetype=obj %t/jmpr.s -o %t/jmpr.o
# RUN: llvm-mc -triple=c166 -filetype=obj %t/bit.s -o %t/bit.o
# RUN: llvm-readobj -r %t/jmpr.o | FileCheck %s --check-prefix=JMPR-RELOC
# RUN: llvm-readobj -r %t/bit.o | FileCheck %s --check-prefix=BIT-RELOC
# RUN: ld.lld %t/jmpr.o -Ttext=0x100 -o %t/jmpr
# RUN: ld.lld %t/bit.o -Ttext=0x100 -o %t/bit
# RUN: llvm-objdump -s -j .text %t/jmpr | FileCheck %s --check-prefix=JMPR
# RUN: llvm-objdump -s -j .text %t/bit | FileCheck %s --check-prefix=BIT
# RUN: not ld.lld %t/jmpr.o -Ttext=0xfffe -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RUN: not ld.lld %t/bit.o -Ttext=0xfffc -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RUN: llvm-mc -triple=c166 -filetype=obj %t/bounds.s -o %t/bounds.o
# RUN: ld.lld %t/bounds.o -Ttext=0x100 -o %t/bounds
# RUN: llvm-objdump -d %t/bounds | FileCheck %s --check-prefix=BOUNDS

## Local branches keep their original size, but not a layout-dependent delta.
# JMPR-RELOC: 0x1 R_C166_PC8 .text 0x2
# BIT-RELOC: 0x2 R_C166_BIT_PC8 .text 0x4
# JMPR: 0100 0d00cc00
# BIT: 0100 8a880030 cc00
# ERROR: relative code relocation crosses a 64 KiB code boundary
# BOUNDS: 1fe: 3d 80{{.*}}jmpr cc_ne, 128
# BOUNDS: 200: 2d 7f{{.*}}jmpr cc_eq, 127

#--- jmpr.s
.globl _start
_start:
jmpr cc_uc, .Ltarget
.Ltarget:
nop
#--- bit.s
.globl _start
_start:
jb psw.3, .Ltarget
.Ltarget:
nop
#--- bounds.s
.globl _start
_start:
.Lback:
nop
.space 252
jmpr cc_ne, .Lback
jmpr cc_eq, .Lforward
.space 254
.Lforward:
nop
