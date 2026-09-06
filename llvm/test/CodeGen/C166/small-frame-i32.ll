; RUN: llc -mtriple=c166-none-elf -code-model=small \
; RUN:   -stop-after=finalize-isel -verify-machineinstrs -o - %s \
; RUN:   | FileCheck %s

target datalayout = "e-m:u-P1-G3-A3-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-p4:16:16-p5:32:16:16:32-p6:32:16:16:32-i32:16-i64:16-f32:16-f64:16-a:0:16-n8:16-S16-ni:2"
target triple = "c166-none-elf"

define i32 @frame_i32_offset(i32 %value) addrspace(1) {
entry:
  %object = alloca [2 x i32], align 2, addrspace(3)
  %address = getelementptr inbounds i8, ptr addrspace(3) %object, i16 4
  store volatile i32 %value, ptr addrspace(3) %address, align 2
  %result = load volatile i32, ptr addrspace(3) %address, align 2
  ret i32 %result
}

; CHECK-LABEL: name: frame_i32_offset
; CHECK:       NEARSTORE32
; CHECK:       = FRAMELOAD32 %stack.0.object, 4

define i32 @near_i32_offset(ptr addrspace(3) %base) addrspace(1) {
entry:
  %address = getelementptr inbounds i8, ptr addrspace(3) %base, i16 4
  %result = load volatile i32, ptr addrspace(3) %address, align 2
  ret i32 %result
}

; CHECK-LABEL: name: near_i32_offset
; CHECK:       = NEARLOAD32
; CHECK-NOT:   FRAMELOAD32
