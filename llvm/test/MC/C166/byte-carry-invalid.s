; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
addcb rl2, #256
; CHECK: error:
subcb rh2, #-129
; CHECK: error:
addcb rl2, [r4]
; CHECK: error:
subcb rh2, [r4+]
; CHECK: error:
addcb rl2, [-r0]
; CHECK: error:
subcb rh2, [r0 + #1]
; CHECK: error:
addcb r4, rh2
; CHECK: error:
subcb rh2, r4
; CHECK: error:
