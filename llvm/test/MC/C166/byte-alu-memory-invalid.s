; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
addcb rl2, -1
; CHECK: error:
subcb rl2, 65536
; CHECK: error:
cmpb rl2, #65536
; CHECK: error:
andb #123, rl2
; CHECK: error:
orb 123, r4
; CHECK: error:
addcb 123, #4
; CHECK: error:
; No CMPB memory-destination form in the ISA.
cmpb 123, rl2
; CHECK: error:
