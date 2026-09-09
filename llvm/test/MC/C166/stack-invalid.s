; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

push rl0
push #1
push [r0]
pop rh7
pop #1
pop [r0]
scxt rl0, #1
scxt [r0], #1
scxt #1, r0
scxt r0, [r1]
; CHECK-COUNT-10: error:
; CHECK-NOT: error:
