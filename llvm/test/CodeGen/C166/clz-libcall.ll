; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE

declare i32 @llvm.ctlz.i32(i32, i1 immarg)
declare i16 @llvm.ctlz.i16(i16, i1 immarg)

define i16 @clz16(i16 %value) {
; CHECK-LABEL: clz16:
; CHECK: prior
; CHECK-NOT: ___clzsi2
; HUGE: rets
; NEAR: ret
  %result = call i16 @llvm.ctlz.i16(i16 %value, i1 true)
  ret i16 %result
}

define i16 @clz16_defined(i16 %value) {
; CHECK-LABEL: clz16_defined:
; PRIOR returns zero for a zero source; defined ctlz must return 16 instead.
; CHECK: cmp r12, #0
; CHECK-NEXT: jmpr cc_eq, [[ZERO:\.LBB[0-9]+_[0-9]+]]
; CHECK: prior r4, r12
; CHECK-NOT: ___clzsi2
; HUGE: rets
; NEAR: ret
; CHECK: [[ZERO]]:
; CHECK-NEXT: mov r4, #16
; HUGE-NEXT: rets
; NEAR-NEXT: ret
  %result = call i16 @llvm.ctlz.i16(i16 %value, i1 false)
  ret i16 %result
}

define i32 @clz32(i32 %value) {
; CHECK-LABEL: clz32:
; HUGE:  calls {{.*}}___clzsi2
; NEAR:  calla {{.*}}___clzsi2
; CHECK: mov r5, r4
; CHECK-NEXT: ashr r5, #15
; HUGE:  rets
; NEAR:  ret
  %result = call i32 @llvm.ctlz.i32(i32 %value, i1 true)
  ret i32 %result
}
