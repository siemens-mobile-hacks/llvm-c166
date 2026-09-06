; RUN: llc -mtriple=c166-none-elf -code-model=large -O2 \
; RUN:   -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -O2 \
; RUN:   -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small -O2 \
; RUN:   -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,HUGE

define i16 @zext_ult(i16 %lhs, i16 %rhs) {
; CHECK-LABEL: zext_ult:
; CHECK:       cmp r12, r13
; CHECK-NEXT:  mov [[RESULT:r[0-9]+]], #0
; CHECK-NEXT:  addc [[RESULT]], #0
; CHECK-NOT:   jmpr
; HUGE:        rets
; NEAR:        ret
  %condition = icmp ult i16 %lhs, %rhs
  %result = zext i1 %condition to i16
  ret i16 %result
}

define i16 @zext_uge(i16 %lhs, i16 %rhs) {
; CHECK-LABEL: zext_uge:
; CHECK:       cmp r12, r13
; CHECK-NEXT:  mov [[RESULT:r[0-9]+]], #1
; CHECK-NEXT:  subc [[RESULT]], #0
; CHECK-NOT:   jmpr
; HUGE:        rets
; NEAR:        ret
  %condition = icmp uge i16 %lhs, %rhs
  %result = zext i1 %condition to i16
  ret i16 %result
}

define i16 @zext_ult_short_immediate(i16 %value) {
; CHECK-LABEL: zext_ult_short_immediate:
; CHECK:       cmp r12, #5
; CHECK-NEXT:  mov [[RESULT:r[0-9]+]], #0
; CHECK-NEXT:  addc [[RESULT]], #0
; CHECK-NOT:   jmpr
; HUGE:        rets
; NEAR:        ret
  %condition = icmp ult i16 %value, 5
  %result = zext i1 %condition to i16
  ret i16 %result
}

define i16 @zext_ult_full_immediate(i16 %value) {
; CHECK-LABEL: zext_ult_full_immediate:
; CHECK:       cmp r12, #1000
; CHECK-NEXT:  mov [[RESULT:r[0-9]+]], #0
; CHECK-NEXT:  addc [[RESULT]], #0
; CHECK-NOT:   jmpr
; HUGE:        rets
; NEAR:        ret
  %condition = icmp ult i16 %value, 1000
  %result = zext i1 %condition to i16
  ret i16 %result
}

define i16 @zext_ule_uses_zero_flag(i16 %lhs, i16 %rhs) {
; CHECK-LABEL: zext_ule_uses_zero_flag:
; CHECK:       mov [[RESULT:r[0-9]+]], #1
; CHECK-NEXT:  cmp r12, r13
; CHECK-NEXT:  jmpr cc_ule,
; CHECK:       mov [[RESULT]], #0
; HUGE:        rets
; NEAR:        ret
  %condition = icmp ule i16 %lhs, %rhs
  %result = zext i1 %condition to i16
  ret i16 %result
}

define i16 @select_non_boolean_values(i16 %lhs, i16 %rhs) {
; CHECK-LABEL: select_non_boolean_values:
; CHECK:       mov [[RESULT:r[0-9]+]], #7
; CHECK-NEXT:  cmp r12, r13
; CHECK-NEXT:  jmpr cc_ult,
; CHECK:       mov [[RESULT]], #3
; HUGE:        rets
; NEAR:        ret
  %condition = icmp ult i16 %lhs, %rhs
  %result = select i1 %condition, i16 7, i16 3
  ret i16 %result
}

declare i16 @produce_relation()

define i32 @select_eq_call_passthrough() {
; CHECK-LABEL: select_eq_call_passthrough:
; CHECK:       {{calla|calls}}
; CHECK-NEXT:  cmp r4, #2
; CHECK-NEXT:  jmpr cc_ne,
; CHECK:       mov r4, #1
; CHECK:       mov r5, r4
; CHECK-NEXT:  ashr r5, #15
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %relation = call i16 @produce_relation()
  %condition = icmp eq i16 %relation, 2
  %selected = select i1 %condition, i16 1, i16 %relation
  %result = sext i16 %selected to i32
  ret i32 %result
}

define i32 @select_ne_call_passthrough() {
; CHECK-LABEL: select_ne_call_passthrough:
; CHECK:       {{calla|calls}}
; CHECK-NEXT:  cmp r4, #2
; CHECK-NEXT:  jmpr cc_eq,
; CHECK:       mov r4, #1
; CHECK:       mov r5, r4
; CHECK-NEXT:  ashr r5, #15
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %relation = call i16 @produce_relation()
  %condition = icmp ne i16 %relation, 2
  %selected = select i1 %condition, i16 1, i16 %relation
  %result = sext i16 %selected to i32
  ret i32 %result
}

define i16 @keep_computed_eq_passthrough(i16 %lhs, i16 %rhs) {
; CHECK-LABEL: keep_computed_eq_passthrough:
; CHECK:       xor [[VALUE:r[0-9]+]], {{r[0-9]+}}
; CHECK:       cmp [[VALUE]], #2
; CHECK-NEXT:  jmpr cc_eq,
; HUGE:        rets
; NEAR:        ret
  %value = xor i16 %lhs, %rhs
  %condition = icmp eq i16 %value, 2
  %result = select i1 %condition, i16 1, i16 %value
  ret i16 %result
}
