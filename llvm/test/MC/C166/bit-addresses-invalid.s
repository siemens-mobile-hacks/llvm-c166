; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

; The lower SFR half is accessible by word/byte instructions, not bitoff.
bset dpp0.7
; CHECK: error:
; CHECK-NEXT: bset dpp0.7
bclr dpp1.0
; CHECK: error:
; CHECK-NEXT: bclr dpp1.0
bset csp.0
; CHECK: error:
; CHECK-NEXT: bset csp.0
bmov psw.0, mdl.0
; CHECK: error:
; CHECK-NEXT: bmov psw.0, mdl.0
bfldl sp, #1, #2
; CHECK: error:
; CHECK-NEXT: bfldl sp, #1, #2
bfldh mdh, #1, #2
; CHECK: error:
; CHECK-NEXT: bfldh mdh, #1, #2
bset cp . 0
; CHECK: error:
; CHECK-NEXT: bset cp . 0
