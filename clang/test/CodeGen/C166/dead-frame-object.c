// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -Os \
// RUN:   -Xclang -fexperimental-max-bitint-width=64 \
// RUN:   -mllvm -verify-machineinstrs -c %s -o %t.large.o
// RUN: llvm-objdump -d %t.large.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=medium -Os \
// RUN:   -Xclang -fexperimental-max-bitint-width=64 \
// RUN:   -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -Os \
// RUN:   -Xclang -fexperimental-max-bitint-width=64 \
// RUN:   -mllvm -verify-machineinstrs -c %s -o %t.small.o

typedef unsigned _BitInt(64) u64;

double bitcast_to_double(unsigned short word0, unsigned short word1,
                         unsigned short word2, unsigned short word3) {
  u64 value = (u64)word0 << 48 | (u64)word1 << 32 | (u64)word2 << 16 | word3;
  return __builtin_bit_cast(double, value);
}

// The temporary used to express a binary64 bit cast is gone after instruction
// selection.  It must not leave an unreferenced eight-byte frame object.
// CHECK-LABEL: <_bitcast_to_double>:
// CHECK-NOT:   sub r0
// CHECK:       rets
