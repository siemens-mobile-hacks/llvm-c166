// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -Oz -mllvm -verify-machineinstrs -c %s -o %t.large.o
// RUN: llvm-objdump -d %t.large.o | FileCheck %s --check-prefix=FAR
// RUN: %clang --target=c166-none-elf -mcmodel=medium -Oz -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: llvm-objdump -d %t.medium.o | FileCheck %s --check-prefix=MEDIUM
// RUN: %clang --target=c166-none-elf -mcmodel=small -Oz -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: llvm-objdump -d %t.small.o | FileCheck %s --check-prefix=FAR
// C166-ABI: calls.tail

typedef unsigned int u16;
typedef struct {
  u16 word[4];
} aggregate;

extern u16 callee(u16);
extern u16 callee_five(u16, u16, u16, u16, u16);
extern u16 __attribute__((c166_near)) near_callee(u16);
extern u16 __attribute__((c166_huge)) huge_callee(u16);
extern double double_callee(double, double);
extern aggregate aggregate_callee(aggregate, aggregate);

u16 tail_default(u16 value) { return callee(value); }

u16 __attribute__((c166_near)) tail_near(u16 value) {
  return near_callee(value);
}

u16 no_tail_mismatched_return(u16 value) { return near_callee(value); }

u16 tail_huge(u16 value) { return huge_callee(value); }

u16 tail_with_frame(u16 value) {
  volatile u16 local = value;
  return callee(local);
}

u16 tail_with_callee_save(u16 value) {
  __asm__ volatile("" ::: "r6");
  return callee(value);
}

u16 no_tail_stack_argument(u16 a, u16 b, u16 c, u16 d, u16 e) {
  return callee_five(a, b, c, d, e);
}

double tail_double(double left, double right) {
  return double_callee(left, right);
}

double no_tail_reordered_double(double left, double right) {
  return double_callee(right, left);
}

aggregate tail_aggregate(aggregate left, aggregate right) {
  return aggregate_callee(left, right);
}

aggregate no_tail_reordered_aggregate(aggregate left, aggregate right) {
  return aggregate_callee(right, left);
}

// FAR-LABEL: <_tail_default>:
// FAR-NEXT:  jmps

// FAR-LABEL: <_no_tail_mismatched_return>:
// FAR:       calla
// FAR-NEXT:  rets

// FAR-LABEL: <_tail_huge>:
// FAR-NEXT:  jmps

// FAR-LABEL: <_tail_with_frame>:
// FAR:       add r0, #2
// FAR-NEXT:  jmps

// FAR-LABEL: <_tail_with_callee_save>:
// FAR:       mov r6, [r0+]
// FAR-NEXT:  jmps

// FAR-LABEL: <_no_tail_stack_argument>:
// FAR:       calls
// FAR:       rets

// FAR-LABEL: <_tail_double>:
// FAR-NEXT:  jmps

// FAR-LABEL: <_no_tail_reordered_double>:
// FAR:       calls
// FAR:       rets

// FAR-LABEL: <_tail_aggregate>:
// FAR-NEXT:  jmps

// FAR-LABEL: <_no_tail_reordered_aggregate>:
// FAR:       calls
// FAR:       rets

// FAR-LABEL: <_tail_near>:
// FAR-NEXT:  jmpa

// MEDIUM-LABEL: <_tail_default>:
// MEDIUM-NEXT:  jmpa

// MEDIUM-LABEL: <_tail_near>:
// MEDIUM-NEXT:  jmpa

// MEDIUM-LABEL: <_no_tail_mismatched_return>:
// MEDIUM-NEXT:  jmpa

// MEDIUM-LABEL: <_tail_huge>:
// MEDIUM:       calls
// MEDIUM-NEXT:  ret

// MEDIUM-LABEL: <_tail_with_frame>:
// MEDIUM:       add r0, #2
// MEDIUM-NEXT:  jmpa

// MEDIUM-LABEL: <_tail_with_callee_save>:
// MEDIUM:       mov r6, [r0+]
// MEDIUM-NEXT:  jmpa

// MEDIUM-LABEL: <_no_tail_stack_argument>:
// MEDIUM:       calla
// MEDIUM:       ret

// MEDIUM-LABEL: <_tail_double>:
// MEDIUM-NEXT:  jmpa

// MEDIUM-LABEL: <_no_tail_reordered_double>:
// MEDIUM:       calla
// MEDIUM:       ret

// MEDIUM-LABEL: <_tail_aggregate>:
// MEDIUM-NEXT:  jmpa

// MEDIUM-LABEL: <_no_tail_reordered_aggregate>:
// MEDIUM:       calla
// MEDIUM:       ret
