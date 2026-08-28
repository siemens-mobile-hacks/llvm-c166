// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: llvm-objdump -d %t-o0.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t-o2.o

typedef unsigned long (*c166_function)(void);

unsigned int function_pointer_equal(c166_function lhs, c166_function rhs) {
  return lhs == rhs;
}

unsigned long function_pointer_mask(c166_function lhs, c166_function rhs) {
  return lhs == rhs ? ~0UL : 0UL;
}

// CHECK-LABEL: <_function_pointer_equal>:
// CHECK:       cmp
// CHECK:       rets

// CHECK-LABEL: <_function_pointer_mask>:
// CHECK:       cmp
// CHECK:       rets
