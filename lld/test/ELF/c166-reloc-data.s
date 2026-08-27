# REQUIRES: c166
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o

## R_C166_{8,16,32} accept the complete signed-or-unsigned field range.  This
## checks zero, positive and negative addends as well as both representable
## boundaries.
# RUN: ld.lld %t.o --defsym=a8=0 --defsym=b8=128 \
# RUN:   --defsym=a16=0 --defsym=b16=32768 \
# RUN:   --defsym=a32=0 --defsym=b32=0x80000000 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=DATA
# DATA: Contents of section .text:
# DATA-NEXT: {{[0-9a-f]+}} 80ff0080 ffff0000 0080ffff ffff

# RUN: not ld.lld %t.o --defsym=a8=-1 --defsym=b8=128 \
# RUN:   --defsym=a16=0 --defsym=b16=32768 \
# RUN:   --defsym=a32=0 --defsym=b32=0x80000000 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=R8-LOW
# R8-LOW: relocation R_C166_8 out of range: -129 is not in [-128, 255]; references 'a8'

# RUN: not ld.lld %t.o --defsym=a8=0 --defsym=b8=129 \
# RUN:   --defsym=a16=0 --defsym=b16=32768 \
# RUN:   --defsym=a32=0 --defsym=b32=0x80000000 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=R8-HIGH
# R8-HIGH: relocation R_C166_8 out of range: 256 is not in [-128, 255]; references 'b8'

# RUN: not ld.lld %t.o --defsym=a8=0 --defsym=b8=128 \
# RUN:   --defsym=a16=-1 --defsym=b16=32768 \
# RUN:   --defsym=a32=0 --defsym=b32=0x80000000 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=R16-LOW
# R16-LOW: relocation R_C166_16 out of range: -32769 is not in [-32768, 65535]; references 'a16'

# RUN: not ld.lld %t.o --defsym=a8=0 --defsym=b8=128 \
# RUN:   --defsym=a16=0 --defsym=b16=32769 \
# RUN:   --defsym=a32=0 --defsym=b32=0x80000000 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=R16-HIGH
# R16-HIGH: relocation R_C166_16 out of range: 65536 is not in [-32768, 65535]; references 'b16'

.text
.globl _start
_start:
  .byte a8 - 128
  .byte b8 + 127
  .short a16 - 32768
  .short b16 + 32767
  .long a32 - 0x80000000
  .long b32 + 0x7fffffff
