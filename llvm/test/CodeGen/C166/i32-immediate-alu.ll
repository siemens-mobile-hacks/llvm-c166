; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

define i32 @add_wide(i32 %value) {
; CHECK-LABEL: add_wide:
; CHECK-NOT:   mov {{r[0-9]+}}, #22136
; CHECK:       add {{r[0-9]+}}, #22136
; CHECK-NEXT:  addc {{r[0-9]+}}, #4660
  %result = add i32 %value, 305419896
  ret i32 %result
}

define i32 @add_negative(i32 %value) {
; CHECK-LABEL: add_negative:
; CHECK-NOT:   mov {{r[0-9]+}}, #65535
; CHECK:       sub {{r[0-9]+}}, #1
; CHECK-NEXT:  subc {{r[0-9]+}}, #1
  %result = add i32 %value, -65537
  ret i32 %result
}

define i32 @add_low_word(i32 %value) {
; CHECK-LABEL: add_low_word:
; CHECK:       add {{r[0-9]+}}, #65280
; CHECK-NEXT:  addc {{r[0-9]+}}, #0
  %result = add i32 %value, 65280
  ret i32 %result
}

define i32 @xor_wide(i32 %value) {
; CHECK-LABEL: xor_wide:
; CHECK-NOT:   mov {{r[0-9]+}}, #772
; CHECK:       xor {{r[0-9]+}}, #772
; CHECK-NEXT:  xor {{r[0-9]+}}, #258
  %result = xor i32 %value, 16909060
  ret i32 %result
}

define i32 @and_identity_high(i32 %value) {
; CHECK-LABEL: and_identity_high:
; CHECK:       and {{r[0-9]+}}, #255
; CHECK-NOT:   and {{r[0-9]+}}, #65535
  %result = and i32 %value, -65281
  ret i32 %result
}

define i32 @or_low_word(i32 %value) {
; CHECK-LABEL: or_low_word:
; CHECK:       or {{r[0-9]+}}, #4660
; CHECK-NOT:   or {{r[0-9]+}}, #0
  %result = or i32 %value, 4660
  ret i32 %result
}

define i32 @set_high_word_bit(i32 %value) {
; CHECK-LABEL: set_high_word_bit:
; CHECK-NOT:   {{[ \t]or[ \t]}}
; CHECK:       bset {{r[0-9]+}}.6
  %result = or i32 %value, 4194304
  ret i32 %result
}

define i32 @clear_high_word_bit(i32 %value) {
; CHECK-LABEL: clear_high_word_bit:
; CHECK-NOT:   {{[ \t]and[ \t]}}
; CHECK:       bclr {{r[0-9]+}}.15
  %result = and i32 %value, 2147483647
  ret i32 %result
}
