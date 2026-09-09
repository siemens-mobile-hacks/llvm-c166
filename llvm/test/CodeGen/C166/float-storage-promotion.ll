; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs -print-after=c166-float-memory-lowering -o /dev/null %s 2>&1 | FileCheck %s
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs -print-after=c166-float-memory-lowering -o /dev/null %s 2>&1 | FileCheck %s
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs -print-after=c166-float-memory-lowering -o /dev/null %s 2>&1 | FileCheck %s

declare void @llvm.c166.float.store.f32.p2(float, ptr addrspace(2), i32)
declare void @escape(ptr addrspace(2))

; The integer view must retain the physical, MSW-first word order even when
; the temporary storage disappears.
define i32 @object_bits(float %value) {
; CHECK-LABEL: define i32 @object_bits(
; CHECK-NOT: alloca
; CHECK: [[BITS:%.*]] = bitcast float %value to i32
; CHECK: [[LOW:%.*]] = and i32 [[BITS]], 65535
; CHECK: [[SHIFT:%.*]] = shl i32 [[LOW]], 16
; CHECK: [[PART:%.*]] = or i32 0, [[SHIFT]]
; CHECK: [[HIGH:%.*]] = lshr i32 [[BITS]], 16
; CHECK: [[MASK:%.*]] = and i32 [[HIGH]], 65535
; CHECK: [[WORDS:%.*]] = or i32 [[PART]], [[MASK]]
; CHECK-NOT: store
; CHECK-NOT: load
; CHECK: ret i32 [[WORDS]]
  %slot = alloca float, align 2, addrspace(2)
  call void @llvm.c166.float.store.f32.p2(float %value, ptr addrspace(2) %slot, i32 2)
  %bits = load i32, ptr addrspace(2) %slot, align 2
  ret i32 %bits
}

define i32 @volatile_bits(float %value) {
; CHECK-LABEL: define i32 @volatile_bits(
; CHECK: %slot = alloca float
; CHECK: store i32 {{.*}}, ptr addrspace(2) %slot
; CHECK: load volatile i32, ptr addrspace(2) %slot
  %slot = alloca float, align 2, addrspace(2)
  call void @llvm.c166.float.store.f32.p2(float %value, ptr addrspace(2) %slot, i32 2)
  %bits = load volatile i32, ptr addrspace(2) %slot, align 2
  ret i32 %bits
}

define i32 @escaped_bits(float %value) {
; CHECK-LABEL: define i32 @escaped_bits(
; CHECK: %slot = alloca float
; CHECK: store i32 {{.*}}, ptr addrspace(2) %slot
; CHECK: call addrspace({{[13]}}) void @escape(ptr addrspace(2) %slot)
; CHECK: load i32, ptr addrspace(2) %slot
  %slot = alloca float, align 2, addrspace(2)
  call void @llvm.c166.float.store.f32.p2(float %value, ptr addrspace(2) %slot, i32 2)
  call void @escape(ptr addrspace(2) %slot)
  %bits = load i32, ptr addrspace(2) %slot, align 2
  ret i32 %bits
}
