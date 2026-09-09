// REQUIRES: c166-registered-target
// RUN: %clang --target=c166 -mcmodel=small -O0 -c %s -o %t.o
// RUN: %clang --target=c166 -mcmodel=medium -O0 -c %s -o %t.medium.o
// RUN: %clang --target=c166 -mcmodel=large -O0 -c %s -o %t.large.o
// RUN: %clang --target=c166 -mcmodel=small -O2 -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s

#include <stdarg.h>

void copy_list(va_list *destination, va_list *source) {
  va_copy(*destination, *source);
}

// CHECK-LABEL: <_copy_list>:
// CHECK-NEXT: {{.*}} mov [r12], [r13]
// CHECK-NEXT: {{.*}} rets
