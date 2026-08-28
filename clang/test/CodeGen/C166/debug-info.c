// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=large -O1 -debug-info-kind=limited \
// RUN:   -main-file-name debug-info.c \
// RUN:   -dwarf-version=5 -emit-obj %s -o %t.o
// RUN: not %clang_cc1 -triple c166-none-elf -O1 -funwind-tables=1 \
// RUN:   -emit-obj %s -o %t-unwind.o 2>&1 | FileCheck %s --check-prefix=UNWIND
// RUN: llvm-readobj --sections --relocations %t.o | FileCheck %s --check-prefix=ELF
// RUN: llvm-dwarfdump --verify %t.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: llvm-dwarfdump --debug-info --debug-line %t.o | FileCheck %s --check-prefix=DWARF
// RUN: llvm-dwarfdump --debug-frame %t.o | FileCheck %s --check-prefix=FRAME
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -debug-info-kind=limited -dwarf-version=5 -emit-obj %s -o %t.medium.o
// RUN: llvm-dwarfdump --verify %t.medium.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=small -O1 -debug-info-kind=limited -dwarf-version=5 -emit-obj %s -o %t.small.o
// RUN: llvm-dwarfdump --verify %t.small.o 2>&1 | FileCheck %s --check-prefix=VERIFY

// CALLS/RETS keep the return address on the hardware system stack,
// while R0 addresses the separate user stack.  Debug CFI must therefore use
// SP and the ABI's virtual 32-bit CSP:IP return-address register, not R0.

unsigned int debug_add(unsigned int lhs, unsigned int rhs) {
  unsigned int sum = lhs + rhs;
  return sum;
}

// ELF: Name: .debug_abbrev
// ELF: Name: .debug_info
// ELF: Name: .rela.debug_info
// ELF: Name: .debug_str_offsets
// ELF: Name: .debug_str
// ELF: Name: .debug_addr
// ELF: Name: .rela.debug_addr
// ELF: Name: .debug_frame
// ELF: Name: .rela.debug_frame
// ELF: Name: .debug_line
// ELF: Name: .rela.debug_line
// ELF-NOT: Name: .eh_frame

// VERIFY: No errors.

// UNWIND: error: C166 does not support runtime .eh_frame unwind tables; use -g for DWARF .debug_frame information

// DWARF: DW_TAG_compile_unit
// DWARF: DW_AT_name{{.*}}debug-info.c
// DWARF: DW_TAG_subprogram
// DWARF: DW_AT_frame_base{{.*}}DW_OP_reg0 R0
// DWARF: DW_AT_name{{.*}}debug_add
// DWARF: DW_TAG_formal_parameter
// DWARF: DW_AT_location{{.*}}DW_OP_reg12 R12
// DWARF: DW_AT_name{{.*}}lhs
// DWARF: DW_TAG_formal_parameter
// DWARF: DW_AT_location
// DWARF: DW_OP_reg13 R13
// DWARF: DW_OP_reg4 R4
// DWARF: DW_AT_name{{.*}}rhs
// DWARF: DW_TAG_variable
// DWARF: DW_AT_location
// DWARF: DW_OP_reg4 R4
// DWARF: DW_AT_name{{.*}}sum
// DWARF: debug-info.c

// FRAME: .debug_frame contents:
// FRAME: CIE
// FRAME: Version:               4
// FRAME: Address size:          4
// FRAME: Code alignment factor: 2
// FRAME: Data alignment factor: -2
// FRAME: Return address column:  301
// FRAME: DW_CFA_def_cfa: SP +4
// FRAME: DW_CFA_offset_extended: RA -4
// FRAME: DW_CFA_offset_extended: CSP -2
// FRAME: DW_CFA_val_offset: SP 0
// FRAME: DW_CFA_same_value: R0
// FRAME: CFA=SP+4:
// FRAME-SAME: RA=[CFA-4]
// FRAME: FDE
