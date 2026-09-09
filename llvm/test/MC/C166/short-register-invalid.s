; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
mov sfr(-1), #0
; CHECK: error: SFR address must be an absolute constant in 0..239
mov sfr(240), #0
; CHECK: error: SFR address must be an absolute constant in 0..239
mov sfr(256), #0
; CHECK: error: SFR address must be an absolute constant in 0..239
mov sfr(external), #0
; CHECK: error: SFR address must be an absolute constant in 0..239
mov sfr(forward), #0
; CHECK: error: SFR address must be an absolute constant in 0..239
.set forward, 189
mov sfr(1, #0
; CHECK: error: expected ')' after SFR address
mov [sfr(189)], r0
; CHECK: error: invalid register name
movb r0, sfr(189)
; CHECK: error:
shl sfr(189), #1
; CHECK: error:
; CHECK-NOT: error:
