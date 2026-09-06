; RUN: llc -mtriple=c166-none-elf -verify-machineinstrs < %s | FileCheck %s

define i16 @bit_is_clear(i16 %value) {
  %masked = and i16 %value, -32768
  %condition = icmp eq i16 %masked, 0
  %result = zext i1 %condition to i16
  ret i16 %result
}

; CHECK-LABEL: bit_is_clear:
; CHECK:       mov [[RESULT:r[0-9]+]], #0
; CHECK-NEXT:  bmovn [[RESULT]].0, {{r[0-9]+}}.15
; CHECK-NEXT:  rets

define i16 @middle_bit_is_clear(i16 %value) {
  %masked = and i16 %value, 32
  %condition = icmp eq i16 %masked, 0
  %result = zext i1 %condition to i16
  ret i16 %result
}

; CHECK-LABEL: middle_bit_is_clear:
; CHECK:       mov [[RESULT:r[0-9]+]], #0
; CHECK-NEXT:  bmovn [[RESULT]].0, {{r[0-9]+}}.5
; CHECK-NEXT:  rets

define i16 @inverted_select(i16 %value) {
  %masked = and i16 %value, 64
  %condition = icmp ne i16 %masked, 0
  %result = select i1 %condition, i16 0, i16 1
  ret i16 %result
}

; CHECK-LABEL: inverted_select:
; CHECK:       mov [[RESULT:r[0-9]+]], #0
; CHECK-NEXT:  bmovn [[RESULT]].0, {{r[0-9]+}}.6
; CHECK-NEXT:  rets

; Extracting the high bit without inversion is already smaller as a shift.
define i16 @bit_is_set(i16 %value) {
  %masked = and i16 %value, -32768
  %condition = icmp ne i16 %masked, 0
  %result = zext i1 %condition to i16
  ret i16 %result
}

; CHECK-LABEL: bit_is_set:
; CHECK-NOT:   bmov
; CHECK:       shr {{r[0-9]+}}, #15
; CHECK-NEXT:  rets

; Selecting arbitrary values still needs the normal conditional select.
define i16 @select_words(i16 %value, i16 %yes, i16 %no) {
  %masked = and i16 %value, -32768
  %condition = icmp ne i16 %masked, 0
  %result = select i1 %condition, i16 %yes, i16 %no
  ret i16 %result
}

; CHECK-LABEL: select_words:
; CHECK-NOT:   bmovn
; CHECK:       jmpr
; CHECK:       rets
