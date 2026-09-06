; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

define i16 @reuse_zero_i16(ptr addrspace(3) %source, i16 %count) {
; CHECK-LABEL: reuse_zero_i16:
; CHECK:       mov r4, #0
; CHECK-NOT:   mov r4, #0
; CHECK:       rets
entry:
  %empty = icmp eq i16 %count, 0
  br i1 %empty, label %exit, label %loop

loop:
  %sum = phi i16 [ 0, %entry ], [ %next_sum, %loop ]
  %remaining = phi i16 [ %count, %entry ], [ %next_remaining, %loop ]
  %cursor = phi ptr addrspace(3) [ %source, %entry ], [ %next_cursor, %loop ]
  %value = load i16, ptr addrspace(3) %cursor, align 2
  %next_sum = add i16 %sum, %value
  %next_cursor = getelementptr i8, ptr addrspace(3) %cursor, i16 2
  %next_remaining = add i16 %remaining, -1
  %done = icmp eq i16 %next_remaining, 0
  br i1 %done, label %exit, label %loop

exit:
  %result = phi i16 [ 0, %entry ], [ %next_sum, %loop ]
  ret i16 %result
}

define i32 @reuse_zero_i32(ptr addrspace(3) %source, i16 %count) {
; CHECK-LABEL: reuse_zero_i32:
; CHECK:       mov r4, #0
; CHECK-NEXT:  mov r5, #0
; CHECK-NOT:   mov r4, #0
; CHECK-NOT:   mov r5, #0
; CHECK:       rets
entry:
  %empty = icmp eq i16 %count, 0
  br i1 %empty, label %exit, label %loop

loop:
  %sum = phi i32 [ 0, %entry ], [ %next_sum, %loop ]
  %remaining = phi i16 [ %count, %entry ], [ %next_remaining, %loop ]
  %cursor = phi ptr addrspace(3) [ %source, %entry ], [ %next_cursor, %loop ]
  %value = load i32, ptr addrspace(3) %cursor, align 2
  %next_sum = add i32 %sum, %value
  %next_cursor = getelementptr i8, ptr addrspace(3) %cursor, i16 4
  %next_remaining = add i16 %remaining, -1
  %done = icmp eq i16 %next_remaining, 0
  br i1 %done, label %exit, label %loop

exit:
  %result = phi i32 [ 0, %entry ], [ %next_sum, %loop ]
  ret i32 %result
}
