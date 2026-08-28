// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=large -O1 -debug-info-kind=limited \
// RUN:   -main-file-name debug-frame.c -dwarf-version=5 -emit-obj %s -o %t.o
// RUN: llvm-dwarfdump --verify %t.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: llvm-dwarfdump --debug-frame %t.o | FileCheck %s --check-prefix=FRAME
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -debug-info-kind=limited -dwarf-version=5 -emit-obj %s -o %t.medium.o
// RUN: llvm-dwarfdump --verify %t.medium.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=small -O1 -debug-info-kind=limited -dwarf-version=5 -emit-obj %s -o %t.small.o
// RUN: llvm-dwarfdump --verify %t.small.o 2>&1 | FileCheck %s --check-prefix=VERIFY

// Exercise both C166 stacks in one FDE.  CALLS/RETS use the hardware system
// stack, while fixed frames, saved register variables, and the fifth argument
// use the DPP1:R0 user stack.

extern unsigned int debug_sink(unsigned int, unsigned int, unsigned int,
                               unsigned int, unsigned int);

unsigned int debug_frame(unsigned int a, unsigned int b, unsigned int c,
                         unsigned int d) {
  unsigned int left = a + b;
  unsigned int right = c ^ d;
  return left + right + debug_sink(a, b, c, d, left ^ right);
}

// VERIFY: No errors.

// FRAME: Return address column: 301
// FRAME: DW_CFA_def_cfa: SP +4
// FRAME: DW_CFA_offset_extended: RA -4
// FRAME: DW_CFA_offset_extended: CSP -2
// FRAME: DW_CFA_val_offset: SP 0
// FRAME: DW_CFA_same_value: R0
// FRAME: DW_CFA_same_value: R6
// FRAME: DW_CFA_same_value: R7
// FRAME: FDE
// FRAME: DW_CFA_val_expression: R0 DW_OP_breg0 R0+4
// FRAME: DW_CFA_expression: R6 DW_OP_bregx DPP1+0, DW_OP_lit14, DW_OP_shl, DW_OP_breg0 R0+2, DW_OP_constu 0x3fff, DW_OP_and, DW_OP_or
// FRAME: DW_CFA_expression: R7 DW_OP_bregx DPP1+0, DW_OP_lit14, DW_OP_shl, DW_OP_breg0 R0+0, DW_OP_constu 0x3fff, DW_OP_and, DW_OP_or
// FRAME: DW_CFA_val_expression: R0 DW_OP_breg0 R0+6
// FRAME: DW_CFA_expression: R6 DW_OP_bregx DPP1+0, DW_OP_lit14, DW_OP_shl, DW_OP_breg0 R0+4, DW_OP_constu 0x3fff, DW_OP_and, DW_OP_or
// FRAME: DW_CFA_expression: R7 DW_OP_bregx DPP1+0, DW_OP_lit14, DW_OP_shl, DW_OP_breg0 R0+2, DW_OP_constu 0x3fff, DW_OP_and, DW_OP_or
// FRAME: DW_CFA_val_expression: R0 DW_OP_breg0 R0+4
// FRAME: DW_CFA_expression: R6 DW_OP_bregx DPP1+0, DW_OP_lit14, DW_OP_shl, DW_OP_breg0 R0+2, DW_OP_constu 0x3fff, DW_OP_and, DW_OP_or
// FRAME: DW_CFA_expression: R7 DW_OP_bregx DPP1+0, DW_OP_lit14, DW_OP_shl, DW_OP_breg0 R0+0, DW_OP_constu 0x3fff, DW_OP_and, DW_OP_or
// FRAME: DW_CFA_restore: R6
// FRAME: DW_CFA_restore: R7
// FRAME: DW_CFA_restore: R0
