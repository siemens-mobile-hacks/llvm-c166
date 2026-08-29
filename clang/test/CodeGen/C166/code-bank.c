// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// C166-ABI: calls.code_banks
// C166-ABI: floating.banked_boundary
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t.o

typedef unsigned int u16;
typedef u16 __attribute__((c166_bank(1))) bank1_fn5(u16, u16, u16, u16, u16);
typedef u16 __attribute__((c166_bank(2))) bank2_fn5(u16, u16, u16, u16, u16);
typedef u16 plain_fn5(u16, u16, u16, u16, u16);
typedef float __attribute__((c166_bank(1))) bank1_float_fn(float, u16);
typedef float __attribute__((c166_bank(2))) bank2_float_fn(float, u16);
typedef double __attribute__((c166_bank(1))) bank1_double_fn(double, u16);
typedef double __attribute__((c166_bank(2))) bank2_double_fn(double, u16);

extern u16 __attribute__((c166_bank(1))) bank1_external(u16, u16, u16, u16, u16);
extern u16 __attribute__((c166_bank(2))) bank2_external(u16, u16, u16, u16, u16);
extern u16 plain_external(u16, u16, u16, u16, u16);
extern float __attribute__((c166_bank(1))) bank1_float_external(float, u16);
extern float __attribute__((c166_bank(2))) bank2_float_external(float, u16);
extern double __attribute__((c166_bank(1))) bank1_double_external(double, u16);
extern double __attribute__((c166_bank(2))) bank2_double_external(double, u16);

u16 __attribute__((c166_bank(1))) bank1_callee(u16 a, u16 b, u16 c, u16 d,
                                               u16 e) {
  return a + b + c + d + e;
}

u16 __attribute__((c166_bank(1))) bank1_same(u16 value) {
  return bank1_external(value, 2, 3, 4, 5);
}

u16 __attribute__((c166_bank(1))) bank1_cross(u16 value) {
  return bank2_external(value, 2, 3, 4, 5);
}

u16 unbanked_to_one(u16 value) {
  return bank1_external(value, 2, 3, 4, 5);
}

u16 __attribute__((c166_bank(1))) bank1_indirect_same(bank1_fn5 *fn,
                                                      u16 value) {
  return fn(value, 2, 3, 4, 5);
}

u16 __attribute__((c166_bank(1))) bank1_indirect_cross(bank2_fn5 *fn,
                                                       u16 value) {
  return fn(value, 2, 3, 4, 5);
}

u16 __attribute__((c166_bank(1))) bank1_to_plain(u16 value) {
  return plain_external(value, 2, 3, 4, 5);
}

float __attribute__((c166_bank(1))) bank1_float_identity(float value,
                                                          u16 tail) {
  return value;
}

double __attribute__((c166_bank(1))) bank1_double_identity(double value,
                                                            u16 tail) {
  return value;
}

float __attribute__((c166_bank(1))) bank1_float_same(float value, u16 tail) {
  return bank1_float_external(value, tail);
}

float __attribute__((c166_bank(1))) bank1_float_cross(float value, u16 tail) {
  return bank2_float_external(value, tail);
}

double __attribute__((c166_bank(1))) bank1_double_same(double value,
                                                        u16 tail) {
  return bank1_double_external(value, tail);
}

double __attribute__((c166_bank(1))) bank1_double_cross(double value,
                                                         u16 tail) {
  return bank2_double_external(value, tail);
}

float __attribute__((c166_bank(1)))
bank1_float_indirect_same(bank1_float_fn *function, float value, u16 tail) {
  return function(value, tail);
}

float __attribute__((c166_bank(1)))
bank1_float_indirect_cross(bank2_float_fn *function, float value, u16 tail) {
  return function(value, tail);
}

double __attribute__((c166_bank(1)))
bank1_double_indirect_same(bank1_double_fn *function, double value, u16 tail) {
  return function(value, tail);
}

double __attribute__((c166_bank(1)))
bank1_double_indirect_cross(bank2_double_fn *function, double value, u16 tail) {
  return function(value, tail);
}

