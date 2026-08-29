// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: llvm-objdump -d %t.small.o | FileCheck %s --check-prefix=SMALL
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t-o1.o
// RUN: llvm-objdump -d %t-o1.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t-o2.o
// C166-ABI: registers.callee_saved_r6_r9

extern volatile unsigned int pressure_word_seed;
extern volatile unsigned long pressure_long_seed;
extern unsigned int pressure_barrier(void);
extern void pressure_observe4(char *, char *, char *, char *);
extern void pressure_void_barrier(void);
extern unsigned int pressure_consume4(char *, char *, char *, char *);

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

unsigned int pressure_frame_addresses(void) {
  char a[2];
  char b[2];
  char c[2];
  char d[2];
  pressure_observe4(a, b, c, d);
  pressure_void_barrier();
  return pressure_consume4(a, b, c, d);
}

// Large frames use one full-immediate adjustment. Matching adjustments in the
// epilogue protect the user-stack delta on every return path.
// CHECK-LABEL: <_pressure_words>:
// CHECK-DAG:   mov [-r0], r6
// CHECK-DAG:   mov [-r0], r7
// CHECK-DAG:   mov [-r0], r8
// CHECK-DAG:   mov [-r0], r9
// CHECK:       sub r0, #16
// CHECK:       calls
// CHECK:       add r0, #16
// CHECK-DAG:   mov r9, [r0+]
// CHECK-DAG:   mov r8, [r0+]
// CHECK-DAG:   mov r7, [r0+]
// CHECK-DAG:   mov r6, [r0+]
// CHECK:       rets

// Near frame addresses are cheap to recompute after calls. Keeping their live
// ranges local avoids introducing callee-saved registers in the Small model.
// SMALL-LABEL: <_pressure_frame_addresses>:
// SMALL-NOT:   mov [-r0]
// SMALL:       sub r0, #8
// SMALL:       calls
// SMALL-NEXT:  calls
// SMALL:       mov r12, r0
// SMALL:       calls
// SMALL-NOT:   mov [-r0]
// SMALL:       rets

// CHECK-LABEL: <_pressure_longs>:
// CHECK-DAG:   mov [-r0], r6
// CHECK-DAG:   mov [-r0], r7
// CHECK-DAG:   mov [-r0], r8
// CHECK-DAG:   mov [-r0], r9
// CHECK:       sub r0, #24
// CHECK:       calls
// CHECK:       add r0, #24
// CHECK-DAG:   mov r9, [r0+]
// CHECK-DAG:   mov r8, [r0+]
// CHECK-DAG:   mov r7, [r0+]
// CHECK-DAG:   mov r6, [r0+]
// CHECK:       rets

// Far frame addresses are formed at each call instead of occupying
// callee-saved pairs across calls. Stack arguments are pushed as they become
// available, and subsequent frame-index offsets account for each push.
// CHECK-LABEL: <_pressure_frame_addresses>:
// CHECK:       sub r0, #8
// CHECK-NEXT:  mov r1, r0
// CHECK:       mov [-r0], r2
// CHECK-NEXT:  mov [-r0], r1
// CHECK-NEXT:  mov r1, r0
// CHECK-NEXT:  add r1, #6
// CHECK:       mov [-r0], r2
// CHECK-NEXT:  mov [-r0], r1
// CHECK:       mov r12, #14
// CHECK-NEXT:  add r12, r0
// CHECK:       mov r14, #12
// CHECK-NEXT:  add r14, r0
// CHECK:       calls
// CHECK-NEXT:  add r0, #8
// CHECK-NEXT:  calls
// CHECK-NOT:   mov [-r0], r6
// CHECK-NOT:   mov [-r0], r7
// CHECK-NOT:   mov [-r0], r8
// CHECK-NOT:   mov [-r0], r9
// CHECK:       mov [-r0], r2
// CHECK-NEXT:  mov [-r0], r1
// CHECK:       calls
// CHECK:       rets
