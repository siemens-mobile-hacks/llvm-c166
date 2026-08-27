; RUN: llc -mtriple=c166-none-elf -verify-machineinstrs < %s | FileCheck %s
; C166-ABI: args.stackparm

define cc 128 i16 @stackparm_callee(i16 %a, i32 %b, i16 %c) {
; CHECK-LABEL: _stackparm_callee:
; CHECK-DAG:   mov r4, [r0]
; CHECK-DAG:   mov [[TMP:r[0-9]+]], [r0 + #2]
; CHECK:       add r4, [[TMP]]
; CHECK-NEXT:  mov [[TMP]], [r0 + #4]
; CHECK-NEXT:  add r4, [[TMP]]
; CHECK-NEXT:  mov [[TMP]], [r0 + #6]
; CHECK-NEXT:  add r4, [[TMP]]
; CHECK-NEXT:  rets
  %blo = trunc i32 %b to i16
  %bhi.shift = lshr i32 %b, 16
  %bhi = trunc i32 %bhi.shift to i16
  %ab = add i16 %a, %blo
  %abh = add i16 %ab, %bhi
  %result = add i16 %abh, %c
  ret i16 %result
}

declare cc 128 i16 @stackparm_external(i16, i32, i16)

define i16 @stackparm_caller(i16 %a, i32 %b, i16 %c) {
; CHECK-LABEL: _stackparm_caller:
; CHECK:       sub r0, #6
; CHECK-NEXT:  sub r0, #2
; CHECK-DAG:   mov [r0], r12
; CHECK-DAG:   mov [r0 + #2], r13
; CHECK-DAG:   mov [r0 + #4], r14
; CHECK-DAG:   mov [r0 + #6], r15
; CHECK:       calls seg(_stackparm_external), sof(_stackparm_external)
; CHECK-NEXT:  add r0, #6
; CHECK-NEXT:  add r0, #2
; CHECK-NEXT:  rets
  %result = call cc 128 i16 @stackparm_external(i16 %a, i32 %b, i16 %c)
  ret i16 %result
}
