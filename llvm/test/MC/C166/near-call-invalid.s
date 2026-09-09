; RUN: not llvm-mc -triple=c166 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

; Numeric relative operands use the encoded unsigned byte spelling.
callr -1
; CHECK: error: expected a symbolic branch target or encoded 8-bit displacement
callr 256
; CHECK: error: expected a symbolic branch target or encoded 8-bit displacement
pcall r4, -1
; CHECK: error: expected a 16-bit code offset or cof(expression)
pcall r4, 65536
; CHECK: error: expected a 16-bit code offset or cof(expression)

pcall rl2, 0
; CHECK: error:
; CHECK-NEXT: pcall rl2, 0
retp rl2
; CHECK: error:
; CHECK-NEXT: retp rl2
calli cc_uc, [r16]
; CHECK: error:
; CHECK-NEXT: calli cc_uc, [r16]
calli cc_uc, [r4+]
; CHECK: error:
; CHECK-NEXT: calli cc_uc, [r4+]
jmpi cc_uc, [r4+2]
; CHECK: error:
; CHECK-NEXT: jmpi cc_uc, [r4+2]
ret r4
; CHECK: error:
; CHECK-NEXT: ret r4
rets r4
; CHECK: error:
; CHECK-NEXT: rets r4
reti r4
; CHECK: error:
; CHECK-NEXT: reti r4
