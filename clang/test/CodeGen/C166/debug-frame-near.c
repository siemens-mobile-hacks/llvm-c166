// RUN: %clang_cc1 -triple c166-none-elf -O1 -debug-info-kind=limited \
// RUN:   -main-file-name debug-frame-near.c -dwarf-version=5 -emit-obj %s -o %t.o
// RUN: llvm-dwarfdump --verify %t.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: llvm-dwarfdump --debug-frame %t.o | FileCheck %s --check-prefix=FRAME

// A near call pushes only IP on the system stack. Its FDE must
// override the default Large CIE, which describes the CSP:IP pair pushed by
// CALLS, and reconstruct the ABI's 32-bit virtual return-address register.

typedef unsigned int u16;

u16 __attribute__((c166_near)) debug_near_leaf(u16 value) {
  return value + 1U;
}

// Force a user-stack frame as well, so the test proves that the near
// system-stack rules and the ordinary R0 frame rule coexist.
u16 __attribute__((c166_near)) debug_near_frame(u16 value) {
  volatile u16 slot = value;
  return slot + 2U;
}

// VERIFY: No errors.

// FRAME: Return address column: 301
// FRAME: DW_CFA_def_cfa: SP +4
// FRAME: FDE
// FRAME-NEXT: Format:       DWARF32
// FRAME: DW_CFA_def_cfa: SP +2
// FRAME-NEXT: DW_CFA_val_expression: RA DW_OP_bregx CSP+0, DW_OP_lit16, DW_OP_shl, DW_OP_call_frame_cfa, DW_OP_lit2, DW_OP_minus, DW_OP_deref_size 0x2, DW_OP_or
// FRAME-NEXT: DW_CFA_same_value: CSP
// FRAME-NEXT: DW_CFA_val_offset: SP 0
// FRAME: CFA=SP+2:
// FRAME: FDE
// FRAME-NEXT: Format:       DWARF32
// FRAME: DW_CFA_def_cfa: SP +2
// FRAME-NEXT: DW_CFA_val_expression: RA DW_OP_bregx CSP+0, DW_OP_lit16, DW_OP_shl, DW_OP_call_frame_cfa, DW_OP_lit2, DW_OP_minus, DW_OP_deref_size 0x2, DW_OP_or
// FRAME-NEXT: DW_CFA_same_value: CSP
// FRAME-NEXT: DW_CFA_val_offset: SP 0
// FRAME: DW_CFA_val_expression: R0 DW_OP_breg0 R0+2
// FRAME: DW_CFA_restore: R0
// FRAME: CFA=SP+2:
