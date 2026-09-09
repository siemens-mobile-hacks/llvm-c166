; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
mov [r0], #0x4002
; CHECK: error:
mov [r0], 65536
; CHECK: error:
mov 65536, [r15]
; CHECK: error:
movb [r15], #0x4003
; CHECK: error:
movb 0x4003, [rh0]
; CHECK: error:
movb [-r0], r4
; CHECK: error:
movb [-rh0], rl4
; CHECK: error:

.irp op, mov, movb
  \op [r0+], [r15+]
  \op [-r0], [r15]
  \op [r0], [-r15]
  \op [r0], [rh7]
.endr
; CHECK-COUNT-8: error:
; CHECK-NOT: error:
