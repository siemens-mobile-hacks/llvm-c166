; RUN: not llvm-mc -triple=c166 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

bset negative . 0
; CHECK: error: bit word address must be in the range 0..255
bclr word_high . 0
; CHECK: error: bit word address must be in the range 0..255
bset 0 . bit_high
; CHECK: error: bit number must be in the range 0..15
bmov 0 . negative, 0 . bit_high
; CHECK-COUNT-2: error: bit number must be in the range 0..15
jbc 0 . bit_high, external
; CHECK: error: bit number must be in the range 0..15
bclr UNKNOWN . 1
; CHECK: error: bit operand must resolve to an absolute constant
bset psw . UNKNOWN
; CHECK: error: bit operand must resolve to an absolute constant
bset 0 . sof(1)
; CHECK: error: bit operand must resolve to an absolute constant
.equ negative, -1
.equ word_high, 256
.equ bit_high, 16
