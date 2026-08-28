// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-max.o
// RUN: not %clang --target=c166-none-elf -mcmodel=large -O0 -DOVERFLOW -c %s -o %t-overflow.o 2>&1 | FileCheck %s

// A frame may exactly fill the 16 KiB DPP1 user
// stack page and diagnoses any larger automatic allocation.
struct stack_limit {
#ifdef OVERFLOW
  unsigned char padding[16384];
#else
  unsigned char padding[16382];
#endif
  unsigned int value;
};

unsigned int stack_limit(void) {
  volatile struct stack_limit frame;
  frame.value = 0x1234;
  return frame.value;
}

// CHECK: error: C166 automatic data exceeds the 16K user stack
// CHECK-NEXT: 1 error generated.
