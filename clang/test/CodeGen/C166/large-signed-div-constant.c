// RUN: %clang_cc1 -triple c166-none-elf -O0 -emit-obj -mllvm -verify-machineinstrs -o %t.o %s
// RUN: llvm-objdump -d %t.o | FileCheck %s
// RUN: %clang_cc1 -triple c166-none-elf -O2 -emit-obj -mllvm -verify-machineinstrs -o %t.opt.o %s
// RUN: llvm-objdump -d %t.opt.o | FileCheck %s

typedef signed char s8;
typedef signed int s16;

__attribute__((noinline)) s16 signed_divide_by_six(s8 a, s8 b) {
  s16 product = (s16)((s16)a * (s16)b);
  s16 delta = (s16)a - (s16)b;
  return (s16)(product / 6 + delta);
}

// CHECK-LABEL: <_signed_divide_by_six>:
// CHECK: mul
// CHECK: mov {{.*}}, mdh
// CHECK: rets
