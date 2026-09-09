; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

.irp op, neg, cpl, negb, cplb
  \op #1
  \op [r0]
  \op mdl
  \op r0, r1
.endr
neg rl0
cpl rh7
negb r0
cplb r15
prior rl0, r0
prior r0, rl0
prior r0, #1
prior r0, [r1]
prior mdl, r0
; CHECK-COUNT-25: error:
; CHECK-NOT: error:
