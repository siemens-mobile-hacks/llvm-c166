; RUN: llc -mtriple=c166-none-elf -verify-machineinstrs < %s | FileCheck %s

define i16 @unsigned_high_product(i16 %lhs, i16 %rhs) {
; CHECK-LABEL: unsigned_high_product:
; CHECK: mulu
; CHECK: mov {{.*}}, mdh
; CHECK: rets
  %lhs.wide = zext i16 %lhs to i32
  %rhs.wide = zext i16 %rhs to i32
  %product = mul i32 %lhs.wide, %rhs.wide
  %high = lshr i32 %product, 16
  %result = trunc i32 %high to i16
  ret i16 %result
}

define i16 @signed_high_product(i16 %lhs, i16 %rhs) {
; CHECK-LABEL: signed_high_product:
; CHECK: mul
; CHECK-NOT: mulu
; CHECK: mov {{.*}}, mdh
; CHECK: rets
  %lhs.wide = sext i16 %lhs to i32
  %rhs.wide = sext i16 %rhs to i32
  %product = mul i32 %lhs.wide, %rhs.wide
  %high = ashr i32 %product, 16
  %result = trunc i32 %high to i16
  ret i16 %result
}
