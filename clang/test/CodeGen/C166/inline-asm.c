// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t.o

unsigned register_constraint(unsigned value) {
  unsigned result;
  __asm__ volatile("mov %0, %1" : "=r"(result) : "r"(value));
  return result;
}

unsigned immediate_constraint(void) {
  unsigned result;
  __asm__ volatile("mov %0, #%1" : "=r"(result) : "I"(7));
  return result;
}

// CHECK-LABEL: _register_constraint:
// CHECK: mov r{{[0-9]+}}, r{{[0-9]+}}
// CHECK-LABEL: _immediate_constraint:
// CHECK: mov r{{[0-9]+}}, #7
