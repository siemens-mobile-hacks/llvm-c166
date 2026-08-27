// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// C166-ABI: pointers.default_far_comparison
// C166-ABI: pointers.default_far_difference
// C166-ABI: pointers.null_comparison
// C166-ABI: pointers.function_comparison
// C166-ABI: pointers.far_arithmetic_width

unsigned int far_eq(const unsigned int *lhs, const unsigned int *rhs) {
  return lhs == rhs;
}

unsigned int far_ne(const unsigned int *lhs, const unsigned int *rhs) {
  return lhs != rhs;
}

unsigned int far_lt(const unsigned int *lhs, const unsigned int *rhs) {
  return lhs < rhs;
}

unsigned int far_le(const unsigned int *lhs, const unsigned int *rhs) {
  return lhs <= rhs;
}

unsigned int far_gt(const unsigned int *lhs, const unsigned int *rhs) {
  return lhs > rhs;
}

unsigned int far_ge(const unsigned int *lhs, const unsigned int *rhs) {
  return lhs >= rhs;
}

unsigned int far_is_null(const unsigned int *address) {
  return address == 0;
}

int far_difference(const unsigned int *lhs, const unsigned int *rhs) {
  return lhs - rhs;
}

const unsigned int *far_subtract(const unsigned int *address,
                                 unsigned int count) {
  return address - count;
}

unsigned char *add_3fff(unsigned char *address) {
  return address + 0x3fffU;
}

unsigned char *add_4000(unsigned char *address) {
  return address + 0x4000U;
}

unsigned char *add_ffff(unsigned char *address) {
  return address + 0xffffU;
}

unsigned char *subtract_one(unsigned char *address) {
  return address - 1U;
}

typedef void (*callback)(void);

unsigned int function_eq(callback lhs, callback rhs) {
  return lhs == rhs;
}

unsigned int function_is_null(callback function) {
  return function == 0;
}

// Large-model data-pointer comparisons use only the page offset except when
// one operand is null.  Function-pointer equality always compares both words.
// IR-LABEL: define{{.*}}i16 @far_eq
// IR:       trunc i32 {{.*}} to i16
// IR:       trunc i32 {{.*}} to i16
// IR:       icmp eq i16
// IR-LABEL: define{{.*}}i16 @far_is_null
// IR:       call{{.*}} i16 @llvm.c166.high.word(i32 {{.*}})
// IR:       icmp eq i16
// IR-LABEL: define{{.*}}i16 @far_difference
// IR:       trunc i32 {{.*}} to i16
// IR:       trunc i32 {{.*}} to i16
// IR:       sub i16
// IR:       ashr exact i16 {{.*}}, 1

// DIS-LABEL: <_far_eq>:
// DIS:       cmp r12, r14
// DIS-NOT:   cmp r13, r15
// DIS:       rets
// DIS-LABEL: <_far_ne>:
// DIS:       cmp r12, r14
// DIS-NOT:   cmp r13, r15
// DIS:       rets
// DIS-LABEL: <_far_lt>:
// DIS:       cmp r12, r14
// DIS-NOT:   cmp r13, r15
// DIS:       rets
// DIS-LABEL: <_far_le>:
// DIS:       cmp r12, r14
// DIS-NOT:   cmp r13, r15
// DIS:       rets
// DIS-LABEL: <_far_gt>:
// DIS:       cmp r12, r14
// DIS-NOT:   cmp r13, r15
// DIS:       rets
// DIS-LABEL: <_far_ge>:
// DIS:       cmp r12, r14
// DIS-NOT:   cmp r13, r15
// DIS:       rets
// DIS-LABEL: <_far_is_null>:
// DIS:       cmp r13, {{r[0-9]+}}
// DIS:       cmp r12, {{r[0-9]+}}
// DIS:       and
// DIS:       rets
// DIS-LABEL: <_far_difference>:
// DIS:       sub r4, r14
// DIS-NEXT:  ashr r4, #1
// DIS:       rets
// DIS-LABEL: <_far_subtract>:
// DIS:       mov r5, r13
// DIS-NOT:   addc
// DIS-NOT:   subc
// DIS:       rets
// DIS-LABEL: <_add_3fff>:
// DIS:       mov r5, r13
// DIS:       mov {{r[0-9]+}}, #16383
// DIS:       add r4, {{r[0-9]+}}
// DIS-NOT:   addc
// DIS:       rets
// DIS-LABEL: <_add_4000>:
// DIS:       mov r5, r13
// DIS:       mov {{r[0-9]+}}, #16384
// DIS:       add r4, {{r[0-9]+}}
// DIS-NOT:   addc
// DIS:       rets
// DIS-LABEL: <_add_ffff>:
// DIS:       mov r5, r13
// DIS:       mov {{r[0-9]+}}, #65535
// DIS:       add r4, {{r[0-9]+}}
// DIS-NOT:   addc
// DIS:       rets
// DIS-LABEL: <_subtract_one>:
// DIS:       mov r5, r13
// DIS:       add r4, {{r[0-9]+}}
// DIS-NOT:   addc
// DIS:       rets
// DIS-LABEL: <_function_eq>:
// DIS:       cmp r13, r15
// DIS:       jmpr cc_ne
// DIS:       cmp r12, r14
// DIS:       jmpr cc_ne
// DIS-NOT:   and
// DIS:       rets
// DIS-LABEL: <_function_is_null>:
// DIS:       cmp r13, {{r[0-9]+}}
// DIS:       jmpr cc_ne
// DIS:       cmp r12, {{r[0-9]+}}
// DIS:       jmpr cc_ne
// DIS-NOT:   and
// DIS:       rets
