; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
movb mdl, #256
; CHECK: error:
movb mdl, #-129
; CHECK: error:
movb mdl, 65536
; CHECK: error:
movb 65536, mdl
; CHECK: error:
movb rl0, 65536
; CHECK: error:
movb r4, 0x4000
; CHECK: error:
