; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

define i32 @unsigned_words(i16 %left, i16 %right) {
; CHECK-LABEL: unsigned_words:
; CHECK-NOT:   calls
; CHECK:       mulu {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  mov r4, mdl
; CHECK-NEXT:  mov r5, mdh
  %left.wide = zext i16 %left to i32
  %right.wide = zext i16 %right to i32
  %product = mul i32 %left.wide, %right.wide
  ret i32 %product
}

define i32 @unsigned_words_across_block(i16 %left, i16 %right) {
; CHECK-LABEL: unsigned_words_across_block:
; CHECK-NOT:   calls
; CHECK:       mulu {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  mov r4, mdl
; CHECK-NEXT:  mov r5, mdh
entry:
  %left.wide = zext i16 %left to i32
  br label %multiply

multiply:
  %right.wide = zext i16 %right to i32
  %product = mul i32 %left.wide, %right.wide
  ret i32 %product
}

define i32 @unsigned_multiply_add(i16 %left, i16 %right, i32 %addend) {
; CHECK-LABEL: unsigned_multiply_add:
; CHECK-NOT:   calls
; CHECK:       mulu {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  add {{r[0-9]+}}, mdl
; CHECK-NEXT:  addc {{r[0-9]+}}, mdh
  %left.wide = zext i16 %left to i32
  %right.wide = zext i16 %right to i32
  %product = mul i32 %left.wide, %right.wide
  %sum = add i32 %product, %addend
  ret i32 %sum
}

define i32 @signed_multiply_add(i16 %left, i16 %right, i32 %addend) {
; CHECK-LABEL: signed_multiply_add:
; CHECK-NOT:   calls
; CHECK:       mul {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  add {{r[0-9]+}}, mdl
; CHECK-NEXT:  addc {{r[0-9]+}}, mdh
  %left.wide = sext i16 %left to i32
  %right.wide = sext i16 %right to i32
  %product = mul i32 %left.wide, %right.wide
  %sum = add i32 %product, %addend
  ret i32 %sum
}

define i32 @signed_words(i16 %left, i16 %right) {
; CHECK-LABEL: signed_words:
; CHECK-NOT:   calls
; CHECK:       mul {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  mov r4, mdl
; CHECK-NEXT:  mov r5, mdh
  %left.wide = sext i16 %left to i32
  %right.wide = sext i16 %right to i32
  %product = mul i32 %left.wide, %right.wide
  ret i32 %product
}

define i32 @signed_words_across_block(i16 %left, i16 %right) {
; CHECK-LABEL: signed_words_across_block:
; CHECK-NOT:   calls
; CHECK:       mul {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  mov r4, mdl
; CHECK-NEXT:  mov r5, mdh
entry:
  %left.wide = sext i16 %left to i32
  br label %multiply

multiply:
  %right.wide = sext i16 %right to i32
  %product = mul i32 %left.wide, %right.wide
  ret i32 %product
}

define i32 @masked_words(i32 %left, i32 %right) {
; CHECK-LABEL: masked_words:
; CHECK-NOT:   calls
; CHECK:       mulu {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  mov r4, mdl
; CHECK-NEXT:  mov r5, mdh
  %left.word = and i32 %left, 65535
  %right.word = and i32 %right, 65535
  %product = mul i32 %left.word, %right.word
  ret i32 %product
}

define i32 @replace_high_word(i32 %low_source, i16 %high) {
; CHECK-LABEL: replace_high_word:
; CHECK:       mov r4, r12
; CHECK-NEXT:  mov r5, r14
; CHECK-NEXT:  rets
  %low = and i32 %low_source, 65535
  %high.wide = zext i16 %high to i32
  %high.positioned = shl i32 %high.wide, 16
  %result = or disjoint i32 %low, %high.positioned
  ret i32 %result
}

define i32 @pack_words(i16 %low, i16 %high) {
; CHECK-LABEL: pack_words:
; CHECK:       mov r4, r12
; CHECK-NEXT:  mov r5, r13
; CHECK-NEXT:  rets
  %low.wide = zext i16 %low to i32
  %high.wide = zext i16 %high to i32
  %high.positioned = shl i32 %high.wide, 16
  %result = or disjoint i32 %low.wide, %high.positioned
  ret i32 %result
}

define i32 @full_width(i32 %left, i32 %right) {
; CHECK-LABEL: full_width:
; CHECK:       calls seg(___mulsi3), sof(___mulsi3)
  %product = mul i32 %left, %right
  ret i32 %product
}

define i32 @mixed_width_across_block(i16 %left, i32 %right) {
; CHECK-LABEL: mixed_width_across_block:
; CHECK:       calls seg(___mulsi3), sof(___mulsi3)
entry:
  %left.wide = zext i16 %left to i32
  br label %multiply

multiply:
  %product = mul i32 %left.wide, %right
  ret i32 %product
}

define i32 @mixed_signedness_across_block(i16 %left, i16 %right) {
; CHECK-LABEL: mixed_signedness_across_block:
; CHECK:       calls seg(___mulsi3), sof(___mulsi3)
entry:
  %left.wide = zext i16 %left to i32
  br label %multiply

multiply:
  %right.wide = sext i16 %right to i32
  %product = mul i32 %left.wide, %right.wide
  ret i32 %product
}
