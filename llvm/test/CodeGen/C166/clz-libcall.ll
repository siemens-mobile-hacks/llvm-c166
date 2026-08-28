; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE

declare i32 @llvm.ctlz.i32(i32, i1 immarg)

define i32 @clz32(i32 %value) {
; CHECK-LABEL: clz32:
; HUGE:  calls {{.*}}___clzsi2
; NEAR:  calla {{.*}}___clzsi2
; HUGE:  rets
; NEAR:  ret
  %result = call i32 @llvm.ctlz.i32(i32 %value, i1 true)
  ret i32 %result
}
