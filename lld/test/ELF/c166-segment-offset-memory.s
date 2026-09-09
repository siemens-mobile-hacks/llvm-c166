# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: ld.lld -Ttext=0x1000 --defsym=object=0x12fedc %t.o -o %t
# RUN: llvm-objdump -s %t | FileCheck %s
# RUN: not ld.lld -Ttext=0x1000 --defsym=object=0xfffff8 %t.o -o %t.bad 2>&1 | FileCheck %s --check-prefix=OVERFLOW

.globl _start
_start:
  exts #seg(object), #2
  mov r4, sof(object+2)
  mov sof(object+4), r4
  movb rl2, sof(object+1)
  movb sof(object+3), rl2
  add r4, sof(object+6)
  add sof(object+8), r4

# CHECK: Contents of section .text:
# CHECK-NEXT: 1000 d7101200 f2f4defe f6f4e0fe f3f4ddfe
# CHECK-NEXT: 1010 f7f4dffe 02f4e2fe 04f4e4fe

# The addend must be included in the 24-bit range check before extracting SOF.
# OVERFLOW: relocation R_C166_SOF16 out of range: 16777216 is not in [0, 16777215]; references 'object'
