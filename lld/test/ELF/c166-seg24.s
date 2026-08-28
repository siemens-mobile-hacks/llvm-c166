# REQUIRES: c166
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
# RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld -r %t.o -o %t.r
# RUN: llvm-readobj --relocations %t.r | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o --section-start=.text=0x200 --defsym=zero=0 \
# RUN:   --defsym=value=0x123450 --defsym=max24=0xffffff -o %t
# RUN: llvm-readelf -x .text %t | FileCheck %s --check-prefix=DATA
# RUN: not ld.lld %t.o --defsym=zero=-1 --defsym=value=0x123450 \
# RUN:   --defsym=max24=0xffffff -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=LOW
# RUN: not ld.lld %t.o --defsym=zero=0 --defsym=value=0x123450 \
# RUN:   --defsym=max24=0x1000000 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=HIGH

# RELOC:      R_C166_SEG24 zero 0x0
# RELOC-NEXT: R_C166_SEG24 value 0x6
# RELOC-NEXT: R_C166_SEG24 max24 0x0

# DATA: 0x00000200 00000012 5634ffff ff

# LOW: relocation R_C166_SEG24 out of range: {{.*}} is not in [0, 16777215]; references 'zero'
# HIGH: relocation R_C166_SEG24 out of range: 16777216 is not in [0, 16777215]; references 'max24'

.text
.globl _start
_start:
zero_site:
  .space 3
  .reloc zero_site, R_C166_SEG24, zero
value_site:
  .space 3
  .reloc value_site, R_C166_SEG24, value + 6
max_site:
  .space 3
  .reloc max_site, R_C166_SEG24, max24
