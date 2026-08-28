// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=large -O1 -debug-info-kind=limited \
// RUN:   -main-file-name debug-register-pieces.c -dwarf-version=5 \
// RUN:   -emit-obj %s -o %t.o
// RUN: llvm-dwarfdump --verify %t.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: llvm-dwarfdump --debug-info %t.o | FileCheck %s --check-prefix=INFO
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -debug-info-kind=limited -dwarf-version=5 -emit-obj %s -o %t.medium.o
// RUN: llvm-dwarfdump --verify %t.medium.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=small -O1 -debug-info-kind=limited -dwarf-version=5 -emit-obj %s -o %t.small.o
// RUN: llvm-dwarfdump --verify %t.small.o 2>&1 | FileCheck %s --check-prefix=VERIFY

typedef unsigned long u32;

extern void debug_wide_sink(u32);

u32 debug_wide_registers(u32 value) {
  u32 local = value + 1UL;
  debug_wide_sink(local);
  return local;
}

// VERIFY: No errors.

// The C166 ELF-DWARF ABI numbers only R0-R15. LLVM's internal 32-bit
// allocation pairs must therefore be emitted as two 16-bit register pieces,
// never as invented pair registers such as R13R12 or R7R6.
// INFO: DW_AT_location
// INFO-NEXT: {{.*}}DW_OP_reg12 R12, DW_OP_piece 0x2, DW_OP_reg13 R13, DW_OP_piece 0x2
// INFO-NEXT: {{.*}}DW_OP_reg6 R6, DW_OP_piece 0x2, DW_OP_reg7 R7, DW_OP_piece 0x2
// INFO: DW_AT_location
// INFO-NEXT: {{.*}}DW_OP_reg6 R6, DW_OP_piece 0x2, DW_OP_reg7 R7, DW_OP_piece 0x2
// INFO-NOT: DW_OP_regx R13R12
// INFO-NOT: DW_OP_regx R7R6
