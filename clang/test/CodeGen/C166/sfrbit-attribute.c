// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=CHECK
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=CHECK
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -c %s -o %t.o
// RUN: llvm-nm --undefined-only %t.o | count 0
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O2 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O2 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang_cc1 -triple c166-none-elf -O0 -emit-llvm %s -o - | FileCheck %s --check-prefix=IR

extern unsigned int ien __attribute__((c166_sfrbit(0xff10, 11)));
extern unsigned int ien;
extern unsigned int t7ir __attribute__((c166_esfrbit(0xf17a, 7)));

unsigned int read_ien(void) { return ien; }
unsigned int read_t7ir(void) { return t7ir; }
void set_ien(void) { ien = 1; }
void clear_ien(void) { ien = 0; }
void write_ien(unsigned int value) { ien = value; }
void set_t7ir(void) { t7ir = 1; }
void clear_t7ir(void) { t7ir = 0; }
void write_t7ir(unsigned int value) { t7ir = value; }

// The declaration is represented in IR, but target lowering consumes every
// access before object emission.
// IR: @ien = external {{.*}}global i16, {{.*}}!c166.sfr.bit ![[SFR:[0-9]+]]
// IR: @t7ir = external {{.*}}global i16, {{.*}}!c166.sfr.bit ![[ESFR:[0-9]+]]
// IR: load volatile i16, {{.*}}@ien
// IR: store volatile i16
// IR: ![[SFR]] = !{i16 -240, i16 11, i1 false}
// IR: ![[ESFR]] = !{i16 -3718, i16 7, i1 true}

// CHECK-LABEL: _read_ien:
// CHECK:       mov [[SFR_RESULT:r[0-9]+]], #0
// CHECK-NEXT:  bmov [[SFR_RESULT]].0, psw.11
// CHECK:       rets

// CHECK-LABEL: _read_t7ir:
// CHECK:       mov [[ESFR_RESULT:r[0-9]+]], #0
// CHECK:       extr #1
// CHECK-NEXT:  bmov [[ESFR_RESULT]].0, 189 . 7
// CHECK:       rets

// CHECK-LABEL: _set_ien:
// CHECK:       bset psw.11
// CHECK:       rets

// CHECK-LABEL: _clear_ien:
// CHECK:       bclr psw.11
// CHECK:       rets

// CHECK-LABEL: _write_ien:
// CHECK:       bmov psw.11, {{r[0-9]+}}.0
// CHECK:       rets

// CHECK-LABEL: _set_t7ir:
// CHECK:       extr #1
// CHECK-NEXT:  bset 189 . 7
// CHECK:       rets

// CHECK-LABEL: _clear_t7ir:
// CHECK:       extr #1
// CHECK-NEXT:  bclr 189 . 7
// CHECK:       rets

// CHECK-LABEL: _write_t7ir:
// CHECK:       extr #1
// CHECK-NEXT:  bmov 189 . 7, {{r[0-9]+}}.0
// CHECK:       rets
