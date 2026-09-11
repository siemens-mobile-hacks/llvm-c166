// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=tiny -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=TINY-IR
// RUN: %clang --target=c166-none-elf -mcmodel=huge -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=HUGE-IR
// RUN: %clang --target=c166-none-elf -mcmodel=tiny -E -dM %s | FileCheck %s --check-prefix=TINY-MACRO
// RUN: %clang --target=c166-none-elf -mcmodel=huge -E -dM %s | FileCheck %s --check-prefix=HUGE-MACRO
// RUN: %clang --target=c166-none-elf -mcmodel=tiny -O1 -mllvm -verify-machineinstrs -c %s -o %t.tiny.o
// RUN: %clang --target=c166-none-elf -mcmodel=huge -O1 -mllvm -verify-machineinstrs -c %s -o %t.huge.o
// RUN: %clang --target=c166-none-elf -mcmodel=huge -O0 -mllvm -verify-machineinstrs -c %s -o %t.huge-o0.o
// RUN: llvm-readobj --file-headers --symbols %t.tiny.o | FileCheck %s --check-prefix=TINY-ELF
// RUN: llvm-readobj --file-headers --symbols %t.huge.o | FileCheck %s --check-prefix=HUGE-ELF
// RUN: llvm-objdump -dr %t.tiny.o | FileCheck %s --check-prefix=TINY-ASM
// RUN: llvm-objdump -dr %t.huge.o | FileCheck %s --check-prefix=HUGE-ASM
// RUN: llvm-objdump -dr %t.huge-o0.o | FileCheck %s --check-prefix=HUGE-O0-ASM

typedef unsigned int u16;
typedef unsigned long u32;
typedef u16 default_function(u16 *);

struct pair {
  u16 first;
  u16 second;
};

volatile u16 default_word;
extern u16 external_function(u16 *);
extern void observe_pair(float, struct pair, u16, u16);

_Static_assert(sizeof(u16 *) ==
#if __C166_MEMORY_MODEL__ == 4
                   2
#else
                   4
#endif
               , "default pointer width");
_Static_assert(sizeof(default_function *) ==
#if __C166_MEMORY_MODEL__ == 4
                   2
#else
                   4
#endif
               , "default function pointer width");

u16 read_default(void) { return default_word; }

u16 call_default(u16 *pointer) {
  return external_function(pointer) + default_word;
}

u16 *advance_default(u16 *pointer, u32 count) { return pointer + count; }

void forward_stack_pair(float value, struct pair pair, u16 word, u16 tail) {
  struct pair *volatile address = &pair;
  observe_pair(value, *address, word, tail);
}

// TINY-MACRO: #define __C166_MEMORY_MODEL__ 4
// TINY-MACRO: #define __INTPTR_TYPE__ int
// TINY-MACRO: #define __SIZEOF_POINTER__ 2
// HUGE-MACRO: #define __C166_MEMORY_MODEL__ 5
// HUGE-MACRO: #define __INTPTR_TYPE__ long int
// HUGE-MACRO: #define __SIZEOF_POINTER__ 4
// HUGE-MACRO: #define __SIZE_TYPE__ unsigned int

// TINY-IR: target datalayout = "{{.*}}-P3-G3-A3-{{.*}}"
// TINY-IR: @default_word = {{.*}}addrspace(3) global i16
// TINY-IR-LABEL: define{{.*}} i16 @call_default(ptr addrspace(3)
// TINY-IR-SAME: addrspace(3)
// TINY-IR: call addrspace(3) i16 @external_function(ptr addrspace(3)
// TINY-IR-LABEL: define{{.*}} ptr addrspace(3) @advance_default(ptr addrspace(3)
// TINY-IR-SAME: addrspace(3)

// HUGE-IR: target datalayout = "{{.*}}-P1-G5-A5-{{.*}}"
// HUGE-IR: @default_word = {{.*}}addrspace(5) global i16
// HUGE-IR-LABEL: define{{.*}} i16 @call_default(ptr addrspace(5)
// HUGE-IR-SAME: addrspace(1)
// HUGE-IR: call addrspace(1) i16 @external_function(ptr addrspace(5)
// HUGE-IR-LABEL: define{{.*}} ptr addrspace(5) @advance_default(ptr addrspace(5)
// HUGE-IR-SAME: addrspace(1)

// TINY-ELF: Flags [ (0x211)
// TINY-ELF-NEXT: EF_C166_CODE_NEAR (0x200)
// TINY-ELF-NEXT: EF_C166_CORE_8X166 (0x1)
// TINY-ELF-NEXT: EF_C166_DATA_NEAR (0x10)
// TINY-ELF: Name: _call_default
// TINY-ELF: STO_C166_CODE_NEAR
// HUGE-ELF: Flags [ (0x141)
// HUGE-ELF-NEXT: EF_C166_CODE_HUGE (0x100)
// HUGE-ELF-NEXT: EF_C166_CORE_8X166 (0x1)
// HUGE-ELF-NEXT: EF_C166_DATA_HUGE (0x40)
// HUGE-ELF: Name: _default_word
// HUGE-ELF: STO_C166_DATA_HUGE

// TINY-ASM-LABEL: <_call_default>:
// TINY-ASM: calla
// TINY-ASM: R_C166_COF16{{[[:space:]]+}}_external_function
// TINY-ASM: ret
// HUGE-ASM-LABEL: <_call_default>:
// HUGE-ASM: calls
// HUGE-ASM: R_C166_SEG24{{[[:space:]]+}}_external_function
// HUGE-ASM: exts
// HUGE-ASM: rets

// HUGE-O0-ASM-LABEL: <_forward_stack_pair>:
// HUGE-O0-ASM: exts
// HUGE-O0-ASM-NOT: extp
// HUGE-O0-ASM: calls
