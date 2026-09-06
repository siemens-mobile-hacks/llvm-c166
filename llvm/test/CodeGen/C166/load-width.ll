; RUN: llc -mtriple=c166 -O2 < %s | FileCheck %s

define i16 @load_low_bit(ptr %address) {
; CHECK-LABEL: load_low_bit:
; CHECK:       mov {{r[0-9]+}}, [{{r[0-9]+}}]
; CHECK-NOT:   movb
; CHECK-NOT:   movbz
  %value = load i16, ptr %address, align 2
  %bit = and i16 %value, 1
  ret i16 %bit
}

define i16 @load_byte(ptr %address) {
; CHECK-LABEL: load_byte:
; CHECK:       movb {{rl[0-7]}}, [{{r[0-9]+}}]
; CHECK-NEXT:  movbz {{r[0-9]+}}, {{rl[0-7]}}
  %value = load i8, ptr %address
  %extended = zext i8 %value to i16
  ret i16 %extended
}

define i16 @load_low_byte_from_word(ptr %address) {
; CHECK-LABEL: load_low_byte_from_word:
; CHECK:       movb {{rl[0-7]}}, [{{r[0-9]+}}]
; CHECK-NEXT:  movbz {{r[0-9]+}}, {{rl[0-7]}}
  %word = load i16, ptr %address, align 2
  %byte = and i16 %word, 255
  ret i16 %byte
}

define i16 @load_high_byte(ptr %address) {
; CHECK-LABEL: load_high_byte:
; CHECK:       movb {{rl[0-7]}}, [{{r[0-9]+}} + #1]
; CHECK-NEXT:  movbz {{r[0-9]+}}, {{rl[0-7]}}
  %word = load i16, ptr %address, align 2
  %shifted = lshr i16 %word, 8
  %byte = and i16 %shifted, 255
  ret i16 %byte
}

define i16 @load_low_bit_with_chain(ptr %first, ptr %second) {
; CHECK-LABEL: load_low_bit_with_chain:
; CHECK:       mov {{r[0-9]+}}, [{{r[0-9]+}}]
; CHECK-NOT:   movbz
  %first.value = load i16, ptr %first, align 2
  %second.value = load i16, ptr %second, align 2
  %bit = and i16 %first.value, 1
  %result = add i16 %bit, %second.value
  ret i16 %result
}
