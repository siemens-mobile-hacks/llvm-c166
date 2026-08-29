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
