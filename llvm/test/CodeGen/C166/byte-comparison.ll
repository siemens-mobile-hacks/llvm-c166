; RUN: llc -mtriple=c166-none-elf -code-model=large -O2 \
; RUN:   -verify-machineinstrs < %s | FileCheck %s

define i16 @equal_signed_bytes(ptr addrspace(2) %left,
                               ptr addrspace(2) %right) {
entry:
  %left.byte = load i8, ptr addrspace(2) %left, align 1
  %right.byte = load i8, ptr addrspace(2) %right, align 1
  %left.word = sext i8 %left.byte to i16
  %right.word = sext i8 %right.byte to i16
  %equal = icmp eq i16 %left.word, %right.word
  br i1 %equal, label %yes, label %no

yes:
  ret i16 1

no:
  ret i16 0
}

; CHECK-LABEL: equal_signed_bytes:
; CHECK:       movb [[LEFT:r[hl][0-9]+]],
; CHECK:       movb [[RIGHT:r[hl][0-9]+]],
; CHECK-NOT:   movbs
; CHECK:       cmpb {{r[hl][0-9]+}}, {{r[hl][0-9]+}}

define ptr addrspace(2) @find_byte(ptr addrspace(2) %source,
                                   i16 %character) #0 {
entry:
  %character.byte = trunc i16 %character to i8
  %character.word = sext i8 %character.byte to i16
  br label %loop

loop:
  %cursor = phi ptr addrspace(2) [ %source, %entry ], [ %next, %advance ]
  %value.byte = load i8, ptr addrspace(2) %cursor, align 1
  %value.word = sext i8 %value.byte to i16
  %matches = icmp eq i16 %value.word, %character.word
  br i1 %matches, label %return, label %advance

advance:
  %next = getelementptr i8, ptr addrspace(2) %cursor, i32 1
  %at.end = icmp eq i16 %value.word, 0
  br i1 %at.end, label %return, label %loop

return:
  %result = phi ptr addrspace(2) [ %cursor, %loop ], [ null, %advance ]
  ret ptr addrspace(2) %result
}

; CHECK-LABEL: find_byte:
; CHECK-NOT:   mov [-r0]
; CHECK:       movb [[VALUE:r[hl][0-9]+]],
; CHECK:       cmpb {{r[hl][0-9]+}}, {{r[hl][0-9]+}}
; CHECK:       cmpb [[VALUE]], #0
; CHECK:       mov r4, #0
; CHECK-NEXT:  mov r5, #0
; CHECK:       rets

attributes #0 = { minsize optsize }
