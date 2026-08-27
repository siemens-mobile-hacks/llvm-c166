// RUN: %clang_cc1 -triple c166-none-elf -O1 -debug-info-kind=limited \
// RUN:   -main-file-name debug-frame-code-bank.c -dwarf-version=5 \
// RUN:   -emit-obj %s -o %t.o
// RUN: llvm-dwarfdump --verify %t.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: llvm-dwarfdump --debug-frame %t.o | FileCheck %s --check-prefix=FRAME
// RUN: llvm-objdump -dr %t.o | FileCheck %s --check-prefix=ASM

typedef unsigned int u16;

extern u16 __attribute__((c166_bank(2)))
bank2_debug_target(u16, u16, u16, u16, u16);

u16 __attribute__((c166_bank(1))) debug_bank_cross(u16 value) {
  return bank2_debug_target(value, 2U, 3U, 4U, 5U);
}

// The banked caller is still entered by ordinary CALLS and therefore uses the
// common Large CIE.  __banksw is a separately linked platform function: its
// implementation must carry its own self-contained FDE if a debugger needs to
// unwind while stopped inside the helper.

// VERIFY: No errors.

// ASM-LABEL: <_debug_bank_cross>:
// ASM:       calls
// ASM:       R_C166_SEG8{{[[:space:]]+}}__banksw
// ASM-NEXT:  {{.*}}R_C166_SOF16{{[[:space:]]+}}__banksw

// FRAME: .debug_frame contents:
// FRAME: CIE
// FRAME: Return address column:  301
// FRAME: DW_CFA_def_cfa: SP +4
// FRAME: DW_CFA_offset_extended: RA -4
// FRAME: DW_CFA_offset_extended: CSP -2
// FRAME: FDE
// FRAME: DW_CFA_val_expression: R0 DW_OP_breg0 R0+4
// FRAME: DW_CFA_val_expression: R0 DW_OP_breg0 R0+0
