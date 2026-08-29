// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=large -O1 -emit-llvm %s -o - | FileCheck %s --check-prefix=HUGE-IR
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=large -O1 -emit-obj %s -o %t.large.o
// RUN: llvm-objdump -dr %t.large.o | FileCheck %s --check-prefix=HUGE-ASM
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -emit-llvm %s -o - | FileCheck %s --check-prefix=MEDIUM-IR
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -emit-obj %s -o %t.medium.o
// RUN: llvm-objdump -dr %t.medium.o | FileCheck %s --check-prefix=MEDIUM-ASM
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=small -O1 -emit-llvm %s -o - | FileCheck %s --check-prefix=HUGE-IR
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=small -O1 -emit-obj %s -o %t.small.o
// RUN: llvm-objdump -dr %t.small.o | FileCheck %s --check-prefix=HUGE-ASM

typedef unsigned int u16;
typedef u16 __attribute__((c166_near)) near_fn(u16);
typedef u16 __attribute__((c166_huge)) huge_fn(u16);

extern u16 __attribute__((c166_near)) near_external(u16);
extern u16 __attribute__((c166_huge)) huge_external(u16);

u16 __attribute__((c166_near)) near_identity(u16 value) { return value; }
u16 __attribute__((c166_huge)) huge_identity(u16 value) { return value; }

u16 call_near(u16 value) { return near_external(value); }
u16 call_huge(u16 value) { return huge_external(value); }
u16 call_near_indirect(near_fn *function, u16 value) {
  return function(value);
}
u16 call_huge_indirect(huge_fn *function, u16 value) {
  return function(value);
}

// C166-ABI: functions.address_attributes
// HUGE-IR-LABEL: define{{.*}} i16 @near_identity(
// HUGE-IR-SAME: addrspace(3)
// HUGE-IR-LABEL: define{{.*}} i16 @huge_identity(
// HUGE-IR-SAME: addrspace(1)
// HUGE-IR: call addrspace(3) i16 @near_external
// HUGE-IR: call addrspace(1) i16 @huge_external
// HUGE-IR-LABEL: define{{.*}} i16 @call_near_indirect(ptr addrspace(3)
// HUGE-IR: call addrspace(3) i16 %
// HUGE-IR-LABEL: define{{.*}} i16 @call_huge_indirect(ptr addrspace(1)
// HUGE-IR: call addrspace(1) i16 %

// MEDIUM-IR-LABEL: define{{.*}} i16 @near_identity(
// MEDIUM-IR-SAME: addrspace(3)
// MEDIUM-IR-LABEL: define{{.*}} i16 @huge_identity(
// MEDIUM-IR-SAME: addrspace(1)
// MEDIUM-IR: call addrspace(3) i16 @near_external
// MEDIUM-IR: call addrspace(1) i16 @huge_external
// MEDIUM-IR-LABEL: define{{.*}} i16 @call_near_indirect(ptr addrspace(3)
// MEDIUM-IR: call addrspace(3) i16 %
// MEDIUM-IR-LABEL: define{{.*}} i16 @call_huge_indirect(ptr addrspace(1)
// MEDIUM-IR: call addrspace(1) i16 %

// HUGE-ASM-LABEL: <_huge_identity>:
// HUGE-ASM: rets
// HUGE-ASM-LABEL: <_call_near>:
// HUGE-ASM: calla
// HUGE-ASM: R_C166_COF16{{[[:space:]]+}}_near_external
// HUGE-ASM-LABEL: <_call_huge>:
// HUGE-ASM: jmps
// HUGE-ASM: R_C166_SEG24{{[[:space:]]+}}_huge_external
// HUGE-ASM-LABEL: <_call_near_indirect>:
// HUGE-ASM: calli
// HUGE-ASM-LABEL: <_call_huge_indirect>:
// HUGE-ASM: calls
// HUGE-ASM: R_C166_SEG24{{[[:space:]]+}}__icall
// HUGE-ASM-LABEL: <_near_identity>:
// HUGE-ASM: ret

// MEDIUM-ASM-LABEL: <_huge_identity>:
// MEDIUM-ASM: rets
// MEDIUM-ASM-LABEL: <_near_identity>:
// MEDIUM-ASM: ret
// MEDIUM-ASM-LABEL: <_call_near>:
// MEDIUM-ASM: jmpa
// MEDIUM-ASM: R_C166_COF16{{[[:space:]]+}}_near_external
// MEDIUM-ASM-LABEL: <_call_huge>:
// MEDIUM-ASM: calls
// MEDIUM-ASM: R_C166_SEG24{{[[:space:]]+}}_huge_external
// MEDIUM-ASM-LABEL: <_call_near_indirect>:
// MEDIUM-ASM: calli
// MEDIUM-ASM-LABEL: <_call_huge_indirect>:
// MEDIUM-ASM: calla
// MEDIUM-ASM: R_C166_COF16{{[[:space:]]+}}__icall
