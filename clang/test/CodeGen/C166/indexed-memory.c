// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t.large.o
// RUN: llvm-objdump -d %t.large.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O2 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O2 -mllvm -verify-machineinstrs -c %s -o %t.small.o

typedef unsigned int u16;
typedef u16 __attribute__((c166_near)) near_u16;
typedef u16 __attribute__((c166_far)) far_u16;
typedef u16 __attribute__((c166_huge)) huge_u16;

u16 sum_near_words(const near_u16 *source, u16 count) {
  u16 sum = 0;
  while (count != 0) {
    sum += *source++;
    --count;
  }
  return sum;
}

u16 sum_far_words(const far_u16 *source, u16 count) {
  u16 sum = 0;
  while (count != 0) {
    sum += *source++;
    --count;
  }
  return sum;
}

u16 sum_huge_words(const huge_u16 *source, u16 count) {
  u16 sum = 0;
  while (count != 0) {
    sum += *source++;
    --count;
  }
  return sum;
}

u16 sum_volatile_far_words(const volatile far_u16 *source, u16 count) {
  u16 sum = 0;
  while (count != 0) {
    sum += *source++;
    --count;
  }
  return sum;
}

u16 sum_volatile_near_words(const volatile near_u16 *source, u16 count) {
  u16 sum = 0;
  while (count != 0) {
    sum += *source++;
    --count;
  }
  return sum;
}

// CHECK-LABEL: <_sum_near_words>:
// CHECK:       mov r{{[0-9]+}}, [r{{[0-9]+}}+]
// CHECK-NOT:   add r{{[0-9]+}}, #2
// CHECK:       rets

// CHECK-LABEL: <_sum_far_words>:
// CHECK:       extp
// CHECK-NEXT:  mov r{{[0-9]+}}, [r{{[0-9]+}}+]
// CHECK-NOT:   add r{{[0-9]+}}, #2
// CHECK:       rets

// A huge pointer may cross a segment boundary and therefore needs the
// carry-aware pointer update instead of the page-local post-increment form.
// CHECK-LABEL: <_sum_huge_words>:
// CHECK-NOT:   [r{{[0-9]+}}+]
// CHECK:       add r{{[0-9]+}}, r{{[0-9]+}}
// CHECK-NEXT:  addc
// CHECK:       rets

// Volatile loads remain unindexed so the memory node keeps its explicit
// ordering and side-effect model.
// CHECK-LABEL: <_sum_volatile_far_words>:
// CHECK-NOT:   [r{{[0-9]+}}+]
// CHECK:       extp
// CHECK-NEXT:  mov r{{[0-9]+}}, [r{{[0-9]+}}]
// CHECK:       add r{{[0-9]+}}, #2
// CHECK:       rets

// CHECK-LABEL: <_sum_volatile_near_words>:
// CHECK-NOT:   [r{{[0-9]+}}+]
// CHECK:       mov r{{[0-9]+}}, [r{{[0-9]+}}]
// CHECK:       add r{{[0-9]+}}, #2
// CHECK:       rets
