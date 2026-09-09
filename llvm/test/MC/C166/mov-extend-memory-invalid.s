; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
.irp op, movbs, movbz
  \op mdl, 65536
  \op mdl, #1
  \op 65536, rl0
  \op 65536, mdl
  \op 0x4000, r4
  \op rl0, 0x4000
  \op r0, r1
  \op rl0, rl1
  \op r0, [r1]
  \op [r0], rl1
.endr
; CHECK-COUNT-20: error:
; CHECK-NOT: error:
