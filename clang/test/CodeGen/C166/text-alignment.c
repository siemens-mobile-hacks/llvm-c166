// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -c %s -o %t.o
// RUN: llvm-readobj --sections %t.o | FileCheck %s

void ordinary_function(void) {}

// CHECK:      Name: .text
// CHECK:      AddressAlignment: 2
