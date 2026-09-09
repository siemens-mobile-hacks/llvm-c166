; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

trap #-1
; CHECK: error: immediate must be in the range 0..127
; CHECK-NEXT: trap #-1
trap #128
; CHECK: error: immediate must be in the range 0..127
; CHECK-NEXT: trap #128
trap #256
; CHECK: error: immediate must be in the range 0..127
; CHECK-NEXT: trap #256
trap 7
; CHECK: error: immediate must be in the range 0..127
; CHECK-NEXT: trap 7
trap #external
; CHECK: error: immediate must be in the range 0..127
; CHECK-NEXT: trap #external
trap r4
; CHECK: error:
; CHECK-NEXT: trap r4
trap [r4]
; CHECK: error:
; CHECK-NEXT: trap [r4]
trap
; CHECK: error:
; CHECK-NEXT: trap
