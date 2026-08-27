// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=MEDIUM
// RUN: %clang_cc1 -triple c166-none-elf -O1 -debug-info-kind=limited -dwarf-version=5 -emit-obj %s -o %t.o
// RUN: llvm-dwarfdump --verify %t.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: llvm-dwarfdump -v --debug-info --debug-frame %t.o | FileCheck %s --check-prefix=DWARF
// C166-ABI: interrupt
// C166-ABI: medium.special.interrupt

typedef unsigned int u16;

volatile u16 interrupt_counter;
extern void ordinary_callee(void);

__attribute__((interrupt(-1))) void empty_interrupt(void) {}

__attribute__((interrupt(34))) void leaf_interrupt(void) {
  ++interrupt_counter;
}

__attribute__((interrupt(35))) void calling_interrupt(void) {
  ordinary_callee();
}

// IR-DAG: define{{.*}}cc129 void @empty_interrupt(){{.*}}#[[EMPTY:[0-9]+]]
// IR-DAG: define{{.*}}cc129 void @leaf_interrupt(){{.*}}#[[LEAF:[0-9]+]]
// IR-DAG: define{{.*}}cc129 void @calling_interrupt(){{.*}}#[[CALLING:[0-9]+]]
// IR-DAG: attributes #[[EMPTY]] = {{.*}}noinline{{.*}}"interrupt"="-1"
// IR-DAG: attributes #[[LEAF]] = {{.*}}noinline{{.*}}"interrupt"="34"
// IR-DAG: attributes #[[CALLING]] = {{.*}}noinline{{.*}}"interrupt"="35"

// ASM-LABEL: _empty_interrupt:
// ASM:       reti

// MEDIUM-LABEL: _empty_interrupt:
// MEDIUM:       reti
// MEDIUM-LABEL: _calling_interrupt:
// MEDIUM:       push r12
// MEDIUM:       scxt mdc, #16
// MEDIUM:       calla cc_uc, cof(_ordinary_callee)
// MEDIUM:       pop r12
// MEDIUM:       reti
// ASM-LABEL: _leaf_interrupt:
// ASM:       push {{r[0-9]+}}
// ASM:       pop {{r[0-9]+}}
// ASM-NEXT:  reti
// ASM-LABEL: _calling_interrupt:
// ASM:       push r12
// ASM:       scxt mdc, #16
// ASM:       calls seg(_ordinary_callee), sof(_ordinary_callee)
// ASM:       pop r12
// ASM:       reti

// VERIFY: No errors.
// DWARF: DW_TAG_subprogram
// DWARF-DAG: DW_AT_name{{.*}}empty_interrupt
// DWARF-DAG: DW_AT_calling_convention [DW_FORM_data1] (0x65)
// DWARF: Return address column:  301
// DWARF: DW_CFA_def_cfa: SP +6
// DWARF: DW_CFA_val_expression: RA DW_OP_call_frame_cfa
// DWARF-SAME: DW_OP_lit4
// DWARF-SAME: DW_OP_lit6
// DWARF: DW_CFA_offset_extended: CSP -4
