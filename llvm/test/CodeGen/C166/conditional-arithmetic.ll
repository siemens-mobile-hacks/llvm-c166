; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE

define i16 @adjust_true_arm(i16 %value) {
entry:
  %baseline = add i16 %value, -128
  %condition = icmp ult i16 %baseline, 2048
  %other = add i16 %value, -121
  %result = select i1 %condition, i16 %other, i16 %baseline
  ret i16 %result
}

; CHECK-LABEL: adjust_true_arm:
; CHECK:       add [[RESULT:r[0-9]+]], #65408
; CHECK-NEXT:  cmp [[RESULT]], #2048
; CHECK-NEXT:  jmpr cc_uge
; CHECK:       add [[RESULT]], #7
; HUGE:        rets
; NEAR:        ret

define i16 @adjust_false_arm(i16 %value) {
entry:
  %baseline = add i16 %value, -128
  %condition = icmp ult i16 %baseline, 2048
  %other = add i16 %value, -125
  %result = select i1 %condition, i16 %baseline, i16 %other
  ret i16 %result
}

; CHECK-LABEL: adjust_false_arm:
; CHECK:       add [[RESULT:r[0-9]+]], #65408
; CHECK-NEXT:  cmp [[RESULT]], #2048
; CHECK-NEXT:  jmpr cc_ult
; CHECK:       add [[RESULT]], #3
; HUGE:        rets
; NEAR:        ret

define i16 @adjust_signed_arm(i16 %value) {
entry:
  %baseline = add i16 %value, -128
  %condition = icmp slt i16 %baseline, 2048
  %other = add i16 %value, -124
  %result = select i1 %condition, i16 %other, i16 %baseline
  ret i16 %result
}

; CHECK-LABEL: adjust_signed_arm:
; CHECK:       add [[RESULT:r[0-9]+]], #65408
; CHECK-NEXT:  cmp [[RESULT]], #2048
; CHECK-NEXT:  jmpr cc_sge
; CHECK:       add [[RESULT]], #4
; HUGE:        rets
; NEAR:        ret

define i16 @keep_full_adjustment(i16 %value) {
entry:
  %baseline = add i16 %value, -128
  %condition = icmp ult i16 %baseline, 2048
  %other = add i16 %value, -120
  %result = select i1 %condition, i16 %other, i16 %baseline
  ret i16 %result
}

; CHECK-LABEL: keep_full_adjustment:
; CHECK-NOT:   add {{r[0-9]+}}, #8
; CHECK:       add [[BASE:r[0-9]+]], #65408
; CHECK-NEXT:  cmp [[BASE]], #2048
; CHECK:       add {{r[0-9]+}}, #65416
; CHECK-NOT:   add {{r[0-9]+}}, #8
; HUGE:        rets
; NEAR:        ret
