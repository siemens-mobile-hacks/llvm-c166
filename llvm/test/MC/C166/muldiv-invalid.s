; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

.irp op, div, divu, divl, divlu
  \op rl0
  \op mdl
  \op #1
  \op [r0]
  \op r0, r1
.endr
.irp op, mul, mulu
  \op rl0, r0
  \op r0, rl0
  \op mdl, r0
  \op r0, #1
  \op r0, [r1]
.endr
; CHECK-COUNT-30: error:
; CHECK-NOT: error:
