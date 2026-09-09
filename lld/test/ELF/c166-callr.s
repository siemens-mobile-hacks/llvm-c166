# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld %t.o --section-start=.text=0 --defsym=callee=2 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=ZERO
# RUN: ld.lld %t.o --section-start=.text=0x100 --defsym=callee=2 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=MIN
# RUN: ld.lld %t.o --section-start=.text=0 --defsym=callee=0x100 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=MAX
# RUN: ld.lld %t.o --section-start=.text=0x12fffe --defsym=callee=0x120000 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=WRAP-FORWARD
# RUN: ld.lld %t.o --section-start=.text=0x120000 --defsym=callee=0x12fffe -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=WRAP-BACKWARD
# RUN: not ld.lld %t.o --section-start=.text=0 --defsym=callee=0x102 -o /dev/null 2>&1 | FileCheck %s --check-prefix=RANGE
# RUN: not ld.lld %t.o --section-start=.text=0 --defsym=callee=3 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ALIGN
# RUN: not ld.lld %t.o --section-start=.text=0xfffe --defsym=callee=0x10000 -o /dev/null 2>&1 | FileCheck %s --check-prefix=SEGMENT

# ZERO: 0000 bb00
# MIN: 0100 bb80
# MAX: 0000 bb7f
# WRAP-FORWARD: 12fffe bb00
# WRAP-BACKWARD: 120000 bbfe
# RANGE: relocation R_C166_PC8 out of range
# ALIGN: R_C166_PC8 target is not word-aligned
# SEGMENT: relative code relocation crosses a 64 KiB code boundary

## CALLR must not relax to CALLS, which saves a different return address.
.globl _start
_start:
callr callee
