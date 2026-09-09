; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

einit #1
; CHECK: error:
; CHECK-NEXT: einit #1
idle r0
; CHECK: error:
; CHECK-NEXT: idle r0
pwrdn #0
; CHECK: error:
; CHECK-NEXT: pwrdn #0
srvwdt r1
; CHECK: error:
; CHECK-NEXT: srvwdt r1
srst #0
; CHECK: error:
; CHECK-NEXT: srst #0

diswdt #0
; CHECK: error:
; CHECK-NEXT: diswdt #0
nop r0
; CHECK: error:
; CHECK-NEXT: nop r0
