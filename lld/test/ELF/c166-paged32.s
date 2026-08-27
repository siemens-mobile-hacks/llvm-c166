# REQUIRES: c166
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
# RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o --section-start=.data=0x400000 \
# RUN:   --section-start=.target=0x500074 -o %t
# RUN: llvm-objdump -s --section=.data %t | FileCheck %s --check-prefix=DATA

## A far-data pointer stores the 14-bit page offset in its low word
## and the 10-bit page number in its high word.  Keep ordinary 32-bit absolute
## data distinct: 0x500074 is 0x0074:0x0140 as a paged pointer, but remains
## 0x00500074 as a linear integer.
# RELOC: R_C166_PAGED32 target
# RELOC: R_C166_32 target
# DATA:      Contents of section .data:
# DATA-NEXT:  400000 74004001 74005000

# RUN: not ld.lld %t.o --defsym=target=-1 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=LOW
# LOW: relocation R_C166_PAGED32 out of range: {{.*}} is not in [0, 16777215]

# RUN: not ld.lld %t.o --defsym=target=0x1000000 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=HIGH
# HIGH: relocation R_C166_PAGED32 out of range: 16777216 is not in [0, 16777215]

.data
.globl _start
_start:
  .long paged(target)
  .long target

.section .target,"a",@progbits
.globl target
target:
  .byte 0
