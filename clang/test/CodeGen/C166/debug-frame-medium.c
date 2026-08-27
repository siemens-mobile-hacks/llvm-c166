// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 \
// RUN:   -debug-info-kind=limited -main-file-name debug-frame-medium.c \
// RUN:   -dwarf-version=5 -emit-obj %s -o %t.o
// RUN: llvm-readobj --file-headers %t.o | FileCheck %s --check-prefix=ELF
// RUN: ld.lld --section-start=.c166.near.text=0x8000 \
// RUN:   --section-start=.text=0x180000 -e _medium_near %t.o -o %t
// RUN: llvm-dwarfdump --verify %t 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: llvm-dwarfdump --debug-frame %t | FileCheck %s --check-prefix=FRAME

// Medium ordinary functions use the near system-stack frame while an
// explicitly huge function keeps the far CALLS/RETS frame. Verify
// both after the relocations have been resolved by LLD at distinct segments.
// C166-ABI: medium.debug.frame_classes

typedef unsigned int u16;

u16 medium_near(u16 value) {
  volatile u16 slot = value;
  return slot + 1U;
}

u16 __attribute__((c166_huge)) medium_huge(u16 value) {
  volatile u16 slot = value;
  return slot + 2U;
}

// ELF: Flags [ (0x221)
// ELF: EF_C166_CODE_NEAR

// VERIFY: No errors.

// FRAME: Return address column: 301
// FRAME: DW_CFA_def_cfa: SP +4
// FRAME: FDE cie={{.*}} pc=00008000...
// FRAME: DW_CFA_def_cfa: SP +2
// FRAME-NEXT: DW_CFA_val_expression: RA DW_OP_bregx CSP+0, DW_OP_lit16, DW_OP_shl, DW_OP_call_frame_cfa, DW_OP_lit2, DW_OP_minus, DW_OP_deref_size 0x2, DW_OP_or
// FRAME-NEXT: DW_CFA_same_value: CSP
// FRAME-NEXT: DW_CFA_val_offset: SP 0
// FRAME: DW_CFA_val_expression: R0 DW_OP_breg0 R0+2
// FRAME: DW_CFA_restore: R0
// FRAME: FDE cie={{.*}} pc=00180000...
// FRAME-NOT: DW_CFA_def_cfa: SP +2
// FRAME-NOT: DW_CFA_val_expression: RA
// FRAME: DW_CFA_val_expression: R0 DW_OP_breg0 R0+2
// FRAME: DW_CFA_restore: R0
