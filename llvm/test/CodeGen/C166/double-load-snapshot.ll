; RUN: llc -mtriple=c166-none-elf -code-model=large -O0 -verify-each -o /dev/null -print-after=c166-f64-lowering %s 2>&1 | FileCheck %s
; RUN: llc -mtriple=c166-none-elf -code-model=medium -O0 -verify-each -o /dev/null -print-after=c166-f64-lowering %s 2>&1 | FileCheck %s
; RUN: llc -mtriple=c166-none-elf -code-model=small -O0 -verify-each -o /dev/null -print-after=c166-f64-lowering %s 2>&1 | FileCheck %s

; A stack address is not a substitute for an SSA value if the object changes
; before the runtime operation reads it. Test both a store and an escaped alloca.
declare void @mutate(ptr addrspace(2))
declare double @llvm.fmuladd.f64(double, double, double)

; Internal binary64 helpers write only their explicit destination. A helper
; writing a distinct stack object must not force snapshots of byval operands.
define void @reuse_byval_across_runtime(
    ptr addrspace(2) noalias sret(double) %out,
    ptr addrspace(2) byval(double) %left,
    ptr addrspace(2) byval(double) %right) {
; CHECK-LABEL: define void @reuse_byval_across_runtime(
; CHECK: %quotient.f64 = alloca double, align 2, addrspace([[STACK_AS:[23]]])
; CHECK-NOT: alloca double
; CHECK-NOT: store double
; CHECK: @llvm.c166.stack.address.p[[STACK_AS]](ptr addrspace([[STACK_AS]]) %quotient.f64)
; CHECK: @llvm.c166.stack.address.p2(ptr addrspace(2) %left)
; CHECK: @llvm.c166.stack.address.p2(ptr addrspace(2) %right)
; CHECK: @__c166_divdf3(
; CHECK: @llvm.c166.stack.address.p2(ptr addrspace(2) %out)
; CHECK: @llvm.c166.stack.address.p2(ptr addrspace(2) %left)
; CHECK: @llvm.c166.stack.address.p2(ptr addrspace(2) %right)
; CHECK: @__c166_muldf3(
; CHECK: @__c166_adddf3(
; CHECK-NOT: alloca double
; CHECK-NOT: store double
; CHECK: ret void
entry:
  %lhs = load double, ptr addrspace(2) %left, align 2
  %rhs = load double, ptr addrspace(2) %right, align 2
  %quotient = fdiv double %lhs, %rhs
  %result = call double @llvm.fmuladd.f64(double %lhs, double %rhs,
                                         double %quotient)
  store double %result, ptr addrspace(2) %out, align 2
  ret void
}

; A safe first use must not cache the source address for a later unsafe use.
define void @snapshot_multiple_uses(ptr addrspace(2) sret(double) %out,
                                   ptr addrspace(2) %first) {
; CHECK-LABEL: define void @snapshot_multiple_uses(
; CHECK: %old = load double, ptr addrspace(2) %p
; CHECK: store double %old, ptr addrspace([[AS:[23]]]) %[[COPY:[-a-zA-Z$._0-9]+]]
; CHECK: @__c166_adddf3(
; CHECK: call {{.*}}@mutate(ptr addrspace(2) %p)
; CHECK: @llvm.c166.stack.address.p[[AS]](ptr addrspace([[AS]]) %[[COPY]])
; CHECK: @__c166_adddf3(
; CHECK: ret void
entry:
  %p = alloca double, align 2, addrspace(2)
  store double 1.0, ptr addrspace(2) %p, align 2
  %old = load double, ptr addrspace(2) %p, align 2
  %a = fadd double %old, %old
  store double %a, ptr addrspace(2) %first, align 2
  call void @mutate(ptr addrspace(2) %p)
  %b = fadd double %old, %old
  store double %b, ptr addrspace(2) %out, align 2
  ret void
}

; A private copy cannot be replaced by a byval source that has since changed.
define void @byval_source_changed(ptr addrspace(2) sret(double) %out,
                                 ptr addrspace(2) byval(double) %source) {
; CHECK-LABEL: define void @byval_source_changed(
; CHECK: call {{.*}}@mutate(ptr addrspace(2) %source)
; CHECK-NOT: @llvm.c166.stack.address.p2(ptr addrspace(2) %source)
; CHECK: @__c166_adddf3(
; CHECK: ret void
entry:
  %copy = alloca double, align 2, addrspace(2)
  %original = load double, ptr addrspace(2) %source, align 2
  store double %original, ptr addrspace(2) %copy, align 2
  call void @mutate(ptr addrspace(2) %source)
  %old = load double, ptr addrspace(2) %copy, align 2
  %sum = fadd double %old, %old
  store double %sum, ptr addrspace(2) %out, align 2
  ret void
}

define void @snapshot_store(ptr addrspace(2) sret(double) %out) {
; CHECK-LABEL: define void @snapshot_store(
; CHECK: %old = load double, ptr addrspace(2) %p
; CHECK: store double %old, ptr addrspace([[AS:[23]]]) %[[COPY:[-a-zA-Z$._0-9]+]]
; CHECK: store double 2.000000e+00, ptr addrspace(2) %p
; CHECK: @llvm.c166.stack.address.p[[AS]](ptr addrspace([[AS]]) %[[COPY]])
; CHECK: @__c166_adddf3(
; CHECK: ret void
entry:
  %p = alloca double, align 2, addrspace(2)
  store double 1.0, ptr addrspace(2) %p, align 2
  %old = load double, ptr addrspace(2) %p, align 2
  store double 2.0, ptr addrspace(2) %p, align 2
  %sum = fadd double %old, %old
  store double %sum, ptr addrspace(2) %out, align 2
  ret void
}

define void @snapshot_call(ptr addrspace(2) sret(double) %out) {
; CHECK-LABEL: define void @snapshot_call(
; CHECK: %old = load double, ptr addrspace(2) %p
; CHECK: store double %old, ptr addrspace([[AS:[23]]]) %[[COPY:[-a-zA-Z$._0-9]+]]
; CHECK: call {{.*}}@mutate(ptr addrspace(2) %p)
; CHECK: @llvm.c166.stack.address.p[[AS]](ptr addrspace([[AS]]) %[[COPY]])
; CHECK: @__c166_adddf3(
; CHECK: ret void
entry:
  %p = alloca double, align 2, addrspace(2)
  store double 1.0, ptr addrspace(2) %p, align 2
  %old = load double, ptr addrspace(2) %p, align 2
  call void @mutate(ptr addrspace(2) %p)
  %sum = fadd double %old, %old
  store double %sum, ptr addrspace(2) %out, align 2
  ret void
}

; CHECK: declare void @__c166_divdf3(ptr addrspace({{[34]}}) writeonly, ptr addrspace({{[34]}}) readonly, ptr addrspace({{[34]}}) readonly)
; CHECK: declare void @__c166_muldf3(ptr addrspace({{[34]}}) writeonly, ptr addrspace({{[34]}}) readonly, ptr addrspace({{[34]}}) readonly)
; CHECK: declare void @__c166_adddf3(ptr addrspace({{[34]}}) writeonly, ptr addrspace({{[34]}}) readonly, ptr addrspace({{[34]}}) readonly)
