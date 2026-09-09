; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
.irp op, addb, addcb, subb, subcb, cmpb, xorb, andb, orb
  \op mdl, #256
  \op mdl, #-129
  \op mdl, 65536
.endr
; CHECK-COUNT-24: error:
cmpb 0x4000, mdl
; CHECK: error:
; CHECK-NOT: error:
