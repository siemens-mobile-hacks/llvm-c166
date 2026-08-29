// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -Os -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -stress-regalloc=1 -mllvm -verify-machineinstrs -c %s -o %t-stress.o

extern void clobber(void);

unsigned long sink_stack_argument(unsigned int a, unsigned int b,
                                  unsigned int c, unsigned int d,
                                  unsigned long value) {
  if (a)
    clobber();
  return value;
}

// The immutable argument is loaded in its sole use block, after the optional
// call.  Keeping it live from entry would require an unnecessary save/reload.
// CHECK-LABEL: <_sink_stack_argument>:
// CHECK:       calls
// CHECK:       mov r4, [r0]
// CHECK-NEXT:  mov r5, [r0 + #2]
// CHECK-NEXT:  rets
