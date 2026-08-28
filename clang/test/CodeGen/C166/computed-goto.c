// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=small -O2 -mllvm -verify-machineinstrs -c %s -o %t-small.o
// RUN: llvm-readobj -r %t-small.o | FileCheck %s --check-prefix=SMALL-RELOC
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=LARGE-RELOC
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=MEDIUM
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O2 -mllvm -verify-machineinstrs -c %s -o %t-medium.o
// RUN: llvm-readobj -r %t-medium.o | FileCheck %s --check-prefix=MEDIUM-RELOC

int computed_goto(unsigned selector) {
  static void *const labels[] = {&&first, &&second};
  goto *labels[selector & 1];

first:
  return 11;
second:
  return 22;
}

// CHECK-LABEL: _computed_goto:
// CHECK: jmpi cc_uc, [r{{[0-9]+}}]
// LARGE-RELOC-COUNT-2: R_C166_32 .text

// MEDIUM-LABEL: _computed_goto:
// MEDIUM: jmpi cc_uc, [r{{[0-9]+}}]
// MEDIUM-RELOC-COUNT-2: R_C166_32 .c166.near.text

// SMALL-RELOC: Section ({{.*}}) .rela.c166.small.rodata {
// SMALL-RELOC-COUNT-2: R_C166_16 .text
