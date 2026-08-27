; RUN: not llvm-mc -triple=c166-none-elf %s -o /dev/null 2>&1 | FileCheck %s

add r0, #-1
; CHECK: error: immediate must be in the range 0..7
sub r0, #8
; CHECK: error: immediate must be in the range 0..7

movb rl0, #16
; CHECK: error: immediate must be in the range 0..15
shl r0, #-1
; CHECK: error: immediate must be in the range 0..15

extp r0, #0
; CHECK: error: instruction count must be in the range 1..4
exts r0, #5
; CHECK: error: instruction count must be in the range 1..4
extp -1, #1
; CHECK: error: expected a 10-bit page or pag(expression)

mov r0, [r0 + #65536]
; CHECK: error: immediate must be in the range 0..65535
mov r0, [r0 + #-1]
; CHECK: error: immediate must be in the range 0..65535

mov r0, -1
; CHECK: error: immediate must be in the range 0..15
mov 16384, r0
; CHECK: error: expected a 14-bit page offset or pof(expression)

jmps -1, 0
; CHECK: error: expected an 8-bit segment or seg(expression)
jmps 0, -1
; CHECK: error: expected a 16-bit offset or sof(expression)
jmpa cc_uc, 65536
; CHECK: error: expected a 16-bit code offset or cof(expression)

jmpr cc_eq, 256
; CHECK: error: expected a symbolic branch target or encoded 8-bit displacement
