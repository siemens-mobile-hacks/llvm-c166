; RUN: llc -mtriple=c166-none-elf -code-model=large -O2 -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

define i16 @add_sign_extended_byte(ptr addrspace(2) %source, i16 %bias) {
; CHECK-LABEL: add_sign_extended_byte:
; CHECK:       extp
; CHECK-NEXT:  movb
; CHECK-NEXT:  movbs r4,
; CHECK-NEXT:  add r4, r14
; CHECK-NEXT:  rets
  %byte = load i8, ptr addrspace(2) %source, align 1
  %extended = sext i8 %byte to i16
  %result = add i16 %bias, %extended
  ret i16 %result
}

define i16 @and_zero_extended_byte(ptr addrspace(2) %source, i16 %mask) {
; CHECK-LABEL: and_zero_extended_byte:
; CHECK:       extp
; CHECK-NEXT:  movb
; CHECK-NEXT:  movbz r4,
; CHECK-NEXT:  and r4, r14
; CHECK-NEXT:  rets
  %byte = load i8, ptr addrspace(2) %source, align 1
  %extended = zext i8 %byte to i16
  %result = and i16 %mask, %extended
  ret i16 %result
}
