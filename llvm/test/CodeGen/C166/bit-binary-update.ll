; RUN: llc -mtriple=c166-none-elf -verify-machineinstrs < %s | FileCheck %s

define i16 @copy_bit(i16 %dst, i16 %src) {
  %cleared = and i16 %dst, -129
  %bit = and i16 %src, 4
  %moved = shl i16 %bit, 5
  %result = or i16 %cleared, %moved
  ret i16 %result
}

; CHECK-LABEL: copy_bit:
; CHECK:       mov [[RESULT:r[0-9]+]], {{r[0-9]+}}
; CHECK-NEXT:  bmov [[RESULT]].7, {{r[0-9]+}}.2
; CHECK-NEXT:  rets

define i16 @copy_inverted_bit(i16 %dst, i16 %src) {
  %cleared = and i16 %dst, -129
  %inverted = xor i16 %src, -1
  %bit = and i16 %inverted, 4
  %moved = shl i16 %bit, 5
  %result = or i16 %cleared, %moved
  ret i16 %result
}

; CHECK-LABEL: copy_inverted_bit:
; CHECK:       mov [[RESULT:r[0-9]+]], {{r[0-9]+}}
; CHECK-NEXT:  bmovn [[RESULT]].7, {{r[0-9]+}}.2
; CHECK-NEXT:  rets

define i16 @and_bit(i16 %dst, i16 %src) {
  %shifted = shl i16 %src, 5
  %mask = or i16 %shifted, -129
  %result = and i16 %dst, %mask
  ret i16 %result
}

; CHECK-LABEL: and_bit:
; CHECK:       mov [[RESULT:r[0-9]+]], {{r[0-9]+}}
; CHECK-NEXT:  band [[RESULT]].7, {{r[0-9]+}}.2
; CHECK-NEXT:  rets

define i16 @or_bit(i16 %dst, i16 %src) {
  %bit = and i16 %src, 4
  %moved = shl i16 %bit, 5
  %result = or i16 %dst, %moved
  ret i16 %result
}

; CHECK-LABEL: or_bit:
; CHECK:       mov [[RESULT:r[0-9]+]], {{r[0-9]+}}
; CHECK-NEXT:  bor [[RESULT]].7, {{r[0-9]+}}.2
; CHECK-NEXT:  rets

define i16 @xor_bit(i16 %dst, i16 %src) {
  %bit = and i16 %src, 4
  %moved = shl i16 %bit, 5
  %result = xor i16 %dst, %moved
  ret i16 %result
}

; CHECK-LABEL: xor_bit:
; CHECK:       mov [[RESULT:r[0-9]+]], {{r[0-9]+}}
; CHECK-NEXT:  bxor [[RESULT]].7, {{r[0-9]+}}.2
; CHECK-NEXT:  rets

; The compact immediate sequence is no larger than BOR.
define i16 @keep_compact_or(i16 %dst, i16 %src) {
  %bit = and i16 %src, 1
  %result = or i16 %dst, %bit
  ret i16 %result
}

; CHECK-LABEL: keep_compact_or:
; CHECK-NOT:   bor
; CHECK:       and {{r[0-9]+}}, #1
; CHECK:       or {{r[0-9]+}}, {{r[0-9]+}}

; The unmasked source contributes more than one bit.
define i16 @keep_full_word_or(i16 %dst, i16 %src) {
  %shifted = shl i16 %src, 5
  %result = or i16 %dst, %shifted
  ret i16 %result
}

; CHECK-LABEL: keep_full_word_or:
; CHECK-NOT:   bor
; CHECK:       shl {{r[0-9]+}}, #5
; CHECK:       or {{r[0-9]+}}, {{r[0-9]+}}

; A bit shifted out of the word cannot reappear after the inverse shift.
define i16 @keep_shifted_out_bit(i16 %dst, i16 %src) {
  %bit = and i16 %src, -32768
  %lost = shl i16 %bit, 1
  %shifted = lshr i16 %lost, 1
  %result = or i16 %dst, %shifted
  ret i16 %result
}

; CHECK-LABEL: keep_shifted_out_bit:
; CHECK-NOT:   bor
; CHECK:       rets
