// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: llvm-nm -u %t.small.o | FileCheck %s --check-prefix=LIBCALL
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t.O0.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t.O2.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -Oz -mllvm -verify-machineinstrs -c %s -o %t.Oz.o

typedef unsigned int size_type;

void copy_bytes(void *destination, const void *source, size_type size) {
  __builtin_memcpy(destination, source, size);
}

void move_bytes(void *destination, const void *source, size_type size) {
  __builtin_memmove(destination, source, size);
}

void clear_bytes(void *destination, size_type size) {
  __builtin_memset(destination, 0, size);
}

// LIBCALL-DAG: U _memcpy
// LIBCALL-DAG: U _memmove
// LIBCALL-DAG: U _memset
