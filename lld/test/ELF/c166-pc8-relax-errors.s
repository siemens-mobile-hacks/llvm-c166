# REQUIRES: c166
# RUN: split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/malformed.s -o %t/malformed.o
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/odd.s -o %t/odd.o
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/overflow.s -o %t/overflow.o
# RUN: not ld.lld -Ttext=0 -e _start --defsym=target=0 %t/malformed.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=MALFORMED
# RUN: not ld.lld -Ttext=0 -e _start --defsym=target=3 %t/odd.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=ODD
# RUN: not ld.lld -Ttext=0 -e _start --defsym=target=0x1000000 %t/overflow.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=OVERFLOW

# MALFORMED: error: {{.*}}R_C166_PC8_RELAX does not refer to a relative branch
# ODD: error: {{.*}}R_C166_PC8_RELAX target is not word-aligned
# OVERFLOW: error: {{.*}}relocation R_C166_PC8_RELAX out of range: 16777216 is not in [0, 16777215]; references 'target'

#--- malformed.s
.text
.globl _start
_start:
  .byte 0xcc, 0x00, 0xcc, 0x00, 0xcc, 0x00
  .reloc _start, R_C166_PC8_RELAX, target

#--- odd.s
.text
.globl _start
_start:
  .byte 0x2d, 0x00, 0xcc, 0x00, 0xcc, 0x00
  .reloc _start, R_C166_PC8_RELAX, target

#--- overflow.s
.text
.globl _start
_start:
  .byte 0x0d, 0x00, 0xcc, 0x00
  .reloc _start, R_C166_PC8_RELAX, target
