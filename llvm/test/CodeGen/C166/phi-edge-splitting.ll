; RUN: llc -mtriple=c166-none-elf -O2 < %s | FileCheck %s

declare i32 @llvm.c166.far.add(i32, i16)

define ptr addrspace(2) @find_last(ptr addrspace(2) %source,
                                   i16 %character) addrspace(1) minsize {
entry:
  %narrow = trunc i16 %character to i8
  %extended = sext i8 %narrow to i16
  br label %loop

loop:
  %current = phi ptr addrspace(2) [ %source, %entry ], [ %next, %loop ]
  %result = phi ptr addrspace(2) [ null, %entry ], [ %selected, %loop ]
  %byte = load i8, ptr addrspace(2) %current, align 1
  %value = sext i8 %byte to i16
  %matches = icmp eq i16 %extended, %value
  %selected = select i1 %matches, ptr addrspace(2) %current,
                                  ptr addrspace(2) %result
  %address = ptrtoaddr ptr addrspace(2) %current to i32
  %next.address = call i32 @llvm.c166.far.add(i32 %address, i16 1)
  %next = inttoptr i32 %next.address to ptr addrspace(2)
  %done = icmp eq i8 %byte, 0
  br i1 %done, label %exit, label %loop

exit:
  ret ptr addrspace(2) %selected
}

; CHECK-LABEL: _find_last:
; CHECK-NOT: mov	[-r0], r6
; CHECK: mov	r1, r14
; CHECK: mov	r4, #0
; CHECK: mov	r5, #0
; CHECK: movb	rl2, [r12]
; CHECK: cmpb	rl1, rl2
; CHECK: mov	r4, r12
; CHECK: mov	r5, r13
; CHECK: cmpb	rl2, #0
; CHECK-NOT: mov	r6, [r0+]
; CHECK: rets
