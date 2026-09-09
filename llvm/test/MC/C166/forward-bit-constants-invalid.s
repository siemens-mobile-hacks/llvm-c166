; RUN: not llvm-mc -triple=c166 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

bset negative
; CHECK: error: packed bit address must be in the range 0..4095
bclr high
; CHECK: error: packed bit address must be in the range 0..4095
bmov negative, high
; CHECK-COUNT-2: error: packed bit address must be in the range 0..4095
bfldl negative, #0, #0
; CHECK: error: bit word address must be in the range 0..255
bfldh word_high, #0, #0
; CHECK: error: bit word address must be in the range 0..255
jb high, external_target
; CHECK: error: packed bit address must be in the range 0..4095
bset external_bit
; CHECK: error: bit operand must resolve to an absolute constant
bfldl external_word, #0, #0
; CHECK: error: bit operand must resolve to an absolute constant
bset local_label
; CHECK: error: bit operand must resolve to an absolute constant
local_label:
.equ negative, -1
.equ high, 4096
.equ word_high, 256
