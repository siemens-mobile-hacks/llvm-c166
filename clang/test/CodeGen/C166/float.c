// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-medium.o
// RUN: llvm-objdump -dr %t-medium.o | FileCheck %s --check-prefix=MEDIUM
// RUN: llvm-objdump -d -r %t.o | FileCheck %s
// RUN: llvm-objdump -s %t.o | FileCheck %s --check-prefix=DATA
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// C166-ABI: floating.float_stack_and_return
// C166-ABI: floating.double_stack_and_return
// C166-ABI: medium.floating.boundary

float add_float(float a, float b) { return a + b; }
float sub_float(float a, float b) { return a - b; }
float mul_float(float a, float b) { return a * b; }
float div_float(float a, float b) { return a / b; }

double identity_double(double value) { return value; }
double call_identity_double(double value) { return identity_double(value); }
double add_double(double a, double b) { return a + b; }
double sub_double(double a, double b) { return a - b; }
double mul_double(double a, double b) { return a * b; }
double div_double(double a, double b) { return a / b; }

volatile double stored_double = 1.0;
double load_stored_double(void) { return stored_double; }
void store_stored_double(double value) { stored_double = value; }

extern float external_float(unsigned int head, float value,
                            unsigned int tail);
float call_external_float(unsigned int head, float value,
                          unsigned int tail) {
  return external_float(head, value, tail);
}

// The C166 ABI copies real floating arguments to the user
// stack.  Keep float direct in IR so the backend can distinguish it from an
// integer pair and apply the MSW-first stack/return representation;
// double is returned through a caller-reserved stack block.
// IR-LABEL: define{{.*}} float @add_float(float{{.*}}, float{{.*}})
// IR-LABEL: define{{.*}} void @identity_double(ptr{{.*}} sret(double) align 2 {{.*}}, ptr{{.*}} byval(double) align 2 {{.*}})

// A binary64 initializer stores the most-significant 16-bit word at
// the lowest address.  Each individual C166 word retains little-endian byte
// order, so IEEE 1.0 (0x3ff0000000000000) begins with bytes f0 3f.
// DATA: Contents of section .data:
// DATA-NEXT: 0000 f03f0000 00000000

// Floating arguments stop allocation in the R12-R15 argument area:
// both operands arrive by value on the user stack.  The LLVM C166 runtime
// helper receives their softened i32 values and returns the result in R4:R5.
// CHECK-LABEL: <_add_float>:
// CHECK-NEXT:  mov [[ARG_BASE:r[0-9]+]], r0
// CHECK-NEXT:  mov r13, {{\[}}[[ARG_BASE]]+]
// CHECK-NEXT:  mov r12, {{\[}}[[ARG_BASE]]+]
// CHECK-NEXT:  mov r15, {{\[}}[[ARG_BASE]]+]
// CHECK-NEXT:  mov r14, {{\[}}[[ARG_BASE]]]
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___addsf3
// CHECK:       mov r1, r5
// CHECK-NEXT:  mov r2, r4
// CHECK-NEXT:  mov r4, r1
// CHECK-NEXT:  mov r5, r2
// CHECK:       rets

// Medium retains the physical floating boundary and caller-owned binary64
// result block; only ordinary definitions, direct calls and runtime helpers
// change to the near class.
// MEDIUM-LABEL: <_add_float>:
// MEDIUM-NEXT:  mov [[ARG_BASE:r[0-9]+]], r0
// MEDIUM-NEXT:  mov r13, {{\[}}[[ARG_BASE]]+]
// MEDIUM-NEXT:  mov r12, {{\[}}[[ARG_BASE]]+]
// MEDIUM-NEXT:  mov r15, {{\[}}[[ARG_BASE]]+]
// MEDIUM-NEXT:  mov r14, {{\[}}[[ARG_BASE]]]
// MEDIUM:       calla
// MEDIUM:       R_C166_COF16 ___addsf3
// MEDIUM:       ret
// MEDIUM-LABEL: <_identity_double>:
// MEDIUM:       mov r10, r4
// MEDIUM:       ret
// MEDIUM-LABEL: <_call_identity_double>:
// MEDIUM-NEXT:  jmpa
// MEDIUM-NEXT:  R_C166_COF16 _identity_double
// MEDIUM-LABEL: <_add_double>:
// MEDIUM:       calla
// MEDIUM:       R_C166_COF16 ___c166_adddf3
// MEDIUM:       ret
// CHECK-LABEL: <_sub_float>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___subsf3
// CHECK:       rets
// CHECK-LABEL: <_mul_float>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___mulsf3
// CHECK:       rets
// CHECK-LABEL: <_div_float>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___divsf3
// CHECK:       rets

// CHECK-LABEL: <_identity_double>:
// CHECK:       mov [[RESULT_ADDRESS:r[0-9]+]], #8
// CHECK-NEXT:  add [[RESULT_ADDRESS]], r0
// CHECK:       mov [r0 + #8],
// CHECK:       mov r10, r4
// CHECK:       rets

// A forwarding double call reuses the incoming argument and result slots when
// their complete ABI layout is identical to the callee's.
// CHECK-LABEL: <_call_identity_double>:
// CHECK-NEXT:  jmps
// CHECK-NEXT:  R_C166_SEG24 _identity_double

// Compiler-generated binary64 arithmetic passes near pointers to the result
// block and both operands.  The containing C functions retain the public
// stack-only arguments and caller-reserved result block.
// CHECK-LABEL: <_add_double>:
// CHECK:       calls
// CHECK:       R_C166_SEG24 ___c166_adddf3
// CHECK-LABEL: <_sub_double>:
// CHECK:       R_C166_SEG24 ___c166_subdf3
// CHECK-LABEL: <_mul_double>:
// CHECK:       R_C166_SEG24 ___c166_muldf3
// CHECK-LABEL: <_div_double>:
// CHECK:       R_C166_SEG24 ___c166_divdf3

// Ordinary object loads/stores use the same type-dependent representation as
// public arguments; their values still cross the caller-reserved double return
// boundary.
// CHECK-LABEL: <_load_stored_double>:
// CHECK:       rets
// CHECK-LABEL: <_store_stored_double>:
// CHECK:       rets

// Once a float forces stack passing, the following integer is stack-only as
// well.  The caller therefore cleans six bytes after the external call while
// the leading integer remains in R12.
// CHECK-LABEL: <_call_external_float>:
// CHECK-NEXT:  mov r1, [r0 + #4]
// CHECK-NEXT:  mov [-r0], r1
// CHECK-NEXT:  mov r1, [r0 + #2]
// CHECK-NEXT:  mov r2, [r0 + #4]
// CHECK-NEXT:  mov [-r0], r2
// CHECK-NEXT:  mov [-r0], r1
// CHECK:       calls
// CHECK:       R_C166_SEG24 _external_float
// CHECK:       add r0, #6
// CHECK:       rets
