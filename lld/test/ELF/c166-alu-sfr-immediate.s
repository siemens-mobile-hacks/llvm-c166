# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=value=0x7ffe -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: ld.lld %t.o -Ttext=0x10000 --defsym=value=-32770 -o %t.negative
# RUN: llvm-objdump -s -j .text %t.negative | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o --defsym=value=65534 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC-COUNT-8: R_C166_16 value 0x2
# BYTES:      10000 06070080 16070080 26070080 36070080
# BYTES-NEXT: 10010 46070080 56070080 66070080 76070080
# ERROR: relocation R_C166_16 out of range
.globl _start
_start:
.irp mnemonic, add, addc, sub, subc, cmp, xor, and, or
  \mnemonic mdl, #value+2
.endr
