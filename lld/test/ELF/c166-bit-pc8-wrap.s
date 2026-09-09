# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld %t.o --section-start=.text=0x12fffc --defsym=target=0x120000 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=FORWARD
# RUN: ld.lld %t.o --section-start=.text=0x120000 --defsym=target=0x12fffe -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=BACKWARD
# RUN: not ld.lld %t.o --section-start=.text=0x12fffc --defsym=target=0x130000 -o /dev/null 2>&1 | FileCheck %s --check-prefix=SEGMENT

# FORWARD: 12fffc 8a880030
# BACKWARD: 120000 8a88fd30
# SEGMENT: relative code relocation crosses a 64 KiB code boundary

## Explicit PC8 relocation, without a relaxation slot. JB psw.3,target.
.globl _start
_start:
.reloc .+2, R_C166_BIT_PC8, target
.byte 0x8a, 0x88, 0, 0x30
