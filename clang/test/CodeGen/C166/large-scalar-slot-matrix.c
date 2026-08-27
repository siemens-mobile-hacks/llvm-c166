// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// C166-ABI: args.scalar_combinatorial_matrix

typedef unsigned int u16;
typedef unsigned long u32;

u16 scalar_w0(u16 value) { return value; }
u16 scalar_w1(u16 p0, u16 value) { return value; }
u16 scalar_w2(u16 p0, u16 p1, u16 value) { return value; }
u16 scalar_w3(u16 p0, u16 p1, u16 p2, u16 value) { return value; }
u16 scalar_w4(u16 p0, u16 p1, u16 p2, u16 p3, u16 value) { return value; }

u32 scalar_d0(u32 value) { return value; }
u32 scalar_d1(u16 p0, u32 value) { return value; }
u32 scalar_d2(u16 p0, u16 p1, u32 value) { return value; }
u32 scalar_d3(u16 p0, u16 p1, u16 p2, u32 value) { return value; }
u32 scalar_d4(u16 p0, u16 p1, u16 p2, u16 p3, u32 value) { return value; }

u16 scalar_d3_tail(u16 p0, u16 p1, u16 p2, u32 value, u16 tail) {
  return tail;
}

// One-word class at every possible starting slot.
// CHECK-LABEL: <_scalar_w0>:
// CHECK:       mov r4, r12
// CHECK-NEXT:  rets
// CHECK-LABEL: <_scalar_w1>:
// CHECK:       mov r4, r13
// CHECK-NEXT:  rets
// CHECK-LABEL: <_scalar_w2>:
// CHECK:       mov r4, r14
// CHECK-NEXT:  rets
// CHECK-LABEL: <_scalar_w3>:
// CHECK:       mov r4, r15
// CHECK-NEXT:  rets
// CHECK-LABEL: <_scalar_w4>:
// CHECK:       mov r4, [r0]
// CHECK-NEXT:  rets

// Two-word class at every possible remaining register capacity.  It is packed
// at odd starts, but moves wholly to the stack when only R15 remains.
// CHECK-LABEL: <_scalar_d0>:
// CHECK:       mov r4, r12
// CHECK-NEXT:  mov r5, r13
// CHECK-NEXT:  rets
// CHECK-LABEL: <_scalar_d1>:
// CHECK:       mov r4, r13
// CHECK-NEXT:  mov r5, r14
// CHECK-NEXT:  rets
// CHECK-LABEL: <_scalar_d2>:
// CHECK:       mov r4, r14
// CHECK-NEXT:  mov r5, r15
// CHECK-NEXT:  rets
// CHECK-LABEL: <_scalar_d3>:
// CHECK:       mov r4, [r0]
// CHECK-NEXT:  mov r5, [r0 + #2]
// CHECK-NEXT:  rets
// CHECK-LABEL: <_scalar_d4>:
// CHECK:       mov r4, [r0]
// CHECK-NEXT:  mov r5, [r0 + #2]
// CHECK-NEXT:  rets

// Once the two-word value is stack-forced, a following word is at offset 4.
// CHECK-LABEL: <_scalar_d3_tail>:
// CHECK:       mov r4, [r0 + #4]
// CHECK-NEXT:  rets
