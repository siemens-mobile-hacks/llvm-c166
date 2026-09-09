; RUN: not llvm-mc -triple=c166 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

; A 32-bit pointer representation cannot fit in an instruction's word operand.
mov r4, #paged(object)
; CHECK: error:
; CHECK-NEXT: mov r4, #paged(object)
add r4, #paged(object)
; CHECK: error:
; CHECK-NEXT: add r4, #paged(object)
scxt r4, #paged(object)
; CHECK: error:
; CHECK-NEXT: scxt r4, #paged(object)
scxt r4, paged(object)
; CHECK: error:
; CHECK-NEXT: scxt r4, paged(object)

; Control-transfer fields require their own address component.
calls 0, seg(object)
; CHECK: error: expected a 16-bit offset or sof(expression)
jmps 0, pag(object)
; CHECK: error: expected a 16-bit offset or sof(expression)
jmpa cc_uc, pag(object)
; CHECK: error: expected a 16-bit code offset or cof(expression)
calla cc_uc, seg(object)
; CHECK: error: expected a 16-bit code offset or cof(expression)
jmpr cc_eq, sof(object)
; CHECK: error:
; CHECK-NEXT: jmpr cc_eq, sof(object)
calls #1, 0
; CHECK: error: expected an 8-bit segment or seg(expression)
calls 1, #0
; CHECK: error: expected a 16-bit offset or sof(expression)
jmpr cc_eq, #0
; CHECK: error:
; CHECK-NEXT: jmpr cc_eq, #0
