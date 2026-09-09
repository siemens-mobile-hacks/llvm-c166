; RUN: not llvm-mc -triple=c166-none-elf %s 2>&1 | FileCheck %s

mov r4, #65536
; CHECK: error: immediate must be in the range 0..15

add r0, #65536
; CHECK: error: immediate must be in the range 0..7

atomic #0
; CHECK: error: atomic count must be in the range 1..4

bclr psw.16
; CHECK: error: bit number must be in the range 0..15

calls 256, 0
; CHECK: error: expected an 8-bit segment or seg(expression)

calls 0, 65536
; CHECK: error: expected a 16-bit offset or sof(expression)

extp 1024, #1
; CHECK: error: expected a 10-bit page or pag(expression)

extp 4, #5
; CHECK: error: instruction count must be in the range 1..4

mov r4, 65536
; CHECK: error: immediate must be in the range 0..15

mov r16, r0
; CHECK: error: expected a 16-bit absolute address

; Both operands of register-register ALU forms must have the same width.
.irp op, add, addc, sub, subc, cmp, xor, and, or
  \op r0, rl0
  \op rl0, r0
.endr
.irp op, addb, addcb, subb, subcb, cmpb, xorb, andb, orb
  \op rl0, r0
  \op r0, rl0
.endr
; CHECK-COUNT-32: error:
; CHECK-NOT: error:
