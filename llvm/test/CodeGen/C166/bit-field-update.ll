; RUN: llc -mtriple=c166 -O2 < %s | FileCheck %s

define i16 @set_low_bit(i16 %value) {
; CHECK-LABEL: set_low_bit:
; CHECK:       movbz {{r[0-9]+}}, {{rl[0-7]}}
; CHECK-NEXT:  bset {{r[0-9]+}}.7
  %kept = and i16 %value, 127
  %result = or i16 %kept, 128
  ret i16 %result
}

define i16 @set_low_field(i16 %value) {
; CHECK-LABEL: set_low_field:
; CHECK:       bfldl {{r[0-9]+}}, #252, #44
  %kept = and i16 %value, -253
  %result = or i16 %kept, 44
  ret i16 %result
}

define i16 @set_high_field(i16 %value) {
; CHECK-LABEL: set_high_field:
; CHECK:       bfldh {{r[0-9]+}}, #255, #18
  %kept = and i16 %value, 255
  %result = or i16 %kept, 4608
  ret i16 %result
}

define i16 @update_both_bytes(i16 %value) {
; CHECK-LABEL: update_both_bytes:
; CHECK-NOT:   bfld
; CHECK:       rets
  %kept = and i16 %value, 3855
  %result = or i16 %kept, 4112
  ret i16 %result
}

define i16 @shared_mask(i16 %value, ptr %output) {
; CHECK-LABEL: shared_mask:
; CHECK-NOT:   bfld
; CHECK:       rets
  %kept = and i16 %value, 15
  store i16 %kept, ptr %output
  %result = or i16 %kept, 16
  ret i16 %result
}
