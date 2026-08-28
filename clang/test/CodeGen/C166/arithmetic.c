// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d -r %t.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o0.o

int mul_int(int a, int b) { return a * b; }
int div_int(int a, int b) { return a / b; }
unsigned int div_uint(unsigned int a, unsigned int b) { return a / b; }
int rem_int(int a, int b) { return a % b; }
unsigned int rem_uint(unsigned int a, unsigned int b) { return a % b; }

long mul_long(long a, long b) { return a * b; }
long div_long(long a, long b) { return a / b; }
unsigned long div_ulong(unsigned long a, unsigned long b) { return a / b; }
long rem_long(long a, long b) { return a % b; }
unsigned long rem_ulong(unsigned long a, unsigned long b) { return a % b; }

long shl_long(long value, unsigned int amount) { return value << amount; }
long ashr_long(long value, unsigned int amount) { return value >> amount; }
unsigned long lshr_long(unsigned long value, unsigned int amount) {
  return value >> amount;
}

// The multiply/divide unit operates on native 16-bit integers. MDL holds
// the quotient/low product, while MDH holds the remainder.
// CHECK-LABEL: <_mul_int>:
// CHECK:       mul
// CHECK-NEXT:  mov r4, mdl
// CHECK:       rets
// CHECK-LABEL: <_div_int>:
// CHECK:       mov mdl, r12
// CHECK-NEXT:  div r13
// CHECK-NEXT:  mov r4, mdl
// CHECK:       rets
// CHECK-LABEL: <_div_uint>:
// CHECK:       mov mdl, r12
// CHECK-NEXT:  divu r13
// CHECK-NEXT:  mov r4, mdl
// CHECK:       rets
// CHECK-LABEL: <_rem_int>:
// CHECK:       div r13
// CHECK-NEXT:  mov r4, mdh
// CHECK:       rets
// CHECK-LABEL: <_rem_uint>:
// CHECK:       divu r13
// CHECK-NEXT:  mov r4, mdh
// CHECK:       rets

// The C166 runtime uses the public C register convention for
// 32-bit helpers.  C symbol decoration adds one ABI underscore to each helper
// spelling, hence the three underscores in ELF.
// CHECK-LABEL: <_mul_long>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___mulsi3
// CHECK:       rets
// CHECK-LABEL: <_div_long>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___divsi3
// CHECK:       rets
// CHECK-LABEL: <_div_ulong>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___udivsi3
// CHECK:       rets
// CHECK-LABEL: <_rem_long>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___modsi3
// CHECK:       rets
// CHECK-LABEL: <_rem_ulong>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___umodsi3
// CHECK:       rets
// CHECK-LABEL: <_shl_long>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___ashlsi3
// CHECK:       rets
// CHECK-LABEL: <_ashr_long>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___ashrsi3
// CHECK:       rets
// CHECK-LABEL: <_lshr_long>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___lshrsi3
// CHECK:       rets
