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

define i16 @eq_zero_i32_high_word(i32 %value) {
entry:
  %high = lshr i32 %value, 16
  %condition = icmp eq i32 %high, 0
  %result = zext i1 %condition to i16
  ret i16 %result
}

; CHECK-LABEL: eq_zero_i32_high_word:
; CHECK-NOT:   mov r1
; CHECK:       cmp r13, #0
; CHECK-NEXT:  jmpr cc_eq

define i16 @ne_zero_i32_high_word_branch(i32 %value) {
entry:
  %high = lshr i32 %value, 16
  %condition = icmp ne i32 %high, 0
  br i1 %condition, label %nonzero, label %zero

nonzero:
  ret i16 1

zero:
  ret i16 0
}

; CHECK-LABEL: ne_zero_i32_high_word_branch:
; CHECK-NOT:   mov r1
; CHECK:       cmp r13, #0
; CHECK-NEXT:  jmpr cc_eq

define i16 @eq_zero_i32_low_mask(i32 %value) {
entry:
  %masked = and i32 %value, 128
  %condition = icmp eq i32 %masked, 0
  br i1 %condition, label %zero, label %nonzero

zero:
  ret i16 1

nonzero:
  ret i16 0
}

; CHECK-LABEL: eq_zero_i32_low_mask:
; CHECK:       jb r12.7
; CHECK-NOT:   and

define i16 @eq_zero_i32_low_word(i32 %value) {
entry:
  %masked = and i32 %value, 65535
  %condition = icmp eq i32 %masked, 0
  br i1 %condition, label %zero, label %nonzero

zero:
  ret i16 1

nonzero:
  ret i16 0
}

; CHECK-LABEL: eq_zero_i32_low_word:
; CHECK:       cmp r12, #0
; CHECK-NEXT:  jmpr cc_eq
; CHECK-NOT:   and

define i16 @ne_zero_i32_high_mask(i32 %value) {
entry:
  %masked = and i32 %value, 8388608
  %condition = icmp ne i32 %masked, 0
  br i1 %condition, label %nonzero, label %zero

nonzero:
  ret i16 1

zero:
  ret i16 0
}

; CHECK-LABEL: ne_zero_i32_high_mask:
; CHECK:       jnb r13.7
; CHECK-NOT:   and

define i16 @eq_zero_i16_bit(i16 %value) {
entry:
  %masked = and i16 %value, 8192
  %condition = icmp eq i16 %masked, 0
  br i1 %condition, label %zero, label %nonzero

zero:
  ret i16 1

nonzero:
  ret i16 0
}

; CHECK-LABEL: eq_zero_i16_bit:
; CHECK:       jb r12.13
; CHECK-NOT:   and

define i16 @eq_zero_i32_or(i32 %left, i32 %right) {
entry:
  %combined = or i32 %left, %right
  %condition = icmp eq i32 %combined, 0
  %result = zext i1 %condition to i16
  ret i16 %result
}

; CHECK-LABEL: eq_zero_i32_or:
; CHECK-DAG:   or r12, r14
; CHECK-DAG:   or r13, r15
; CHECK:       mov r1, r12
; CHECK-NEXT:  or r1, r13
; CHECK-NEXT:  jmpr cc_eq
