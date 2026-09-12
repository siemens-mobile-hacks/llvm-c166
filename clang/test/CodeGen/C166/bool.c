// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=tiny -O1 -mllvm -verify-machineinstrs -c %s -o %t.tiny.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t.large.o
// RUN: %clang --target=c166-none-elf -mcmodel=huge -O1 -mllvm -verify-machineinstrs -c %s -o %t.huge.o
// RUN: %clang_cc1 -triple c166-none-elf -emit-llvm -o - %s | FileCheck %s

#include <stdarg.h>

struct bool_record {
  unsigned char prefix;
  _Bool value;
  unsigned int tail;
};

_Static_assert(sizeof(_Bool) == 1, "byte-sized bool");
_Static_assert(_Alignof(_Bool) == 1, "byte-aligned bool");
_Static_assert(sizeof(struct bool_record) == 4, "bool record layout");
_Static_assert(__builtin_offsetof(struct bool_record, value) == 1,
               "bool record offset");
_Static_assert(__builtin_offsetof(struct bool_record, tail) == 2,
               "word member alignment");

_Bool identity_bool(_Bool value) { return value; }
_Bool compare_bool(unsigned int left, unsigned int right) {
  return left < right;
}
unsigned int promote_bool(_Bool value) { return value; }
unsigned int take_bool_vararg(unsigned int tag, ...) {
  va_list args;
  va_start(args, tag);
  int value = va_arg(args, int);
  va_end(args);
  return (unsigned int)value;
}
unsigned int pass_bool_vararg(_Bool value) {
  return take_bool_vararg(0, value);
}

// CHECK-LABEL: define{{.*}} i1 @identity_bool(i1 noundef %value)
// CHECK-LABEL: define{{.*}} i1 @compare_bool(i16 noundef %left, i16 noundef %right)
// CHECK: icmp ult i16
// CHECK-LABEL: define{{.*}} i16 @promote_bool(i1 noundef %value)
// CHECK: zext i1 %{{.*}} to i16
// CHECK-LABEL: define{{.*}} i16 @pass_bool_vararg(i1 noundef %value)
// CHECK: zext i1 %{{.*}} to i16
// CHECK: call addrspace(1) i16 (i16, ...) @take_bool_vararg(i16 noundef 0, i16 noundef %{{.*}})
