; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

extr #0
; CHECK: error: instruction count must be in the range 1..4
extr #5
; CHECK: error: instruction count must be in the range 1..4
extr 1
; CHECK: error: instruction count must be in the range 1..4
extpr #1024, #1
; CHECK: error: expected a 10-bit page or pag(expression)
extsr #256, #1
; CHECK: error: expected an 8-bit segment or seg(expression)
exts #-1, #1
; CHECK: error: expected an 8-bit segment or seg(expression)
extsr #pag(object), #1
; CHECK: error: expected an 8-bit segment or seg(expression)
extpr r0, #5
; CHECK: error: instruction count must be in the range 1..4
extsr r0, #0
; CHECK: error: instruction count must be in the range 1..4

atomic #-1
; CHECK: error: atomic count must be in the range 1..4
atomic #5
; CHECK: error: atomic count must be in the range 1..4
