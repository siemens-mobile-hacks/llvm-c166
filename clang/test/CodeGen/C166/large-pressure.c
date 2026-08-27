// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t-o1.o
// RUN: llvm-objdump -d %t-o1.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t-o2.o
// C166-ABI: registers.callee_saved_r6_r9

extern volatile unsigned int pressure_word_seed;
extern volatile unsigned long pressure_long_seed;
extern unsigned int pressure_barrier(void);

unsigned int pressure_words(unsigned int a0, unsigned int a1,
                            unsigned int a2, unsigned int a3,
                            unsigned int a4, unsigned int a5,
                            unsigned int a6, unsigned int a7) {
  unsigned int v0 = a0 + pressure_word_seed;
  unsigned int v1 = a1 + pressure_word_seed;
  unsigned int v2 = a2 + pressure_word_seed;
  unsigned int v3 = a3 + pressure_word_seed;
  unsigned int v4 = a4 + pressure_word_seed;
  unsigned int v5 = a5 + pressure_word_seed;
  unsigned int v6 = a6 + pressure_word_seed;
  unsigned int v7 = a7 + pressure_word_seed;

  switch (pressure_barrier() & 7) {
  case 0: return v0;
  case 1: return v1;
  case 2: return v2;
  case 3: return v3;
  case 4: return v4;
  case 5: return v5;
  case 6: return v6;
  default: return v7;
  }
}

unsigned long pressure_longs(unsigned long a0, unsigned long a1,
                             unsigned long a2, unsigned long a3,
                             unsigned long a4, unsigned long a5) {
  unsigned long v0 = a0 + pressure_long_seed;
  unsigned long v1 = a1 + pressure_long_seed;
  unsigned long v2 = a2 + pressure_long_seed;
  unsigned long v3 = a3 + pressure_long_seed;
  unsigned long v4 = a4 + pressure_long_seed;
  unsigned long v5 = a5 + pressure_long_seed;

  switch (pressure_barrier() & 7) {
  case 0: return v0;
  case 1: return v1;
  case 2: return v2;
  case 3: return v3;
  case 4: return v4;
  default: return v5;
  }
}

// Large frames use one scratch-register adjustment rather than a linear chain
// of compact six-byte increments.  Matching adjustments in the epilogue
// protect the user-stack delta on every return path.
// CHECK-LABEL: <_pressure_words>:
// CHECK:       mov r1, #24
// CHECK-NEXT:  sub r0, r1
// CHECK-DAG:   mov [r0 + #{{[0-9]+}}], r6
// CHECK-DAG:   mov [r0 + #{{[0-9]+}}], r7
// CHECK-DAG:   mov [r0 + #{{[0-9]+}}], r8
// CHECK-DAG:   mov [r0 + #{{[0-9]+}}], r9
// CHECK:       calls
// CHECK-DAG:   mov r9, [r0 + #{{[0-9]+}}]
// CHECK-DAG:   mov r8, [r0 + #{{[0-9]+}}]
// CHECK-DAG:   mov r7, [r0 + #{{[0-9]+}}]
// CHECK-DAG:   mov r6, [r0 + #{{[0-9]+}}]
// CHECK:       mov r1, #24
// CHECK-NEXT:  add r0, r1
// CHECK:       rets

// CHECK-LABEL: <_pressure_longs>:
// CHECK:       mov r1, #48
// CHECK-NEXT:  sub r0, r1
// CHECK-DAG:   mov [r0 + #{{[0-9]+}}], r6
// CHECK-DAG:   mov [r0 + #{{[0-9]+}}], r7
// CHECK-DAG:   mov [r0 + #{{[0-9]+}}], r8
// CHECK-DAG:   mov [r0 + #{{[0-9]+}}], r9
// CHECK:       calls
// CHECK-DAG:   mov r9, [r0 + #{{[0-9]+}}]
// CHECK-DAG:   mov r8, [r0 + #{{[0-9]+}}]
// CHECK-DAG:   mov r7, [r0 + #{{[0-9]+}}]
// CHECK-DAG:   mov r6, [r0 + #{{[0-9]+}}]
// CHECK:       mov r1, #48
// CHECK-NEXT:  add r0, r1
// CHECK:       rets
