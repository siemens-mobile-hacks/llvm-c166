; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

define i16 @eq_zero(i16 %value) {
entry:
  %condition = icmp eq i16 %value, 0
  %result = zext i1 %condition to i16
  ret i16 %result
}

; CHECK-LABEL: eq_zero:
; CHECK:       cmp r12, #0
; CHECK-NEXT:  jmpr cc_eq

define i16 @ugt_128(i16 %value) {
entry:
  %condition = icmp ugt i16 %value, 128
  %result = zext i1 %condition to i16
  ret i16 %result
}

; CHECK-LABEL: ugt_128:
; CHECK:       cmp r12, #128
; CHECK-NEXT:  jmpr cc_ugt
; CHECK-NOT:   mov {{r[0-9]+}}, #128

define i16 @eq_zero_i32(i32 %value) {
entry:
  %condition = icmp eq i32 %value, 0
  %result = zext i1 %condition to i16
  ret i16 %result
}

; CHECK-LABEL: eq_zero_i32:
; CHECK:       mov r1, r12
; CHECK-NEXT:  or r1, r13
; CHECK-NEXT:  jmpr cc_eq
; CHECK-NOT:   cmp r12, #0
