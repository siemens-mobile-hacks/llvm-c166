; RUN: llc -mtriple=c166-none-elf -code-model=large -O0 -verify-each -o /dev/null -print-after=c166-f64-lowering %s 2>&1 | FileCheck %s
; RUN: llc -mtriple=c166-none-elf -code-model=medium -O0 -verify-each -o /dev/null -print-after=c166-f64-lowering %s 2>&1 | FileCheck %s
; RUN: llc -mtriple=c166-none-elf -code-model=small -O0 -verify-each -o /dev/null -print-after=c166-f64-lowering %s 2>&1 | FileCheck %s

; The sole store user does not allow the helper to overwrite p before the
; intervening load. The value written to out must remain 1, not 5.
define void @early_store(ptr addrspace(2) sret(double) %out) {
; CHECK-LABEL: define void @early_store(
; CHECK: store double 1.000000e+00, ptr addrspace(2) %p
; CHECK-NOT: @llvm.c166.stack.address.p2(ptr addrspace(2) %p)
; CHECK: @__c166_adddf3(
; CHECK: %before = load double, ptr addrspace(2) %p
; CHECK: store double %{{[-a-zA-Z$._0-9]+}}, ptr addrspace(2) %p
; CHECK: store double %before, ptr addrspace(2) %out
; CHECK: ret void
entry:
  %p = alloca double, align 2, addrspace(2)
  store double 1.0, ptr addrspace(2) %p, align 2
  %sum = fadd double 2.0, 3.0
  %before = load double, ptr addrspace(2) %p, align 2
  store double %sum, ptr addrspace(2) %p, align 2
  store double %before, ptr addrspace(2) %out, align 2
  ret void
}

declare double @llvm.fmuladd.f64(double, double, double)

; An intermediate operation inherits sret without having its own store user.
define void @nested_fmuladd(ptr addrspace(2) noalias sret(double) %out) {
; CHECK-LABEL: define void @nested_fmuladd(
; CHECK: %f64.product = alloca double
; CHECK: @llvm.c166.stack.address.p{{[23]}}(ptr addrspace({{[23]}}) %f64.product)
; CHECK: @__c166_muldf3(
; CHECK: @__c166_adddf3(
; CHECK: @__c166_adddf3(
; CHECK: ret void
  store double 1.0, ptr addrspace(2) %out, align 2
  %old = load double, ptr addrspace(2) %out, align 2
  %fused = call double @llvm.fmuladd.f64(double 2.0, double 3.0, double %old)
  %sum = fadd double %fused, 1.0
  store double %sum, ptr addrspace(2) %out, align 2
  ret void
}

; Even a noalias sret pointer can be an input to the operation. The product
; must not destroy the old addend before the addition has consumed it.
define void @fmuladd_sret_addend(ptr addrspace(2) noalias sret(double) %out) {
; CHECK-LABEL: define void @fmuladd_sret_addend(
; CHECK: %f64.product = alloca double
; CHECK: @__c166_muldf3(
; CHECK: @__c166_adddf3(
; CHECK: ret void
entry:
  %old = load double, ptr addrspace(2) %out, align 2
  %sum = call double @llvm.fmuladd.f64(double 2.0, double 3.0, double %old)
  store double %sum, ptr addrspace(2) %out, align 2
  ret void
}
