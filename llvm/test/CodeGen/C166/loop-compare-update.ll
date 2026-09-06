; RUN: llc -mtriple=c166-none-elf -O2 < %s | FileCheck %s

define i16 @sum_until_wrap(ptr addrspace(2) %values, i16 %count) addrspace(1) {
entry:
  %at_end = icmp eq i16 %count, -2
  br i1 %at_end, label %exit, label %loop

loop:
  %index = phi i16 [ %count, %entry ], [ %next_index, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %next_sum, %loop ]
  %offset = and i16 %index, 7
  %address = getelementptr i16, ptr addrspace(2) %values, i16 %offset
  %value = load i16, ptr addrspace(2) %address, align 2
  %next_sum = add i16 %sum, %value
  %next_index = add i16 %index, -2
  %done = icmp eq i16 %index, 0
  br i1 %done, label %exit, label %loop

exit:
  %result = phi i16 [ 0, %entry ], [ %next_sum, %loop ]
  ret i16 %result
}

; CHECK-LABEL: _sum_until_wrap:
; CHECK:       cmpd2 r14, #0
; CHECK-NEXT:  jmpr cc_ne,
