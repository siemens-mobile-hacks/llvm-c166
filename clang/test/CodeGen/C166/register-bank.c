// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=MEDIUM
// RUN: %clang_cc1 -triple c166-none-elf -O1 -debug-info-kind=limited -dwarf-version=5 -emit-obj %s -o %t.o
// RUN: llvm-dwarfdump --verify %t.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// C166-ABI: calls.interrupt_named_register_bank
// C166-ABI: medium.special.named_register_bank

typedef unsigned int u16;

volatile u16 bank_counter;
extern void ordinary_callee(void);

__attribute__((interrupt(-1), c166_register_bank("FAST_BANK")))
void banked_leaf(void) {
  ++bank_counter;
}

__attribute__((c166_register_bank("FAST_BANK"), interrupt(34)))
void banked_call(void) {
  ordinary_callee();
}

// One coalescable near 16-register bank is shared by all handlers with the
// same name, including handlers emitted by different translation units.
// IR-DAG: @__c166_register_bank_FAST_BANK = weak addrspace(3) global [16 x i16] zeroinitializer, section ".c166.regbank", align 2
// IR: @llvm.compiler.used = appending addrspace(2) global [3 x ptr addrspace(2)] [ptr addrspace(2) addrspacecast (ptr addrspace(3) @__c166_register_bank_FAST_BANK to ptr addrspace(2)),
// IR-DAG: define{{.*}}cc129 void @banked_leaf(){{.*}}#[[LEAF:[0-9]+]]
// IR-DAG: define{{.*}}cc129 void @banked_call(){{.*}}#[[CALL:[0-9]+]]
// IR-DAG: attributes #[[LEAF]] = {{.*}}"c166-register-bank"="__c166_register_bank_FAST_BANK"
// IR-DAG: attributes #[[CALL]] = {{.*}}"c166-register-bank"="__c166_register_bank_FAST_BANK"

// ASM-LABEL: _banked_leaf:
// ASM:       mov ___c166_register_bank_FAST_BANK, r0
// ASM-NEXT:  scxt cp, #___c166_register_bank_FAST_BANK
// ASM-NEXT:  nop
// ASM-NOT:   push r{{[0-9]+}}
// ASM:       pop cp
// ASM-NEXT:  reti

// MEDIUM-LABEL: _banked_leaf:
// MEDIUM:       mov ___c166_register_bank_FAST_BANK, r0
// MEDIUM-NEXT:  scxt cp, #___c166_register_bank_FAST_BANK
// MEDIUM:       pop cp
// MEDIUM-NEXT:  reti
// MEDIUM-LABEL: _banked_call:
// MEDIUM:       scxt cp, #___c166_register_bank_FAST_BANK
// MEDIUM:       calla cc_uc, cof(_ordinary_callee)
// MEDIUM:       pop cp
// MEDIUM-NEXT:  reti

// VERIFY: No errors.

// ASM-LABEL: _banked_call:
// ASM:       mov ___c166_register_bank_FAST_BANK, r0
// ASM-NEXT:  scxt cp, #___c166_register_bank_FAST_BANK
// ASM-NOT:   push r{{[0-9]+}}
// ASM-NEXT:  scxt mdc, #16
// ASM-NEXT:  push dpp0
// ASM:       push dpp2
// ASM:       push mdh
// ASM:       push mdl
// ASM:       calls seg(_ordinary_callee), sof(_ordinary_callee)
// ASM:       pop mdl
// ASM:       pop mdh
// ASM:       pop dpp2
// ASM:       pop dpp0
// ASM:       pop mdc
// ASM:       pop cp
// ASM-NEXT:  reti
