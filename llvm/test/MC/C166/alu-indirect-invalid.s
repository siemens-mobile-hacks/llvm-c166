; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

.irp op, add, addc, sub, subc, cmp, xor, and, or
  \op r0, [r4]
  \op r15, [r4+]
  \op r0, [-r0]
  \op r15, [r0 + #2]
  \op rl0, [r0]
.endr
; CHECK-COUNT-40: error: {{invalid operand for instruction|expected a 16-bit absolute address}}
; CHECK-NOT: error:
