; RUN: llc -mtriple=c166-none-elf -O2 -verify-machineinstrs < %s | FileCheck %s

define i16 @far_unsigned_sum(ptr addrspace(2) %source, i16 %count) {
entry:
  br label %loop

loop:
  %cursor = phi ptr addrspace(2) [ %source, %entry ], [ %next, %loop ]
  %remaining = phi i16 [ %count, %entry ], [ %remaining.next, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %sum.next, %loop ]
  %value = load i8, ptr addrspace(2) %cursor, align 1
  %next = getelementptr i8, ptr addrspace(2) %cursor, i32 1
  %extended = zext i8 %value to i16
  %sum.next = add i16 %sum, %extended
  %remaining.next = add i16 %remaining, -1
  %done = icmp eq i16 %remaining.next, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i16 %sum.next
}

; CHECK-LABEL: far_unsigned_sum:
; CHECK:       extp [[PAGE:r[0-9]+]], #1
; CHECK-NEXT:  movb [[BYTE:r[hl][0-9]+]], [{{r[0-9]+}}+]
; CHECK-NEXT:  movbz {{r[0-9]+}}, [[BYTE]]

define i16 @far_signed_sum(ptr addrspace(2) %source, i16 %count) {
entry:
  br label %loop

loop:
  %cursor = phi ptr addrspace(2) [ %source, %entry ], [ %next, %loop ]
  %remaining = phi i16 [ %count, %entry ], [ %remaining.next, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %sum.next, %loop ]
  %value = load i8, ptr addrspace(2) %cursor, align 1
  %next = getelementptr i8, ptr addrspace(2) %cursor, i32 1
  %extended = sext i8 %value to i16
  %sum.next = add i16 %sum, %extended
  %remaining.next = add i16 %remaining, -1
  %done = icmp eq i16 %remaining.next, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i16 %sum.next
}

; CHECK-LABEL: far_signed_sum:
; CHECK:       extp [[PAGE:r[0-9]+]], #1
; CHECK-NEXT:  movb [[BYTE:r[hl][0-9]+]], [{{r[0-9]+}}+]
; CHECK-NEXT:  movbs {{r[0-9]+}}, [[BYTE]]

define i16 @near_unsigned_sum(ptr addrspace(3) %source, i16 %count) {
entry:
  br label %loop

loop:
  %cursor = phi ptr addrspace(3) [ %source, %entry ], [ %next, %loop ]
  %remaining = phi i16 [ %count, %entry ], [ %remaining.next, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %sum.next, %loop ]
  %value = load i8, ptr addrspace(3) %cursor, align 1
  %next = getelementptr i8, ptr addrspace(3) %cursor, i16 1
  %extended = zext i8 %value to i16
  %sum.next = add i16 %sum, %extended
  %remaining.next = add i16 %remaining, -1
  %done = icmp eq i16 %remaining.next, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i16 %sum.next
}

; CHECK-LABEL: near_unsigned_sum:
; CHECK-NOT:   extp
; CHECK:       movb [[BYTE:r[hl][0-9]+]], [{{r[0-9]+}}+]
; CHECK-NEXT:  movbz {{r[0-9]+}}, [[BYTE]]

define i16 @near_signed_sum(ptr addrspace(3) %source, i16 %count) {
entry:
  br label %loop

loop:
  %cursor = phi ptr addrspace(3) [ %source, %entry ], [ %next, %loop ]
  %remaining = phi i16 [ %count, %entry ], [ %remaining.next, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %sum.next, %loop ]
  %value = load i8, ptr addrspace(3) %cursor, align 1
  %next = getelementptr i8, ptr addrspace(3) %cursor, i16 1
  %extended = sext i8 %value to i16
  %sum.next = add i16 %sum, %extended
  %remaining.next = add i16 %remaining, -1
  %done = icmp eq i16 %remaining.next, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i16 %sum.next
}

