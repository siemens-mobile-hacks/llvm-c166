; REQUIRES: c166-registered-target
; RUN: split-file %s %t
; RUN: llc -mtriple=c166-none-elf -code-model=large -o /dev/null \
; RUN:   -print-after=c166-atomic-lowering %t/large.ll 2>&1 | \
; RUN:   FileCheck %s --check-prefix=LARGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -o /dev/null \
; RUN:   -print-after=c166-atomic-lowering %t/medium.ll 2>&1 | \
; RUN:   FileCheck %s --check-prefix=MEDIUM
; RUN: llc -mtriple=c166-none-elf -code-model=small -o /dev/null \
; RUN:   -print-after=c166-atomic-lowering %t/small.ll 2>&1 | \
; RUN:   FileCheck %s --check-prefix=SMALL

; The runtime data pointer follows the model's default globals address space.
; Runtime functions independently follow the program address space.

; LARGE: call addrspace(1) void @__atomic_load(i16 2, ptr addrspace(2) %object, ptr addrspace(2) {{%.*}}, i16 2)
; MEDIUM: call addrspace(3) void @__atomic_load(i16 2, ptr addrspace(2) %object, ptr addrspace(2) {{%.*}}, i16 2)
; SMALL: call addrspace(1) void @__atomic_load(i16 2, ptr addrspace(3) %object, ptr addrspace(3) {{%.*}}, i16 2)

;--- large.ll
target datalayout = "e-m:u-P1-G2-A2-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-p4:16:16-p5:32:16:16:32-p6:32:16:16:32-i32:16-i64:16-f32:16-f64:16-a:0:16-n8:16-S16-ni:2"
target triple = "c166-none-elf"

define i16 @load(ptr addrspace(2) %object) addrspace(1) {
  %value = load atomic i16, ptr addrspace(2) %object acquire, align 2
  ret i16 %value
}

;--- medium.ll
target datalayout = "e-m:u-P3-G2-A2-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-p4:16:16-p5:32:16:16:32-p6:32:16:16:32-i32:16-i64:16-f32:16-f64:16-a:0:16-n8:16-S16-ni:2"
target triple = "c166-none-elf"

define i16 @load(ptr addrspace(2) %object) addrspace(3) {
  %value = load atomic i16, ptr addrspace(2) %object acquire, align 2
  ret i16 %value
}

;--- small.ll
target datalayout = "e-m:u-P1-G3-A3-p:16:16-p1:32:16-p2:32:16:16:32-p3:16:16-p4:16:16-p5:32:16:16:32-p6:32:16:16:32-i32:16-i64:16-f32:16-f64:16-a:0:16-n8:16-S16-ni:2"
target triple = "c166-none-elf"

define i16 @load(ptr addrspace(3) %object) addrspace(1) {
  %value = load atomic i16, ptr addrspace(3) %object acquire, align 2
  ret i16 %value
}
