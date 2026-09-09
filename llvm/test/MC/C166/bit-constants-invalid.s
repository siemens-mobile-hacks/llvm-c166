; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s

.equ HIGH, 256
.equ NEGATIVE, -1
.equ BIT, 16
.equ PORT, 0x88
bset HIGH.3
; CHECK: error: bit word address must be in the range 0..255
bset NEGATIVE.3
; CHECK: error: bit word address must be in the range 0..255
bclr PORT.BIT
; CHECK: error: bit number must be in the range 0..15
bclr PORT . (-1)
; CHECK: error: bit number must be in the range 0..15
; Unresolved components are checked in split-bit-constants-invalid.s at layout.

; Neither a packed bit operand nor a bit-word operand can truncate its value.
bset -1
; CHECK: error: expected an 8-bit word address and bit number 0..15
bclr 4096
; CHECK: error: expected an 8-bit word address and bit number 0..15
bmov 0, 4096
; CHECK: error: expected an 8-bit word address and bit number 0..15
bmov -1, 0
; CHECK: error: expected an 8-bit word address and bit number 0..15
bfldl -1, #0, #0
; CHECK: error: expected a bit-addressable direct register or 8-bit word
bfldh 256, #0, #0
; CHECK: error: expected a bit-addressable direct register or 8-bit word
bfldl 0, #-1, #0
; CHECK: error:
; CHECK-NEXT: bfldl 0, #-1, #0
bfldh 0, #0, #256
; CHECK: error:
; CHECK-NEXT: bfldh 0, #0, #256

; An explicitly defined dotted symbol wins even when its value is invalid.
.equ psw.3, 4096
bset psw.3
; CHECK: error: expected an 8-bit word address and bit number 0..15
