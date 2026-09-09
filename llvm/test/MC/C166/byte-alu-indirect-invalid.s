; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

.irp op, addb, subb, cmpb, xorb, andb, orb
  \op rl2, [r4]
  \op rh2, [r4+]
  \op rl2, [-r0]
  \op rh2, [r0 + #1]
  \op r4, [r0]
.endr
; CHECK-COUNT-30: error:
; CHECK-NOT: error:
