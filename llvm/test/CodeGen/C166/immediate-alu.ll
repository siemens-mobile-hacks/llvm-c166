; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

define i16 @add_large(i16 %value) {
  %result = add i16 %value, 4660
  ret i16 %result
}

; CHECK-LABEL: add_large:
; CHECK-NOT:   mov {{r[0-9]+}}, #4660
; CHECK:       add {{r[0-9]+}}, #4660

define i16 @sub_large(i16 %value) {
  %result = sub i16 %value, 128
  ret i16 %result
}

; CHECK-LABEL: sub_large:
; CHECK-NOT:   mov {{r[0-9]+}}, #128
; CHECK-NOT:   mov {{r[0-9]+}}, #65408
; CHECK:       add {{r[0-9]+}}, #65408

define i16 @and_small(i16 %value) {
  %result = and i16 %value, 3
  ret i16 %result
}

; CHECK-LABEL: and_small:
; CHECK:       and {{r[0-9]+}}, #3

define i16 @and_large(i16 %value) {
  %result = and i16 %value, 16383
  ret i16 %result
}

; CHECK-LABEL: and_large:
; CHECK-NOT:   mov {{r[0-9]+}}, #16383
; CHECK:       and {{r[0-9]+}}, #16383

define i16 @mask_low_byte(i16 %value) {
  %result = and i16 %value, 255
  ret i16 %result
}

; CHECK-LABEL: mask_low_byte:
; CHECK-NOT:   and
; CHECK:       movbz {{r[0-9]+}}, rl{{[0-7]}}

define i16 @or_large(i16 %value) {
  %result = or i16 %value, 4660
  ret i16 %result
}

; CHECK-LABEL: or_large:
; CHECK-NOT:   mov {{r[0-9]+}}, #4660
; CHECK:       or {{r[0-9]+}}, #4660

define i16 @xor_large(i16 %value) {
  %result = xor i16 %value, 4660
  ret i16 %result
}

; CHECK-LABEL: xor_large:
; CHECK-NOT:   mov {{r[0-9]+}}, #4660
; CHECK:       xor {{r[0-9]+}}, #4660

define i16 @set_register_bit(i16 %value) {
  %result = or i16 %value, 1024
  ret i16 %result
}

; CHECK-LABEL: set_register_bit:
; CHECK-NOT:   or
; CHECK:       bset {{r[0-9]+}}.10

define i16 @clear_register_bit(i16 %value) {
  %result = and i16 %value, 32767
  ret i16 %result
}

; CHECK-LABEL: clear_register_bit:
; CHECK-NOT:   and
; CHECK:       bclr {{r[0-9]+}}.15

define i16 @keep_multi_bit_clear(i16 %value) {
  %result = and i16 %value, 127
  ret i16 %result
}

; CHECK-LABEL: keep_multi_bit_clear:
; CHECK-NOT:   bclr
; CHECK:       and {{r[0-9]+}}, #127

; Values covered by the compact three-bit immediate encoding remain ordinary
; OR instructions because BSET would not be smaller.
define i16 @keep_compact_or(i16 %value) {
  %result = or i16 %value, 4
  ret i16 %result
}

; CHECK-LABEL: keep_compact_or:
; CHECK:       or {{r[0-9]+}}, #4

; BCLR's flags describe the old bit rather than the resulting word.  Keep the
; explicit compare when the updated value is tested as a whole.
define i16 @clear_register_bit_and_test(i16 %value) {
  %cleared = and i16 %value, 32767
  %zero = icmp eq i16 %cleared, 0
  br i1 %zero, label %yes, label %no

yes:
  ret i16 1

no:
  ret i16 0
}

; CHECK-LABEL: clear_register_bit_and_test:
; CHECK:       bclr [[VALUE:r[0-9]+]].15
; CHECK-NEXT:  cmp [[VALUE]], #0
; CHECK-NEXT:  jmpr cc_eq

define i16 @shift_mask_sign_bit(i16 %value) {
  %shifted = lshr i16 %value, 4
  %result = and i16 %shifted, 2047
  ret i16 %result
}

; CHECK-LABEL: shift_mask_sign_bit:
; CHECK-NOT:   and
; CHECK:       bclr [[VALUE:r[0-9]+]].15
; CHECK-NEXT:  shr [[VALUE]], #4

define i16 @shift_mask_sign_bit_byte(i16 %value) {
  %shifted = lshr i16 %value, 8
  %result = and i16 %shifted, 127
  ret i16 %result
}

; CHECK-LABEL: shift_mask_sign_bit_byte:
; CHECK-NOT:   and
; CHECK:       bclr [[VALUE:r[0-9]+]].15
; CHECK-NEXT:  shr [[VALUE]], #8

; The compact three-bit AND is the same size as BCLR, so keep the canonical
; shift-and sequence.
define i16 @keep_equal_cost_shift_mask(i16 %value) {
  %shifted = lshr i16 %value, 12
  %result = and i16 %shifted, 7
  ret i16 %result
}

; CHECK-LABEL: keep_equal_cost_shift_mask:
; CHECK-NOT:   bclr
; CHECK:       shr [[VALUE:r[0-9]+]], #12
; CHECK-NEXT:  and [[VALUE]], #7

; Two compact shifts are smaller than a compact shift followed by a full
; immediate AND.
define i16 @shift_multi_bit_mask(i16 %value) {
  %shifted = lshr i16 %value, 4
  %result = and i16 %shifted, 1023
  ret i16 %result
}

; CHECK-LABEL: shift_multi_bit_mask:
; CHECK-NOT:   bclr
; CHECK-NOT:   and
; CHECK:       shl [[VALUE:r[0-9]+]], #2
; CHECK-NEXT:  shr [[VALUE]], #6

define i16 @clear_shared_shift_known_bit(i16 %value, ptr %output) {
  %shifted = lshr i16 %value, 4
  store volatile i16 %shifted, ptr %output
  %result = and i16 %shifted, 2047
  ret i16 %result
}

; CHECK-LABEL: clear_shared_shift_known_bit:
; CHECK:       shr [[VALUE:r[0-9]+]], #4
; CHECK-NOT:   and
; CHECK:       mov [{{r[0-9]+}}], [[VALUE]]
; CHECK-NEXT:  bclr [[VALUE]].11
