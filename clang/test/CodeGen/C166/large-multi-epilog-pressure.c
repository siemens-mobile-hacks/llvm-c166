// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -stress-regalloc=4 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -stress-regalloc=4 -mllvm -verify-machineinstrs -c %s -o %t-o1.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -stress-regalloc=4 -mllvm -verify-machineinstrs -c %s -o %t-o2.o
// RUN: llvm-objdump -d -r %t-o2.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O3 -mllvm -stress-regalloc=4 -mllvm -verify-machineinstrs -c %s -o %t-o3.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -Os -mllvm -stress-regalloc=4 -mllvm -verify-machineinstrs -c %s -o %t-os.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -Oz -mllvm -stress-regalloc=4 -mllvm -verify-machineinstrs -c %s -o %t-oz.o

typedef unsigned int u16;
typedef unsigned long u32;

extern u16 multi_epilog_selector(u16);

__attribute__((noinline))
u32 multi_epilog_pressure(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5,
                          u16 mode, u16 rounds) {
  if (mode == 0xffffU)
    return a0 ^ a5;

  u32 v0 = a0 + 0x00010001UL;
  u32 v1 = a1 + 0x01020304UL;
  u32 v2 = a2 + 0x10203040UL;
  u32 v3 = a3 + 0x7fff0001UL;
  u32 v4 = a4 + 0x8000ffffUL;
  u32 v5 = a5 + 0xfedcba98UL;
  u16 gate = multi_epilog_selector(mode);

  if (gate == 0U)
    return v0 ^ v5;

  u32 accumulator = v0 + (v1 ^ v5);
  for (u16 index = 0; index < rounds; ++index) {
    u16 step = multi_epilog_selector((gate + index) & 7U);
    u32 selected;

    switch ((index + step) % 6U) {
    case 0: selected = v0; break;
    case 1: selected = v1; break;
    case 2: selected = v2; break;
    case 3: selected = v3; break;
    case 4: selected = v4; break;
    default: selected = v5; break;
    }
    accumulator = ((accumulator << 5) | (accumulator >> 27)) ^ selected;
    accumulator += (u32)step * 0x10001UL + index;
    if (step == 7U)
      return accumulator ^ v4;
  }

  if (gate & 1U)
    return accumulator + v1 + v3;
  if (gate & 2U)
    return accumulator ^ v2 ^ v5;
  return accumulator + v4;
}

// The source contains exits before and after calls, a loop exit and three
// post-loop exits. Branch folding tail-merges them into one physical epilogue;
// every logical exit must therefore reach the same frame restoration.
// CHECK-LABEL: <_multi_epilog_pressure>:
// CHECK:       mov r1, #[[FRAME:[0-9]+]]
// CHECK-NEXT:  sub r0, r1
// CHECK:       calls
// CHECK:       R_C166_SOF16 _multi_epilog_selector
// CHECK:       calls
// CHECK:       R_C166_SOF16 _multi_epilog_selector
// CHECK:       jmpr
// CHECK:       mov r1, #[[FRAME]]
// CHECK-NEXT:  add r0, r1
// CHECK-NEXT:  rets
