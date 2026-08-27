; RUN: llc -mtriple=c166-none-elf -verify-machineinstrs < %s | FileCheck %s

declare i32 @llvm.ctlz.i32(i32, i1 immarg)

define i32 @clz32(i32 %value) {
; CHECK-LABEL: clz32:
; CHECK: calls {{.*}}___clzsi2
; CHECK: rets
  %result = call i32 @llvm.ctlz.i32(i32 %value, i1 true)
  ret i32 %result
}
