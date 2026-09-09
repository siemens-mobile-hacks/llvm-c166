; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
jbc r0.16, 0
; CHECK: error:
jnbs 256.0, 0
; CHECK: error:
jbc r0.0, 256
; CHECK: error:
jnbs r0.0, [r1]
; CHECK: error:
jbc r0, 0
; CHECK: error:
