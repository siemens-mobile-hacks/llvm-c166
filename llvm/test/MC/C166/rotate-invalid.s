; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
rol r2, #-1
; CHECK: error: immediate must be in the range 0..15
ror r4, #16
; CHECK: error: immediate must be in the range 0..15
rol r2, 8
; CHECK: error:
ror rl2, #8
; CHECK: error:
rol r2, [r0]
; CHECK: error:
