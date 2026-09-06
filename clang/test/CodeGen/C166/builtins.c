// RUN: %clang_cc1 -triple c166-none-elf -emit-llvm -o - %s | FileCheck %s

unsigned long packed_divlu(unsigned long dividend, unsigned int divisor) {
  return __builtin_c166_divlu(dividend, divisor);
}

// CHECK-LABEL: define{{.*}} i32 @packed_divlu(i32{{.*}}, i16{{.*}})
// CHECK: call addrspace(1) i32 @llvm.c166.divlu(i32 %{{.*}}, i16 %{{.*}})

unsigned int leading_zeros(unsigned int value) {
  return __builtin_clz(value);
}

// CHECK-LABEL: define{{.*}} i16 @leading_zeros(i16{{.*}})
// CHECK: call addrspace(1) i16 @llvm.ctlz.i16(i16 %{{.*}}, i1 true)
