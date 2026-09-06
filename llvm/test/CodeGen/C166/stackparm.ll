; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; C166-ABI: args.stackparm

define cc 128 i16 @stackparm_callee(i16 %a, i32 %b, i16 %c) {
; CHECK-LABEL: _stackparm_callee:
; CHECK:       mov [[CUR:r[0-9]+]], r0
; CHECK-NEXT:  add [[CUR]], #2
; CHECK-NEXT:  mov [[SUM:r[0-9]+]], [r0]
; CHECK-NEXT:  add [[SUM]], {{\[}}[[CUR]]{{\+\]}}
; CHECK-NEXT:  mov [[TMP:r[0-9]+]], {{\[}}[[CUR]]{{\+\]}}
; CHECK-NEXT:  add [[TMP]], [[SUM]]
; CHECK-NEXT:  mov r4, {{\[}}[[CUR]]{{\]}}
; CHECK-NEXT:  add r4, [[TMP]]
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
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
; CHECK-NOT:   sub r0
; CHECK:       mov [-r0], r15
; CHECK-NEXT:  mov [-r0], r14
; CHECK-NEXT:  mov [-r0], r13
; CHECK-NEXT:  mov [-r0], r12
; HUGE:        calls seg(_stackparm_external), sof(_stackparm_external)
; NEAR:        calla cc_uc, cof(_stackparm_external)
; CHECK-NEXT:  add r0, #8
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %result = call cc 128 i16 @stackparm_external(i16 %a, i32 %b, i16 %c)
  ret i16 %result
}

declare i16 @normal_six(i16, i16, i16, i16, i16, i16)

define i16 @normal_six_caller(i16 %a, i16 %b, i16 %c, i16 %d) {
; CHECK-LABEL: _normal_six_caller:
; CHECK-NOT:   sub r0
; CHECK:       mov [-r0],
; CHECK-NEXT:  mov [-r0],
; HUGE:        calls seg(_normal_six), sof(_normal_six)
; NEAR:        calla cc_uc, cof(_normal_six)
; CHECK-NEXT:  add r0, #4
  %result = call i16 @normal_six(i16 %a, i16 %b, i16 %c, i16 %d,
                                 i16 %a, i16 %b)
  ret i16 %result
}
