; RUN: llc -mtriple=c166-none-elf -verify-machineinstrs < %s | FileCheck %s
; C166-ABI: interrupt

; A C166 interrupt is entered through a six-byte hardware frame and must
; return with RETI.  Registers actually clobbered by the handler are preserved
; on the system stack with PUSH/POP.

define cc 129 void @empty_interrupt() {
; CHECK-LABEL: _empty_interrupt:
; CHECK:       reti
  ret void
}

@interrupt_word = external addrspace(2) global i16

define cc 129 void @leaf_interrupt() {
; CHECK-LABEL: _leaf_interrupt:
; CHECK:       push [[REG:r[1-9][0-5]?]]
; CHECK:       {{mov|add}}
; CHECK:       pop [[REG]]
; CHECK-NEXT:  reti
  %value = load volatile i16, ptr addrspace(2) @interrupt_word, align 2
  %next = add i16 %value, 1
  store volatile i16 %next, ptr addrspace(2) @interrupt_word, align 2
  ret void
}

declare void @ordinary_callee()

define cc 129 void @calling_interrupt() {
; CHECK-LABEL: _calling_interrupt:
; CHECK:       push r12
; CHECK:       push r13
; CHECK:       push r14
; CHECK:       push r15
; CHECK:       scxt mdc, #16
; CHECK:       calls seg(_ordinary_callee), sof(_ordinary_callee)
; CHECK:       pop r15
; CHECK:       pop r14
; CHECK:       pop r13
; CHECK:       pop r12
; CHECK:       reti
  call void @ordinary_callee()
  ret void
}
