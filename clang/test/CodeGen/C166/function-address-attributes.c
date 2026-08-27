// RUN: %clang_cc1 -triple c166-none-elf -O1 -emit-llvm %s -o - | FileCheck %s --check-prefix=LARGE-IR
// RUN: %clang_cc1 -triple c166-none-elf -O1 -emit-obj %s -o %t.large.o
// RUN: llvm-objdump -dr %t.large.o | FileCheck %s --check-prefix=LARGE-ASM
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -emit-llvm %s -o - | FileCheck %s --check-prefix=MEDIUM-IR
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -emit-obj %s -o %t.medium.o
// RUN: llvm-objdump -dr %t.medium.o | FileCheck %s --check-prefix=MEDIUM-ASM

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
// LARGE-IR-LABEL: define{{.*}} i16 @near_identity(
// LARGE-IR-SAME: addrspace(3)
// LARGE-IR-LABEL: define{{.*}} i16 @huge_identity(
// LARGE-IR-SAME: addrspace(1)
// LARGE-IR: call addrspace(3) i16 @near_external
// LARGE-IR: call addrspace(1) i16 @huge_external
// LARGE-IR-LABEL: define{{.*}} i16 @call_near_indirect(ptr addrspace(3)
// LARGE-IR: call addrspace(3) i16 %
// LARGE-IR-LABEL: define{{.*}} i16 @call_huge_indirect(ptr addrspace(1)
// LARGE-IR: call addrspace(1) i16 %

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

// LARGE-ASM-LABEL: <_huge_identity>:
// LARGE-ASM: rets
// LARGE-ASM-LABEL: <_call_near>:
// LARGE-ASM: calla
// LARGE-ASM: R_C166_COF16{{[[:space:]]+}}_near_external
// LARGE-ASM-LABEL: <_call_huge>:
// LARGE-ASM: calls
// LARGE-ASM: R_C166_SEG8{{[[:space:]]+}}_huge_external
// LARGE-ASM-NEXT: {{.*}}R_C166_SOF16{{[[:space:]]+}}_huge_external
// LARGE-ASM-LABEL: <_call_near_indirect>:
// LARGE-ASM: calli
// LARGE-ASM-LABEL: <_call_huge_indirect>:
// LARGE-ASM: calls
// LARGE-ASM: R_C166_SEG8{{[[:space:]]+}}__icall
// LARGE-ASM-LABEL: <_near_identity>:
// LARGE-ASM: ret

// MEDIUM-ASM-LABEL: <_huge_identity>:
// MEDIUM-ASM: rets
// MEDIUM-ASM-LABEL: <_near_identity>:
// MEDIUM-ASM: ret
// MEDIUM-ASM-LABEL: <_call_near>:
// MEDIUM-ASM: calla
// MEDIUM-ASM: R_C166_COF16{{[[:space:]]+}}_near_external
// MEDIUM-ASM-LABEL: <_call_huge>:
// MEDIUM-ASM: calls
// MEDIUM-ASM: R_C166_SEG8{{[[:space:]]+}}_huge_external
// MEDIUM-ASM-NEXT: {{.*}}R_C166_SOF16{{[[:space:]]+}}_huge_external
// MEDIUM-ASM-LABEL: <_call_near_indirect>:
// MEDIUM-ASM: calli
// MEDIUM-ASM-LABEL: <_call_huge_indirect>:
// MEDIUM-ASM: calla
// MEDIUM-ASM: R_C166_COF16{{[[:space:]]+}}__icall
