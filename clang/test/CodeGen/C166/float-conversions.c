// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d -r %t.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o0.o

int float_to_int(float value) { return (int)value; }
unsigned int float_to_uint(float value) { return (unsigned int)value; }
long float_to_long(float value) { return (long)value; }
unsigned long float_to_ulong(float value) { return (unsigned long)value; }

float int_to_float(int value) { return (float)value; }
float uint_to_float(unsigned int value) { return (float)value; }
float long_to_float(long value) { return (float)value; }
float ulong_to_float(unsigned long value) { return (float)value; }

int float_equal(float lhs, float rhs) { return lhs == rhs; }
int float_not_equal(float lhs, float rhs) { return lhs != rhs; }
int float_less(float lhs, float rhs) { return lhs < rhs; }
int float_less_equal(float lhs, float rhs) { return lhs <= rhs; }
int float_greater(float lhs, float rhs) { return lhs > rhs; }
int float_greater_equal(float lhs, float rhs) { return lhs >= rhs; }
int float_unordered(float lhs, float rhs) {
  return __builtin_isunordered(lhs, rhs);
}

int double_to_int(double value) { return (int)value; }
unsigned int double_to_uint(double value) { return (unsigned int)value; }
long double_to_long(double value) { return (long)value; }
unsigned long double_to_ulong(double value) { return (unsigned long)value; }

double int_to_double(int value) { return (double)value; }
double uint_to_double(unsigned int value) { return (double)value; }
double long_to_double(long value) { return (double)value; }
double ulong_to_double(unsigned long value) { return (double)value; }

int double_equal(double lhs, double rhs) { return lhs == rhs; }
int double_not_equal(double lhs, double rhs) { return lhs != rhs; }
int double_less(double lhs, double rhs) { return lhs < rhs; }
int double_less_equal(double lhs, double rhs) { return lhs <= rhs; }
int double_greater(double lhs, double rhs) { return lhs > rhs; }
int double_greater_equal(double lhs, double rhs) { return lhs >= rhs; }
int double_unordered(double lhs, double rhs) {
  return __builtin_isunordered(lhs, rhs);
}

// The standard compiler-rt entry points are private implementation details;
// their public C signatures still use the stack-only float boundary.
// CHECK: R_C166_SEG8 ___fixsfsi
// CHECK: R_C166_SEG8 ___fixunssfsi
// CHECK: R_C166_SEG8 ___floatsisf
// CHECK: R_C166_SEG8 ___floatunsisf
// CHECK: R_C166_SEG8 ___eqsf2
// CHECK: R_C166_SEG8 ___nesf2
// CHECK: R_C166_SEG8 ___ltsf2
// CHECK: R_C166_SEG8 ___lesf2
// CHECK: R_C166_SEG8 ___gtsf2
// CHECK: R_C166_SEG8 ___gesf2
// CHECK: R_C166_SEG8 ___unordsf2

// Binary64 conversions and comparisons use the standard compiler-rt names.
// Their double operands remain stack-only/MSW-first at the call
// boundary.  In particular, scalar-result calls retain their R4:R5 dependency
// through caller cleanup instead of allowing the cleanup to be deleted.
// CHECK-LABEL: <_double_to_int>:
// CHECK:       calls
// CHECK:       R_C166_SEG8 ___fixdfsi
// CHECK:       R_C166_SOF16 ___fixdfsi
// CHECK:       add r0, #6
// CHECK-NEXT:  add r0, #2
// CHECK-NEXT:  rets
// CHECK-LABEL: <_double_to_uint>:
// CHECK:       R_C166_SEG8 ___fixunsdfsi
// CHECK-LABEL: <_double_to_long>:
// CHECK:       R_C166_SEG8 ___fixdfsi
// CHECK-LABEL: <_double_to_ulong>:
// CHECK:       R_C166_SEG8 ___fixunsdfsi

// A binary64 return uses the caller-owned eight-byte block addressed through
// R4.  Read the block before releasing it, then copy into the public sret slot.
// CHECK-LABEL: <_int_to_double>:
// CHECK:       R_C166_SEG8 ___floatsidf
// CHECK:       mov {{r[0-9]+}}, [r4]
// CHECK:       add r0, #6
// CHECK-NEXT:  add r0, #2
// CHECK:       rets
// CHECK-LABEL: <_uint_to_double>:
// CHECK:       R_C166_SEG8 ___floatunsidf
// CHECK-LABEL: <_long_to_double>:
// CHECK:       R_C166_SEG8 ___floatsidf
// CHECK-LABEL: <_ulong_to_double>:
// CHECK:       R_C166_SEG8 ___floatunsidf

// Comparison libcalls consume a sixteen-byte stack frame.  The scalar result
// remains available in R4:R5 after cleanup for the predicate materialization.
// CHECK-LABEL: <_double_equal>:
// CHECK:       R_C166_SEG8 ___eqdf2
// CHECK:       add r0, #6
// CHECK-NEXT:  add r0, #6
// CHECK-NEXT:  add r0, #4
// CHECK:       cmp r5,
// CHECK-LABEL: <_double_not_equal>:
// CHECK:       R_C166_SEG8 ___nedf2
// CHECK-LABEL: <_double_less>:
// CHECK:       R_C166_SEG8 ___ltdf2
// CHECK-LABEL: <_double_less_equal>:
// CHECK:       R_C166_SEG8 ___ledf2
// CHECK-LABEL: <_double_greater>:
// CHECK:       R_C166_SEG8 ___gtdf2
// CHECK-LABEL: <_double_greater_equal>:
// CHECK:       R_C166_SEG8 ___gedf2
// CHECK-LABEL: <_double_unordered>:
// CHECK:       R_C166_SEG8 ___unorddf2
