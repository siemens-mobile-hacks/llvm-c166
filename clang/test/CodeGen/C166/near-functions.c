// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-readobj --sections --relocations %t.o | FileCheck %s --check-prefix=ELF
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// C166-ABI: pointers.near_qualified_functions

typedef unsigned int u16;

extern u16 __attribute__((c166_near)) near_external(u16 a, u16 b);
extern u16 default_external(u16 a, u16 b);

typedef u16 __attribute__((c166_near)) near_function(u16 a, u16 b);
typedef near_function *near_function_pointer;
typedef u16 (*default_function_pointer)(u16 a, u16 b);

_Static_assert(sizeof(near_function_pointer) == 2,
               "near function pointer width");
#if __C166_MEMORY_MODEL__ == 1
_Static_assert(sizeof(default_function_pointer) == 4,
               "Large default function pointer width");
#else
_Static_assert(sizeof(default_function_pointer) == 2,
               "Medium default function pointer width");
#endif

u16 call_near_then_add(u16 a, u16 b) {
  return near_external(a, b) + 1U;
}

u16 __attribute__((c166_near)) near_callee(u16 a, u16 b) {
  return a + b;
}

u16 __attribute__((c166_near)) near_calls_default_then_add(u16 a, u16 b) {
  return default_external(a, b) + 1U;
}

u16 call_near_indirect(near_function_pointer function, u16 a, u16 b) {
  return function(a, b) + 1U;
}

// IR: target datalayout = "{{.*}}p3:16:16{{.*}}"
// IR: declare{{.*}}i16 @near_external(i16{{.*}}, i16{{.*}}){{.*}}addrspace(3)
// IR: define{{.*}}i16 @near_callee(i16{{.*}}, i16{{.*}}){{.*}}addrspace(3)
// IR: define{{.*}}i16 @near_calls_default_then_add(i16{{.*}}, i16{{.*}}){{.*}}addrspace(3)
// IR-LABEL: define{{.*}}i16 @call_near_indirect(ptr addrspace(3){{.*}}%function
// IR: call addrspace(3) i16 %{{.*}}(i16{{.*}}, i16{{.*}})

// ASM-LABEL: _call_near_then_add:
// ASM: calla cc_uc, cof(_near_external)
// ASM: rets
// ASM: .section .c166.near.text
// ASM-LABEL: _near_callee:
// ASM: ret
// ASM-LABEL: _near_calls_default_then_add:
// ASM: calls seg(_default_external), sof(_default_external)
// ASM: ret
// ASM-LABEL: _call_near_indirect:
// ASM: calli cc_uc, [r{{[0-9]+}}]
// ASM: rets

// ELF: Name: .c166.near.text
// ELF: R_C166_COF16 _near_external
// ELF: R_C166_SEG24 _default_external
