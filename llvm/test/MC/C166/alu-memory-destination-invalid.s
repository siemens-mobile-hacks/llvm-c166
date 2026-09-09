; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
add -1, r4
; CHECK: error:
addc 65536, r4
; CHECK: error:
sub #123, r4
; CHECK: error:
subc 123, #4
; CHECK: error:
and 123, rl2
; CHECK: error:
or 123, r16
; CHECK: error:
; The ISA has no CMP mem,reg (opcode 44 is reserved).
cmp 123, r4
; CHECK: error:
