; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

.irp op, shl, shr, ashr
  \op r0, #-1
  \op r15, #16
  \op rl0, #1
  \op r0, rl0
  \op r0, [r1]
.endr
; CHECK-COUNT-15: error:
; CHECK-NOT: error:
