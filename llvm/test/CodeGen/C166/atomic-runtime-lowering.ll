; REQUIRES: c166-registered-target
; RUN: llc -mtriple=c166-none-elf -code-model=large -o /dev/null \
; RUN:   -print-after=c166-atomic-lowering %s 2>&1 | \
; RUN:   FileCheck %s --check-prefix=ATOMIC
; RUN: llc -mtriple=c166-none-elf -code-model=large -o /dev/null \
; RUN:   -print-after=c166-float-memory-lowering %s 2>&1 | \
; RUN:   FileCheck %s --check-prefix=FLOAT

target datalayout = "e-m:u-P1-G2-A2-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-p4:16:16-p5:32:16:16:32-p6:32:16:16:32-i32:16-i64:16-f32:16-f64:16-a:0:16-n8:16-S16-ni:2"
target triple = "c166-none-elf"

define i16 @integer_ops(ptr addrspace(2) %object, i16 %value) addrspace(1) {
entry:
  %loaded = load atomic i16, ptr addrspace(2) %object acquire, align 2
  store atomic i16 %value, ptr addrspace(2) %object release, align 2
  %old = atomicrmw add ptr addrspace(2) %object, i16 %value seq_cst, align 2
  %pair = cmpxchg ptr addrspace(2) %object, i16 %old, i16 %value acq_rel acquire, align 2
  fence seq_cst
  ret i16 %loaded
}

define float @float_exchange(ptr addrspace(2) %object, float %value) addrspace(1) {
entry:
  %old = atomicrmw xchg ptr addrspace(2) %object, float %value seq_cst, align 2
  ret float %old
}

; C166 uses 16-bit size_t and int in every runtime signature.
; ATOMIC-LABEL: define i16 @integer_ops(
; ATOMIC: call addrspace(1) void @__atomic_load(i16 2, ptr addrspace(2) %object, ptr addrspace(2) {{%.*}}, i16 2)
; ATOMIC: call addrspace(1) void @__atomic_store(i16 2, ptr addrspace(2) %object, ptr addrspace(2) {{%.*}}, i16 3)
; ATOMIC: call addrspace(1) void @__c166_atomic_rmw(i16 2, ptr addrspace(2) %object, ptr addrspace(2) {{%.*}}, ptr addrspace(2) {{%.*}}, i16 0)
; ATOMIC: call zeroext addrspace(1) i1 @__atomic_compare_exchange(i16 2, ptr addrspace(2) %object, ptr addrspace(2) {{%.*}}, ptr addrspace(2) {{%.*}}, i16 4, i16 2)
; ATOMIC: call addrspace(1) void @__c166_atomic_fence(i16 5)
; ATOMIC-NOT: load atomic
; ATOMIC-NOT: store atomic
; ATOMIC-NOT: atomicrmw
; ATOMIC-NOT: cmpxchg
; ATOMIC-NOT: fence seq_cst

; Atomic lowering must run first so float memory lowering can encode both
; helper temporaries in physical MSW-first word order.
; FLOAT-LABEL: define float @float_exchange(
; FLOAT: %[[BITS:.*]] = bitcast float %value to i32
; FLOAT: %[[PHYSICAL_PART:.*]] = or i32 0, {{%.*}}
; FLOAT: %[[PHYSICAL:.*]] = or i32 %[[PHYSICAL_PART]], {{%.*}}
; FLOAT: store i32 %[[PHYSICAL]], ptr addrspace(2) {{%.*}}, align 2
; FLOAT: call addrspace(1) void @__atomic_exchange(i16 4, ptr addrspace(2) %object, ptr addrspace(2) {{%.*}}, ptr addrspace(2) {{%.*}}, i16 5)
; FLOAT: %[[OLD_PHYSICAL:.*]] = load i32, ptr addrspace(2) {{%.*}}, align 2
; FLOAT: %[[LOGICAL_PART:.*]] = or i32 0, {{%.*}}
; FLOAT: %[[LOGICAL:.*]] = or i32 %[[LOGICAL_PART]], {{%.*}}
; FLOAT: bitcast i32 %[[LOGICAL]] to float
