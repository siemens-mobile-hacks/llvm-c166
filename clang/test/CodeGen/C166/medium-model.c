// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -E -dM %s | FileCheck %s --check-prefix=MACRO
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -emit-obj %s -o %t.o
// RUN: llvm-objdump -dr %t.o | FileCheck %s --check-prefix=ASM
// RUN: llvm-readobj --file-headers --symbols %t.o | FileCheck %s --check-prefix=ELF
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -S %s -o %t.s
// RUN: FileCheck %s --check-prefix=MODEL-ASM < %t.s
// RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t.s -o %t.roundtrip.o
// RUN: llvm-readobj --file-headers --symbols %t.roundtrip.o | FileCheck %s --check-prefix=ELF

typedef unsigned int u16;

// C166-ABI: medium.elf.identity

typedef u16 default_function(u16, u16);
typedef u16 __attribute__((c166_huge)) huge_function(u16, u16);

_Static_assert(sizeof(default_function *) == 2,
               "Medium default function pointer must be near");
_Static_assert(sizeof(huge_function *) == 4,
               "Medium huge function pointer must be inter-segment");
_Static_assert(sizeof(u16 *) == 4,
               "Medium default data pointer remains paged");

extern u16 default_external(u16, u16);
extern u16 __attribute__((c166_huge)) huge_external(u16, u16);

u16 __attribute__((c166_huge)) huge_identity(u16 value) { return value; }

u16 __attribute__((section(".custom.medium.text")))
custom_section_near(u16 value) {
  return value + 1U;
}

// C166-ABI: medium.functions.default_near
u16 call_default(u16 a, u16 b) { return default_external(a, b); }

// C166-ABI: medium.functions.default_pointer
u16 call_default_indirect(default_function *function, u16 a, u16 b) {
  return function(a, b);
}

default_function *get_default_address(void) { return default_external; }

// C166-ABI: medium.functions.explicit_huge
u16 call_huge(u16 a, u16 b) { return huge_external(a, b); }

// C166-ABI: medium.functions.explicit_huge_pointer
u16 call_huge_indirect(huge_function *function, u16 a, u16 b) {
  return function(a, b);
}

// MACRO: #define __C166_MEMORY_MODEL__ 2
// MACRO: #define __SIZEOF_POINTER__ 4

// MODEL-ASM: .c166_model{{[[:space:]]+}}medium
// MODEL-ASM: .c166_function{{[[:space:]]+}}near, _custom_section_near

// ELF: Flags [ (0x221)
// ELF-NEXT: EF_C166_CODE_NEAR (0x200)
// ELF-NEXT: EF_C166_CORE_8X166 (0x1)
// ELF-NEXT: EF_C166_DATA_FAR (0x20)
// ELF: Name: _custom_section_near
// ELF: Other [ (0x20)
// ELF-NEXT: STO_C166_CODE_NEAR (0x20)

// IR: target datalayout = "{{.*}}-P3-{{.*}}"
// IR-LABEL: define{{.*}} i16 @huge_identity(
// IR-SAME: addrspace(1)
// IR-LABEL: define{{.*}} i16 @call_default(
// IR-SAME: addrspace(3)
// IR: call addrspace(3) i16 @default_external
// IR: declare i16 @default_external(
// IR-SAME: addrspace(3)
// IR-LABEL: define{{.*}} i16 @call_default_indirect(ptr addrspace(3)
// IR-SAME: addrspace(3)
// IR: call addrspace(3) i16 %
// IR-LABEL: define{{.*}} i16 @call_huge(
// IR-SAME: addrspace(3)
// IR: call addrspace(1) i16 @huge_external
// IR: declare i16 @huge_external(
// IR-SAME: addrspace(1)
// IR-LABEL: define{{.*}} i16 @call_huge_indirect(ptr addrspace(1)
// IR-SAME: addrspace(3)
// IR: call addrspace(1) i16 %

// ASM-LABEL: <_huge_identity>:
// ASM: rets
// ASM-LABEL: <_call_default>:
// ASM: calla
// ASM: R_C166_COF16{{[[:space:]]+}}_default_external
// ASM: ret
// ASM-LABEL: <_call_default_indirect>:
// ASM: calli
// ASM: ret
// ASM-LABEL: <_get_default_address>:
// ASM: R_C166_COF16{{[[:space:]]+}}_default_external
// ASM-NOT: R_C166_DPP2_16{{[[:space:]]+}}_default_external
// ASM-LABEL: <_call_huge>:
// ASM: calls
// ASM: R_C166_SEG24{{[[:space:]]+}}_huge_external
// ASM: ret
// ASM-LABEL: <_call_huge_indirect>:
// ASM: calla
// ASM: R_C166_COF16{{[[:space:]]+}}__icall
// ASM: ret
