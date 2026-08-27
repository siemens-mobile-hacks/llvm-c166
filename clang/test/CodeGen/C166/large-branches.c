// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s

extern void less(unsigned int);
extern void greater_equal(unsigned int);

void choose_branch(unsigned int a, unsigned int b) {
  if (a < b)
    less(a);
  else
    greater_equal(b);
}

void signed_branch(signed int a, signed int b) {
  if (a >= b)
    less((unsigned int)a);
  else
    greater_equal((unsigned int)b);
}

unsigned int choose_value(unsigned int a, unsigned int b) {
  return a < b ? a : b;
}

unsigned int compare_value(unsigned int a, unsigned int b) {
  return a != b;
}

void count_down(unsigned int n) {
  while (n) {
    less(n);
    --n;
  }
}

// CHECK-LABEL: <_choose_branch>:
// CHECK:       cmp r12, r13
// CHECK-NEXT:  jmpr cc_uge
// CHECK:       calls
// CHECK:       rets
// CHECK:       calls
// CHECK:       rets

// CHECK-LABEL: <_signed_branch>:
// CHECK:       cmp r12, r13
// CHECK-NEXT:  jmpr cc_sge
// CHECK:       calls
// CHECK:       calls

// CHECK-LABEL: <_choose_value>:
// CHECK:       mov r4, r12
// CHECK-NEXT:  cmp r4, r13
// CHECK-NEXT:  jmpr cc_ult
// CHECK:       mov r4, r13
// CHECK:       rets

// CHECK-LABEL: <_compare_value>:
// CHECK:       mov r4, #1
// CHECK-NEXT:  cmp r12, r13
// CHECK-NEXT:  jmpr cc_ne
// CHECK:       mov r4, #0
// CHECK:       rets

// CHECK-LABEL: <_count_down>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_eq
// CHECK:       calls
// CHECK:       sub [[COUNT:r[0-9]+]], #1
// CHECK-NEXT:  mov {{r[0-9]+}}, #0
// CHECK-NEXT:  cmp [[COUNT]],
// CHECK-NEXT:  jmpr cc_ne
// CHECK:       rets
