// RUN: %clang_cc1 -triple c166 -mcmodel=large -Werror -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,HUGE,BUILTIN
// RUN: %clang_cc1 -triple c166 -mcmodel=medium -Werror -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,NEAR,BUILTIN
// RUN: %clang_cc1 -triple c166 -mcmodel=small -Werror -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,HUGE,BUILTIN
// RUN: %clang_cc1 -triple c166 -mcmodel=large -Werror -fno-builtin -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,HUGE,LIBCALL
// RUN: %clang_cc1 -triple c166 -mcmodel=medium -Werror -fno-builtin -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,NEAR,LIBCALL
// RUN: %clang_cc1 -triple c166 -mcmodel=small -Werror -fno-builtin -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,HUGE,LIBCALL
// RUN: %clang_cc1 -triple c166 -mcmodel=large -Werror -emit-obj %s -o %t.large.o
// RUN: %clang_cc1 -triple c166 -mcmodel=medium -Werror -emit-obj %s -o %t.medium.o
// RUN: %clang_cc1 -triple c166 -mcmodel=small -Werror -emit-obj %s -o %t.small.o

typedef __SIZE_TYPE__ size_t;
extern void *memcpy(void *, const void *, size_t);
extern void *memmove(void *, const void *, size_t);
extern void *memset(void *, int, size_t);
extern int strcmp(const char *, const char *);
extern int strcmp(const char *, const char *);

typedef void *copy_fn(void *, const void *, size_t);
typedef int compare_fn(const char *, const char *);
copy_fn *copy_address = memcpy;
compare_fn *compare_address = strcmp;

// HUGE: @copy_address = {{.*}}global ptr addrspace(1) @memcpy
// HUGE: @compare_address = {{.*}}global ptr addrspace(1) @strcmp
// NEAR: @copy_address = {{.*}}global ptr addrspace(3) @memcpy
// NEAR: @compare_address = {{.*}}global ptr addrspace(3) @strcmp

int compare_direct(const char *a, const char *b) { return strcmp(a, b); }
// CHECK-LABEL: define{{.*}} @compare_direct(
// HUGE: call addrspace(1) i16 @strcmp(
// NEAR: call addrspace(3) i16 @strcmp(

int compare_indirect(compare_fn *f, const char *a, const char *b) {
  return f(a, b);
}
// CHECK-LABEL: define{{.*}} @compare_indirect(
// HUGE: call addrspace(1) i16 %
// NEAR: call addrspace(3) i16 %

void *copy_indirect(copy_fn *f, void *dst, const void *src, size_t n) {
  return f(dst, src, n);
}

void *copy_direct(void *dst, const void *src, size_t n) {
  return memcpy(dst, src, n);
}
// CHECK-LABEL: define{{.*}} @copy_direct(
// BUILTIN: call addrspace({{[13]}}) void @llvm.memcpy.
// LIBCALL: call {{.*}} @memcpy(

void *move_direct(void *dst, const void *src, size_t n) {
  return memmove(dst, src, n);
}
// CHECK-LABEL: define{{.*}} @move_direct(
// BUILTIN: call addrspace({{[13]}}) void @llvm.memmove.
// LIBCALL: call {{.*}} @memmove(

void *fill_direct(void *dst, int value, size_t n) {
  return memset(dst, value, n);
}
// CHECK-LABEL: define{{.*}} @fill_direct(
// BUILTIN: call addrspace({{[13]}}) void @llvm.memset.
// LIBCALL: call {{.*}} @memset(
