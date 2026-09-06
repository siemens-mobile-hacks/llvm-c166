; RUN: llc -mtriple=c166 -O2 < %s | FileCheck %s

define i16 @negate_word(i16 %value) {
; CHECK-LABEL: negate_word:
; CHECK:       neg {{r[0-9]+}}
; CHECK-NOT:   sub
; CHECK:       rets
  %result = sub i16 0, %value
  ret i16 %result
}

define i16 @complement_word(i16 %value) {
; CHECK-LABEL: complement_word:
; CHECK:       cpl {{r[0-9]+}}
; CHECK-NOT:   xor
; CHECK:       rets
  %result = xor i16 %value, -1
  ret i16 %result
}

define i32 @complement_long(i32 %value) {
; CHECK-LABEL: complement_long:
; CHECK-COUNT-2: cpl {{r[0-9]+}}
; CHECK-NOT:     xor
; CHECK:         rets
  %result = xor i32 %value, -1
  ret i32 %result
}
