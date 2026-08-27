; RUN: llc -mtriple=c166-none-elf -verify-machineinstrs < %s | FileCheck %s

; DAG combine recognizes this expression as ROTL. C166 has native shifts but
; no rotate instruction, so target lowering must expand the combined node.
define i16 @rotl5(i16 %value) {
; CHECK-LABEL: rotl5:
; CHECK-DAG:   shl
; CHECK-DAG:   shr
; CHECK:       or
; CHECK:       rets
  %left = shl i16 %value, 5
  %right = lshr i16 %value, 11
  %result = or i16 %left, %right
  ret i16 %result
}