; CHECK-LABEL: near_signed_sum:
; CHECK-NOT:   extp
; CHECK:       movb [[BYTE:r[hl][0-9]+]], [{{r[0-9]+}}+]
; CHECK-NEXT:  movbs {{r[0-9]+}}, [[BYTE]]

define i16 @far_volatile_sum(ptr addrspace(2) %source, i16 %count) {
entry:
  br label %loop

loop:
  %cursor = phi ptr addrspace(2) [ %source, %entry ], [ %next, %loop ]
  %remaining = phi i16 [ %count, %entry ], [ %remaining.next, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %sum.next, %loop ]
  %value = load volatile i8, ptr addrspace(2) %cursor, align 1
  %next = getelementptr i8, ptr addrspace(2) %cursor, i32 1
  %extended = zext i8 %value to i16
  %sum.next = add i16 %sum, %extended
  %remaining.next = add i16 %remaining, -1
  %done = icmp eq i16 %remaining.next, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i16 %sum.next
}

; CHECK-LABEL: far_volatile_sum:
; CHECK:       extp [[PAGE:r[0-9]+]], #1
; CHECK-NEXT:  movb [[BYTE:r[hl][0-9]+]], [{{r[0-9]+}}+]
; CHECK-NEXT:  movbz {{r[0-9]+}}, [[BYTE]]

define i16 @near_volatile_sum(ptr addrspace(3) %source, i16 %count) {
entry:
  br label %loop

loop:
  %cursor = phi ptr addrspace(3) [ %source, %entry ], [ %next, %loop ]
  %remaining = phi i16 [ %count, %entry ], [ %remaining.next, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %sum.next, %loop ]
  %value = load volatile i8, ptr addrspace(3) %cursor, align 1
  %next = getelementptr i8, ptr addrspace(3) %cursor, i16 1
  %extended = zext i8 %value to i16
  %sum.next = add i16 %sum, %extended
  %remaining.next = add i16 %remaining, -1
  %done = icmp eq i16 %remaining.next, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i16 %sum.next
}

; CHECK-LABEL: near_volatile_sum:
; CHECK-NOT:   extp
; CHECK:       movb [[BYTE:r[hl][0-9]+]], [{{r[0-9]+}}+]
; CHECK-NEXT:  movbz {{r[0-9]+}}, [[BYTE]]

define i16 @far_volatile_word_sum(ptr addrspace(2) %source, i16 %count) {
entry:
  br label %loop

loop:
  %cursor = phi ptr addrspace(2) [ %source, %entry ], [ %next, %loop ]
  %remaining = phi i16 [ %count, %entry ], [ %remaining.next, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %sum.next, %loop ]
  %value = load volatile i16, ptr addrspace(2) %cursor, align 2
  %next = getelementptr i16, ptr addrspace(2) %cursor, i32 1
  %sum.next = add i16 %sum, %value
  %remaining.next = add i16 %remaining, -1
  %done = icmp eq i16 %remaining.next, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i16 %sum.next
}

; CHECK-LABEL: far_volatile_word_sum:
; CHECK:       extp [[PAGE:r[0-9]+]], #1
; CHECK-NEXT:  mov {{r[0-9]+}}, [{{r[0-9]+}}+]

define i16 @near_volatile_word_sum(ptr addrspace(3) %source, i16 %count) {
entry:
  br label %loop

loop:
  %cursor = phi ptr addrspace(3) [ %source, %entry ], [ %next, %loop ]
  %remaining = phi i16 [ %count, %entry ], [ %remaining.next, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %sum.next, %loop ]
  %value = load volatile i16, ptr addrspace(3) %cursor, align 2
  %next = getelementptr i16, ptr addrspace(3) %cursor, i16 1
  %sum.next = add i16 %sum, %value
  %remaining.next = add i16 %remaining, -1
  %done = icmp eq i16 %remaining.next, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i16 %sum.next
}

; CHECK-LABEL: near_volatile_word_sum:
; CHECK-NOT:   extp
; CHECK:       mov {{r[0-9]+}}, [{{r[0-9]+}}+]
