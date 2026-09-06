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
extern void case_six(void);
extern void case_seven(void);
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

void four_way_dispatch(unsigned int value) {
  switch (value) {
  case 0: case_zero(); break;
  case 1: case_one(); break;
  case 2: case_two(); break;
  case 3: case_three(); break;
  default: case_default(); break;
  }
}

void seven_way_dispatch(unsigned int value) {
  switch (value) {
  case 0: case_zero(); break;
  case 1: case_one(); break;
  case 2: case_two(); break;
  case 3: case_three(); break;
  case 4: case_four(); break;
  case 5: case_five(); break;
  case 6: case_six(); break;
  default: case_default(); break;
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
  case 6: case_six(); break;
  case 7: case_seven(); break;
  default: case_default(); break;
  }
}

// Sparse and four-case switches retain comparison trees. Seven cases still
// favor branches when paged table setup is required, but fit a Small-model
// table. Eight cases use 16-bit code-offset tables in every model.
// LARGE-LABEL: <_sparse_switch>:
// LARGE:       cmp
// LARGE:       jmpr
// LARGE-NOT:   jmpi
// LARGE:       rets

// LARGE-LABEL: <_four_way_dispatch>:
// LARGE:       cmp
// LARGE:       jmpr
// LARGE-NOT:   jmpi
// LARGE:       rets

// LARGE-LABEL: <_seven_way_dispatch>:
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
// LARGE:       R_C166_SEG24 _case_zero

// MEDIUM-LABEL: <_four_way_dispatch>:
// MEDIUM:       cmp
// MEDIUM:       jmpr
// MEDIUM-NOT:   jmpi
// MEDIUM:       ret

// MEDIUM-LABEL: <_seven_way_dispatch>:
// MEDIUM:       cmp
// MEDIUM:       jmpr
// MEDIUM-NOT:   jmpi
// MEDIUM:       ret

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

// SMALL-LABEL: <_four_way_dispatch>:
// SMALL:       cmp
// SMALL:       jmpr
// SMALL-NOT:   jmpi
// SMALL:       rets

// SMALL-LABEL: <_seven_way_dispatch>:
// SMALL:       shl
// SMALL:       R_C166_16 .c166.small.rodata
// SMALL:       add
// SMALL-NOT:   extp
// SMALL:       mov
// SMALL:       jmpi

// SMALL-LABEL: <_dense_dispatch>:
// SMALL:       shl
// SMALL:       R_C166_16 .c166.small.rodata
// SMALL:       add
// SMALL-NOT:   extp
// SMALL:       mov
// SMALL:       jmpi
// SMALL:       calls
// SMALL:       R_C166_SEG24 _case_zero

// LARGE-ELF:      .rodata           PROGBITS
// LARGE-ELF-SAME: 000010
// LARGE-ELF:      Relocation section '.rela.rodata'
// LARGE-ELF-COUNT-8: R_C166_SOF16
// LARGE-ELF:      16 OBJECT  LOCAL  DEFAULT {{.*}} __c166_jt.

// MEDIUM-ELF:      .rodata           PROGBITS
// MEDIUM-ELF-SAME: 000010
// MEDIUM-ELF:      Relocation section '.rela.rodata'
// MEDIUM-ELF-COUNT-8: R_C166_16
// MEDIUM-ELF:      16 OBJECT  LOCAL  DEFAULT {{.*}} __c166_jt.

// SMALL-ELF:      .c166.small.rodata PROGBITS
// SMALL-ELF-SAME: 00001e
// SMALL-ELF:      Relocation section '.rela.c166.small.rodata'
// SMALL-ELF-COUNT-15: R_C166_SOF16
// SMALL-ELF-DAG:  14 OBJECT  LOCAL  DEFAULT {{.*}} __c166_jt.
// SMALL-ELF-DAG:  16 OBJECT  LOCAL  DEFAULT {{.*}} __c166_jt.
