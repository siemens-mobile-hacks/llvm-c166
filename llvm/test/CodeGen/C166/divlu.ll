; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s
; RUN: llc -mtriple=c166-none-elf -stop-after=finalize-isel -verify-machineinstrs < %s | FileCheck %s --check-prefix=ISEL

; The quotient is written before DIVLU consumes the divisor and low dividend.
; ISEL: early-clobber %{{[0-9]+}}:gr32, %{{[0-9]+}}:gr16 = UDIVREM32_16_FULL

declare i32 @llvm.c166.divlu(i32, i16)

define i32 @packed_divlu(i32 %dividend, i16 %divisor) {
; CHECK-LABEL: packed_divlu:
; CHECK:       mov mdh, {{r[0-9]+}}
; CHECK-NEXT:  mov mdl, {{r[0-9]+}}
; CHECK-NEXT:  divlu {{r[0-9]+}}
; CHECK-NEXT:  mov r4, mdl
; CHECK-NEXT:  mov r5, mdh
; CHECK-NEXT:  rets
  %result = call i32 @llvm.c166.divlu(i32 %dividend, i16 %divisor)
  ret i32 %result
}

define i32 @udiv_i32_i16(i32 %dividend, i16 %divisor) {
; CHECK-LABEL: udiv_i32_i16:
; CHECK: divu
; CHECK: divlu
; CHECK-NOT: calls
  %wide_divisor = zext i16 %divisor to i32
  %result = udiv i32 %dividend, %wide_divisor
  ret i32 %result
}

define i32 @multiply_then_udiv_i32_i16(i16 %left, i16 %right, i16 %divisor) {
; CHECK-LABEL: multiply_then_udiv_i32_i16:
; CHECK:       mulu [[RIGHT:r[0-9]+]], [[LEFT:r[0-9]+]]
; CHECK-NEXT:  mov [[LOW:r[0-9]+]], mdl
; CHECK-NEXT:  mov mdl, mdh
; CHECK-NEXT:  divu [[DIVISOR:r[0-9]+]]
; CHECK:       mov mdl, [[LOW]]
; CHECK-NEXT:  divlu [[DIVISOR]]
; CHECK-NOT:   calls
  %wide_left = zext i16 %left to i32
  %wide_right = zext i16 %right to i32
  %product = mul nuw i32 %wide_left, %wide_right
  %wide_divisor = zext i16 %divisor to i32
  %result = udiv i32 %product, %wide_divisor
  ret i32 %result
}

define i32 @urem_i32_i16(i32 %dividend, i16 %divisor) {
; CHECK-LABEL: urem_i32_i16:
; CHECK: divu
; CHECK: divlu
; CHECK-NOT: calls
  %wide_divisor = zext i16 %divisor to i32
  %result = urem i32 %dividend, %wide_divisor
  ret i32 %result
}

define i32 @udiv_i32_masked(i32 %dividend, i32 %divisor) {
; CHECK-LABEL: udiv_i32_masked:
; CHECK: divu
; CHECK: divlu
; CHECK-NOT: calls
  %narrow_divisor = and i32 %divisor, 65535
  %result = udiv i32 %dividend, %narrow_divisor
  ret i32 %result
}

define i32 @udiv_i32_i32(i32 %dividend, i32 %divisor) {
; CHECK-LABEL: udiv_i32_i32:
; CHECK: calls {{.*}}___udivsi3
; CHECK-NOT: divlu
  %result = udiv i32 %dividend, %divisor
  ret i32 %result
}

define i32 @urem_i32_i32(i32 %dividend, i32 %divisor) {
; CHECK-LABEL: urem_i32_i32:
; CHECK: calls {{.*}}___umodsi3
; CHECK-NOT: divlu
  %result = urem i32 %dividend, %divisor
  ret i32 %result
}
