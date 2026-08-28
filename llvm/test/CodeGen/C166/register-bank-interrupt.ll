; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; C166-ABI: calls.interrupt_named_register_bank

@__c166_register_bank_TEST = common addrspace(3) global [16 x i16] zeroinitializer, align 2
@bank_word = external addrspace(2) global i16

define cc 129 void @banked_leaf() #0 {
; CHECK-LABEL: _banked_leaf:
; CHECK:       mov ___c166_register_bank_TEST, r0
; CHECK-NEXT:  scxt cp, #___c166_register_bank_TEST
; CHECK-NEXT:  nop
; CHECK-NOT:   push r{{[0-9]+}}
; CHECK:       pop cp
; CHECK-NEXT:  reti
  %value = load volatile i16, ptr addrspace(2) @bank_word, align 2
  %next = add i16 %value, 1
  store volatile i16 %next, ptr addrspace(2) @bank_word, align 2
  ret void
}

declare void @ordinary_callee()

define cc 129 void @banked_call() #0 {
; CHECK-LABEL: _banked_call:
; CHECK:       mov ___c166_register_bank_TEST, r0
; CHECK-NEXT:  scxt cp, #___c166_register_bank_TEST
; CHECK-NOT:   push r{{[0-9]+}}
; CHECK-NEXT:  scxt mdc, #16
; CHECK-NEXT:  push dpp0
; CHECK:       push dpp2
; CHECK:       push mdh
; CHECK:       push mdl
; HUGE:        calls seg(_ordinary_callee), sof(_ordinary_callee)
; NEAR:        calla cc_uc, cof(_ordinary_callee)
; CHECK:       pop mdl
; CHECK:       pop mdh
; CHECK:       pop dpp2
; CHECK:       pop dpp0
; CHECK:       pop mdc
; CHECK:       pop cp
; CHECK-NEXT:  reti
  call void @ordinary_callee()
  ret void
}

attributes #0 = { "c166-register-bank"="__c166_register_bank_TEST" }
