; RUN: llc -mtriple=c166 -stop-after=finalize-isel -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=c166 -verify-machineinstrs < %s -o - | FileCheck %s --check-prefix=ASM

declare void @sink(i16) addrspace(1)

define void @branch_ult(i32 %lhs, i32 %rhs) addrspace(1) {
entry:
  %condition = icmp ult i32 %lhs, %rhs
  br i1 %condition, label %less, label %not_less

less:
  call void @sink(i16 1)
  ret void

not_less:
  call void @sink(i16 0)
  ret void
}

; A relational i32 branch is lowered to an atomic subtract-and-branch pseudo.
; Its false edge must remain explicit until block placement: custom inserters
; may create unrelated blocks between the branch and its false successor.
; CHECK-LABEL: name: branch_ult
; CHECK: SUB32BR
; CHECK-NEXT: BR

define i16 @branch_ult_reuses_words(i16 %low, i16 %high, i32 %rhs)
    addrspace(1) {
entry:
  %low.ext = zext i16 %low to i32
  %high.ext = zext i16 %high to i32
  %high.shift = shl i32 %high.ext, 16
  %lhs = or i32 %high.shift, %low.ext
  %condition = icmp ult i32 %lhs, %rhs
  br i1 %condition, label %less, label %not_less

less:
  %less.result = add i16 %low, 1
  ret i16 %less.result

not_less:
  %not-less.result = add i16 %high, 2
  ret i16 %not-less.result
}

; Keep a live pair intact and compare its words without allocating a tied
; subtract result.
; CHECK-LABEL: name: branch_ult_reuses_words
; CHECK-NOT: SUB32BR
; CHECK-COUNT-3: CMPBR

; ASM-LABEL: _branch_ult_reuses_words:
; ASM-NOT: sub
; ASM: cmp
; ASM: jmpr cc_ule,
; ASM: jmpr cc_ult,
; ASM: cmp
; ASM: jmpr cc_uge,

define void @branch_sgt_negative_one(i32 %value) addrspace(1) {
entry:
  %condition = icmp sgt i32 %value, -1
  br i1 %condition, label %nonnegative, label %negative

nonnegative:
  call void @sink(i16 1)
  ret void

negative:
  call void @sink(i16 0)
  ret void
}

; CHECK-LABEL: name: branch_sgt_negative_one
; CHECK-NOT:   SUB32BR
; CHECK:       BITBR %{{[0-9]+}}.sub_hi16, 15, 1
; CHECK-NEXT:  BR

; ASM-LABEL: _branch_sgt_negative_one:
; ASM-NOT:   sub
; ASM:       jb r13.15

define i32 @select_sgt_negative_one(i32 %value, i32 %nonnegative,
                                    i32 %negative) addrspace(1) {
entry:
  %condition = icmp sgt i32 %value, -1
  %result = select i1 %condition, i32 %nonnegative, i32 %negative
  ret i32 %result
}

; CHECK-LABEL: name: select_sgt_negative_one
; CHECK-NOT:   SUB32BR
; CHECK:       BITBR %{{[0-9]+}}.sub_hi16, 15, 0
; CHECK-NEXT:  BR

; ASM-LABEL: _select_sgt_negative_one:
; ASM-NOT:   sub
; ASM:       jnb r13.15

define void @branch_sle_negative_one(i32 %value) addrspace(1) {
entry:
  %condition = icmp sle i32 %value, -1
  br i1 %condition, label %negative, label %nonnegative

negative:
  call void @sink(i16 1)
  ret void

nonnegative:
  call void @sink(i16 0)
  ret void
}

; CHECK-LABEL: name: branch_sle_negative_one
; CHECK-NOT:   SUB32BR
; CHECK:       BITBR %{{[0-9]+}}.sub_hi16, 15, 0
; CHECK-NEXT:  BR

; ASM-LABEL: _branch_sle_negative_one:
; ASM-NOT:   sub
; ASM:       jnb r13.15

define i32 @select_sle_negative_one(i32 %value, i32 %negative,
                                    i32 %nonnegative) addrspace(1) {
entry:
  %condition = icmp sle i32 %value, -1
  %result = select i1 %condition, i32 %negative, i32 %nonnegative
  ret i32 %result
}

; CHECK-LABEL: name: select_sle_negative_one
; CHECK-NOT:   SUB32BR
; CHECK:       BITBR %{{[0-9]+}}.sub_hi16, 15, 1
; CHECK-NEXT:  BR

; ASM-LABEL: _select_sle_negative_one:
; ASM-NOT:   sub
; ASM:       jb r13.15
