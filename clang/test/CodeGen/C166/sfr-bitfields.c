// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=O0
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.o1.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -Os -mllvm -verify-machineinstrs -c %s -o %t.os.o
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O2 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O2 -mllvm -verify-machineinstrs -c %s -o %t.small.o

typedef unsigned short u16;

typedef union {
  u16 value;
  struct {
    unsigned low : 11;
    unsigned ien : 1;
    unsigned high : 4;
  };
} psw_type;

#define PSW (*(volatile __sfr psw_type *)0xff10U)
#define ESFR_TEST (*(volatile __esfr psw_type *)0xf112U)
#define LOWER_SFR_TEST (*(volatile __sfr psw_type *)0xfe10U)
#define LOWER_ESFR_TEST (*(volatile __esfr psw_type *)0xf010U)

u16 read_psw(void) { return PSW.value; }
void write_psw(u16 value) { PSW.value = value; }

void set_ien(void) { PSW.ien = 1; }
void clear_ien(void) { PSW.ien = 0; }
void write_ien(u16 value) { PSW.ien = value; }

void set_esfr_bit(void) { ESFR_TEST.ien = 1; }
void clear_esfr_bit(void) { ESFR_TEST.ien = 0; }
void write_esfr_bit(u16 value) { ESFR_TEST.ien = value; }
void write_esfr_computed(u16 lhs, u16 rhs) { ESFR_TEST.ien = lhs + rhs; }
void write_lower_sfr_bit(void) { LOWER_SFR_TEST.ien = 1; }
void write_lower_esfr_bit(void) { LOWER_ESFR_TEST.ien = 1; }
void manual_volatile_rmw(void) {
  volatile __sfr u16 *reg = (volatile __sfr u16 *)0xff10U;
  u16 value = *reg;
  *reg = value | 0x0800U;
}

// CHECK-LABEL: _read_psw:
// CHECK:       mov r4, -240
// CHECK-NEXT:  rets

// CHECK-LABEL: _write_psw:
// CHECK:       mov -240, r12
// CHECK-NEXT:  rets

// CHECK-LABEL: _set_ien:
// CHECK:       bset psw.11
// CHECK-NEXT:  rets

// CHECK-LABEL: _clear_ien:
// CHECK:       bclr psw.11
// CHECK-NEXT:  rets

// CHECK-LABEL: _write_ien:
// CHECK:       bmov psw.11, r12.0
// CHECK-NEXT:  rets

// CHECK-LABEL: _set_esfr_bit:
// CHECK:       extr #1
// CHECK-NEXT:  bset 137 . 11
// CHECK-NEXT:  rets

// CHECK-LABEL: _clear_esfr_bit:
// CHECK:       extr #1
// CHECK-NEXT:  bclr 137 . 11
// CHECK-NEXT:  rets

// CHECK-LABEL: _write_esfr_bit:
// CHECK:       extr #1
// CHECK-NEXT:  bmov 137 . 11, r12.0
// CHECK-NEXT:  rets

// CHECK-LABEL: _write_esfr_computed:
// CHECK:       add
// CHECK:       extr #1
// CHECK-NEXT:  bmov 137 . 11, {{r[0-9]+}}.0
// CHECK-NEXT:  rets

// The lower half of each register space aliases bit-addressable RAM in bit
// instructions, so it must retain a word read-modify-write sequence.
// CHECK-LABEL: _write_lower_sfr_bit:
// CHECK-NOT:   bset 8 . 11
// CHECK:       rets

// CHECK-LABEL: _write_lower_esfr_bit:
// CHECK-NOT:   extr
// CHECK:       rets

// Explicit volatile C accesses are not bit-field operations and must not be
// folded into a single hardware access.
// CHECK-LABEL: _manual_volatile_rmw:
// CHECK-NOT:   bset psw.11
// CHECK:       rets

// At -O0 arguments may be spilled, but bit-field stores must still use direct
// bit instructions and keep EXTR adjacent to an ESFR access.
// O0-LABEL: _set_ien:
// O0:       bset psw.11
// O0-LABEL: _clear_ien:
// O0:       bclr psw.11
// O0-LABEL: _write_ien:
// O0:       bmov psw.11, {{r[0-9]+}}.0
// O0-LABEL: _set_esfr_bit:
// O0:       extr #1
// O0-NEXT:  bset 137 . 11
// O0-LABEL: _clear_esfr_bit:
// O0:       extr #1
// O0-NEXT:  bclr 137 . 11
// O0-LABEL: _write_esfr_bit:
// O0:       extr #1
// O0-NEXT:  bmov 137 . 11, {{r[0-9]+}}.0
// O0-LABEL: _manual_volatile_rmw:
// O0-NOT:   bset psw.11
// O0:       rets
