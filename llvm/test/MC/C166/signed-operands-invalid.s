; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

mov r4, #-32769
; CHECK: error:
; CHECK-NEXT: mov r4, #-32769
scxt r4, #-32769
; CHECK: error:
; CHECK-NEXT: scxt r4, #-32769
mov r4, [r5 - 32769]
; CHECK: error:
; CHECK-NEXT: mov r4, [r5 - 32769]
mov r4, [r5 -]
; CHECK: error:
; CHECK-NEXT: mov r4, [r5 -]
scxt r4, -1
; CHECK: error:
; CHECK-NEXT: scxt r4, -1
