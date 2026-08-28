// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-readobj --sections --relocations %t.o | FileCheck %s --check-prefix=ELF
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// C166-ABI: pointers.near_xnear_global_placement

typedef unsigned int u16;

typedef u16 __attribute__((c166_near)) near_u16;
typedef u16 __attribute__((c166_xnear)) xnear_u16;

volatile near_u16 near_zero;
volatile near_u16 near_initialized = 0x1234U;
volatile xnear_u16 xnear_zero;
volatile xnear_u16 xnear_initialized = 0x5678U;

const near_u16 near_constant = 0x9abcU;
const xnear_u16 xnear_constant = 0xdef0U;

u16 read_near_zero(void) { return near_zero; }
void write_near_zero(u16 value) { near_zero = value; }
u16 read_xnear_zero(void) { return xnear_zero; }
void write_xnear_zero(u16 value) { xnear_zero = value; }

volatile near_u16 *address_near_zero(void) { return &near_zero; }
volatile xnear_u16 *address_xnear_zero(void) { return &xnear_zero; }

// IR-DAG: @near_zero ={{.*}} addrspace(3) global i16 0
// IR-DAG: @near_initialized ={{.*}} addrspace(3) global i16 4660
// IR-DAG: @xnear_zero ={{.*}} addrspace(4) global i16 0
// IR-DAG: @xnear_initialized ={{.*}} addrspace(4) global i16 22136
// IR-DAG: @near_constant ={{.*}} addrspace(3) constant i16 -25924
// IR-DAG: @xnear_constant ={{.*}} addrspace(4) constant i16 -8464

// ASM-LABEL: _read_near_zero:
// ASM: mov r4, dpp2(_near_zero)
// ASM-LABEL: _write_near_zero:
// ASM: mov dpp2(_near_zero), r12
// ASM-LABEL: _read_xnear_zero:
// ASM: mov r4, dpp1(_xnear_zero)
// ASM-LABEL: _write_xnear_zero:
// ASM: mov dpp1(_xnear_zero), r12
// ASM-LABEL: _address_near_zero:
// ASM: mov r4, #dpp2(_near_zero)
// ASM-LABEL: _address_xnear_zero:
// ASM: mov r4, #dpp1(_xnear_zero)
// ASM: .section .c166.near.data
// ASM: _near_initialized:
// ASM: .section .c166.xnear.data
// ASM: _xnear_initialized:
// ASM: .section .c166.near.rodata
// ASM: _near_constant:
// ASM: .section .c166.xnear.rodata
// ASM: _xnear_constant:
// ASM: .section .c166.near.bss
// ASM: _near_zero:
// ASM: .section .c166.xnear.bss
// ASM: _xnear_zero:

// ELF: Name: .c166.near.data
// ELF: Name: .c166.xnear.data
// ELF: Name: .c166.near.rodata
// ELF: Name: .c166.xnear.rodata
// ELF: Name: .c166.near.bss
// ELF: Name: .c166.xnear.bss
// ELF: R_C166_DPP2_16 _near_zero
// ELF: R_C166_DPP2_16 _near_zero
// ELF: R_C166_DPP1_16 _xnear_zero
// ELF: R_C166_DPP1_16 _xnear_zero
// ELF: R_C166_DPP2_16 _near_zero
// ELF: R_C166_DPP1_16 _xnear_zero
