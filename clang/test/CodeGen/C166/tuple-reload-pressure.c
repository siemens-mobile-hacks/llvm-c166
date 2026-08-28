// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -stress-regalloc=3 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -stress-regalloc=3 -mllvm -verify-machineinstrs -c %s -o %t-o1.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -stress-regalloc=3 -mllvm -verify-machineinstrs -c %s -o %t-o2.o
// RUN: llvm-objdump -d %t-o2.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O3 -mllvm -stress-regalloc=3 -mllvm -verify-machineinstrs -c %s -o %t-o3.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -Os -mllvm -stress-regalloc=3 -mllvm -verify-machineinstrs -c %s -o %t-os.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -Oz -mllvm -stress-regalloc=3 -mllvm -verify-machineinstrs -c %s -o %t-oz.o

typedef unsigned int u16;
typedef unsigned long u32;

extern u16 tuple_reload_selector(u16);

#define DEFINE_TUPLE_RELOAD(name, word)                                     \
  __attribute__((noinline)) u16 name(u32 a0, u32 a1, u32 a2, u32 a3,       \
                                      u32 a4, u32 a5, u16 selector) {        \
    u32 v0 = a0 + 0x00010001UL;                                             \
    u32 v1 = a1 + 0x01020304UL;                                             \
    u32 v2 = a2 + 0x10203040UL;                                             \
    u32 v3 = a3 + 0x7fff0001UL;                                             \
    u32 v4 = a4 + 0x8000ffffUL;                                             \
    u32 v5 = a5 + 0xfedcba98UL;                                             \
    switch (tuple_reload_selector(selector)) {                              \
    case 0: return (u16)(v0 word);                                          \
    case 1: return (u16)(v1 word);                                          \
    case 2: return (u16)(v2 word);                                          \
    case 3: return (u16)(v3 word);                                          \
    case 4: return (u16)(v4 word);                                          \
    default: return (u16)(v5 word);                                         \
    }                                                                       \
  }

DEFINE_TUPLE_RELOAD(tuple_reload_low, )
DEFINE_TUPLE_RELOAD(tuple_reload_high, >> 16)

// Generic RA requests whole GR32 reloads (SubReg=0).  Under constrained
// allocation the pair is therefore stored and loaded as two adjacent words;
// the selected low or high word is consumed only after the reload.
// CHECK-LABEL: <_tuple_reload_low>:
// CHECK:       sub r0
// CHECK:       mov [r0 + #{{[0-9]+}}], r14
// CHECK-NEXT:  mov [r0 + #{{[0-9]+}}], r15
// CHECK:       calls
// CHECK:       mov r2, [r0 + #{{[0-9]+}}]
// CHECK-NEXT:  mov r3, [r0 + #{{[0-9]+}}]
// CHECK:       rets

// CHECK-LABEL: <_tuple_reload_high>:
// CHECK:       sub r0
// CHECK:       mov [r0 + #{{[0-9]+}}], r14
// CHECK-NEXT:  mov [r0 + #{{[0-9]+}}], r15
// CHECK:       calls
// CHECK:       mov r3, [r0 + #{{[0-9]+}}]
// CHECK-NEXT:  mov r4, [r0 + #{{[0-9]+}}]
// CHECK:       rets