// IR: define{{.*}}i16 @bank1_callee{{.*}}addrspace(257)
// IR: define{{.*}}i16 @bank1_same{{.*}}addrspace(257)
// IR: call addrspace(257){{.*}}@bank1_external
// IR: define{{.*}}i16 @bank1_cross{{.*}}addrspace(257)
// IR: call addrspace(258){{.*}}@bank2_external
// IR: define{{.*}}i16 @unbanked_to_one{{.*}}addrspace(1)
// IR: call addrspace(257){{.*}}@bank1_external
// IR: define{{.*}}i16 @bank1_indirect_same{{.*}}addrspace(257)
// IR: call addrspace(257){{.*}}%{{.*}}
// IR: define{{.*}}i16 @bank1_indirect_cross{{.*}}addrspace(257)
// IR: call addrspace(258){{.*}}%{{.*}}
// IR: define{{.*}}float @bank1_float_identity{{.*}}addrspace(257)
// IR: define{{.*}}void @bank1_double_identity{{.*}}addrspace(257)
// IR: define{{.*}}float @bank1_float_same{{.*}}addrspace(257)
// IR: call addrspace(257){{.*}}float @bank1_float_external
// IR: define{{.*}}float @bank1_float_cross{{.*}}addrspace(257)
// IR: call addrspace(258){{.*}}float @bank2_float_external
// IR: define{{.*}}void @bank1_double_same{{.*}}addrspace(257)
// IR: call addrspace(257){{.*}}void @bank1_double_external
// IR: define{{.*}}void @bank1_double_cross{{.*}}addrspace(257)
// IR: call addrspace(258){{.*}}void @bank2_double_external
// IR: define{{.*}}float @bank1_float_indirect_same{{.*}}addrspace(257)
// IR: call addrspace(257){{.*}}float %{{.*}}
// IR: define{{.*}}float @bank1_float_indirect_cross{{.*}}addrspace(257)
// IR: call addrspace(258){{.*}}float %{{.*}}
// IR: define{{.*}}void @bank1_double_indirect_same{{.*}}addrspace(257)
// IR: call addrspace(257){{.*}}void %{{.*}}
// IR: define{{.*}}void @bank1_double_indirect_cross{{.*}}addrspace(257)
// IR: call addrspace(258){{.*}}void %{{.*}}

// ASM: .section .text.c166.bank.1
// ASM-LABEL: _bank1_callee:
// ASM: mov {{r[0-9]+}}, [r0 + #2]
// ASM: rets
// ASM-LABEL: _bank1_same:
// ASM: mov [-r0], {{r[0-9]+}}
// ASM: sub r0, #2
// ASM: calls seg(_bank1_external), sof(_bank1_external)
// ASM: add r0, #4
// ASM-LABEL: _bank1_cross:
// ASM: mov [-r0], {{r[0-9]+}}
// ASM: mov r3, #258
// ASM-NEXT: mov [-r0], r3
// ASM: calls seg(__banksw), sof(__banksw)
// ASM: add r0, #4
// ASM-LABEL: _unbanked_to_one:
// ASM: mov r3, #1
// ASM-NEXT: mov [-r0], r3
// ASM: calls seg(__banksw), sof(__banksw)
// ASM-LABEL: _bank1_indirect_same:
// ASM: mov [-r0], {{r[0-9]+}}
// ASM: sub r0, #2
// ASM: calls seg(__icall), sof(__icall)
// ASM-LABEL: _bank1_indirect_cross:
// ASM: mov r3, #258
// ASM-NEXT: mov [-r0], r3
// ASM: calls seg(__banksw), sof(__banksw)
// ASM-LABEL: _bank1_to_plain:
// ASM: mov [-r0], {{r[0-9]+}}
// ASM: calls seg(_plain_external), sof(_plain_external)
// ASM-LABEL: _bank1_float_identity:
// ASM: mov r4, [r0 + #2]
// ASM-NEXT: mov r5, [r0 + #4]
// ASM-NEXT: rets
// ASM-LABEL: _bank1_double_identity:
// ASM: mov {{r[0-9]+}}, [r0 + #2]
// ASM: mov [r0 + #12], {{r[0-9]+}}
// ASM: mov r10, r4
// ASM-NEXT: rets
// ASM-LABEL: _bank1_float_same:
// ASM-COUNT-3: mov [-r0], {{r[0-9]+}}
// ASM: sub r0, #2
// ASM: calls seg(_bank1_float_external), sof(_bank1_float_external)
// ASM: rets
// ASM-LABEL: _bank1_float_cross:
// ASM-COUNT-3: mov [-r0], {{r[0-9]+}}
// ASM: mov r3, #258
// ASM-NEXT: mov [-r0], r3
// ASM: calls seg(__banksw), sof(__banksw)
// ASM: rets
// ASM-LABEL: _bank1_double_same:
// ASM-COUNT-5: mov [-r0], {{r[0-9]+}}
// ASM: sub r0, #2
// ASM: calls seg(_bank1_double_external), sof(_bank1_double_external)
// ASM: mov {{r[0-9]+}}, [r4]
// ASM: rets
// ASM-LABEL: _bank1_double_cross:
// ASM-COUNT-5: mov [-r0], {{r[0-9]+}}
// ASM: mov r3, #258
// ASM-NEXT: mov [-r0], r3
// ASM: calls seg(__banksw), sof(__banksw)
// ASM: mov {{r[0-9]+}}, [r4]
// ASM: rets
// ASM-LABEL: _bank1_float_indirect_same:
// ASM: calls seg(__icall), sof(__icall)
// ASM: rets
// ASM-LABEL: _bank1_float_indirect_cross:
// ASM: mov r3, #258
// ASM-NEXT: mov [-r0], r3
// ASM: calls seg(__banksw), sof(__banksw)
// ASM: rets
// ASM-LABEL: _bank1_double_indirect_same:
// ASM: calls seg(__icall), sof(__icall)
// ASM: mov {{r[0-9]+}}, [r4]
// ASM: rets
// ASM-LABEL: _bank1_double_indirect_cross:
// ASM: mov r3, #258
// ASM-NEXT: mov [-r0], r3
// ASM: calls seg(__banksw), sof(__banksw)
// ASM: mov {{r[0-9]+}}, [r4]
// ASM: rets
