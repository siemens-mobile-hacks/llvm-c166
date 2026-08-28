// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d -r %t.o | FileCheck %s --check-prefix=LARGE
// RUN: llvm-readelf -S -r -s %t.o | FileCheck %s --check-prefix=LARGE-ELF
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-medium.o
// RUN: llvm-objdump -d -r %t-medium.o | FileCheck %s --check-prefix=MEDIUM
// RUN: llvm-readelf -S -r -s %t-medium.o | FileCheck %s --check-prefix=MEDIUM-ELF
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-small.o
// RUN: llvm-objdump -d -r %t-small.o | FileCheck %s --check-prefix=SMALL
// RUN: llvm-readelf -S -r -s %t-small.o | FileCheck %s --check-prefix=SMALL-ELF
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o0.o

extern void case_zero(void);
extern void case_one(void);
extern void case_two(void);
extern void case_three(void);
extern void case_four(void);
extern void case_five(void);
extern void case_default(void);

unsigned int sparse_switch(int value) {
  switch (value) {
  case -7: return 10;
  case 2: return 20;
  case 19: return 30;
  case 300: return 40;
  default: return 50;
  }
}

void dense_dispatch(unsigned int value) {
  switch (value) {
  case 0: case_zero(); break;
  case 1: case_one(); break;
  case 2: case_two(); break;
  case 3: case_three(); break;
  case 4: case_four(); break;
  case 5: case_five(); break;
  default: case_default(); break;
  }
}

// Sparse switches retain the comparison tree. Dense switches use six 16-bit
// code offsets, matching the native JMPI table representation. Large and
// Medium access the table through a paged data pointer; Small places it in the
// direct LDAT range.
// LARGE-LABEL: <_sparse_switch>:
// LARGE:       cmp
// LARGE:       jmpr
// LARGE-NOT:   jmpi
// LARGE:       rets

// LARGE-LABEL: <_dense_dispatch>:
// LARGE:       shl
// LARGE:       R_C166_POF14 __c166_jt.
// LARGE:       R_C166_PAG10 __c166_jt.
// LARGE:       add
// LARGE:       extp
// LARGE:       mov
// LARGE:       jmpi
// LARGE:       calls
// LARGE:       R_C166_SEG8 _case_zero
// LARGE:       R_C166_SOF16 _case_zero

// MEDIUM-LABEL: <_dense_dispatch>:
// MEDIUM:       shl
// MEDIUM:       R_C166_POF14 __c166_jt.
// MEDIUM:       R_C166_PAG10 __c166_jt.
// MEDIUM:       add
// MEDIUM:       extp
// MEDIUM:       mov
// MEDIUM:       jmpi
// MEDIUM:       calla
// MEDIUM:       R_C166_COF16 _case_zero

// SMALL-LABEL: <_dense_dispatch>:
// SMALL:       shl
// SMALL:       R_C166_16 .c166.small.rodata
// SMALL:       add
// SMALL-NOT:   extp
// SMALL:       mov
// SMALL:       jmpi
// SMALL:       calls
// SMALL:       R_C166_SEG8 _case_zero
// SMALL:       R_C166_SOF16 _case_zero

// LARGE-ELF:      .rodata           PROGBITS
// LARGE-ELF-SAME: 00000c
// LARGE-ELF:      Relocation section '.rela.rodata'
// LARGE-ELF-COUNT-6: R_C166_SOF16
// LARGE-ELF:      12 OBJECT  LOCAL  DEFAULT {{.*}} __c166_jt.

// MEDIUM-ELF:      .rodata           PROGBITS
// MEDIUM-ELF-SAME: 00000c
// MEDIUM-ELF:      Relocation section '.rela.rodata'
// MEDIUM-ELF-COUNT-6: R_C166_16
// MEDIUM-ELF:      12 OBJECT  LOCAL  DEFAULT {{.*}} __c166_jt.

// SMALL-ELF:      .c166.small.rodata PROGBITS
// SMALL-ELF-SAME: 00000c
// SMALL-ELF:      Relocation section '.rela.c166.small.rodata'
// SMALL-ELF-COUNT-6: R_C166_SOF16
// SMALL-ELF:      12 OBJECT  LOCAL  DEFAULT {{.*}} __c166_jt.
