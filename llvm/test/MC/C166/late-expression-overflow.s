; RUN: not llvm-mc -triple=c166 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

mov r4, #too_large
; CHECK: error: value does not fit in the relocation field
scxt r4, too_large
; CHECK: error: direct address must be in the range 0..65535
mov r4, [r5 + too_large]
; CHECK: error: value does not fit in the relocation field
.set too_large, 65536

mov r4, #sof(0x1000000)
; CHECK: error: code address exceeds 24 bits
mov r4, #cof(0x1000000)
; CHECK: error: code address exceeds 24 bits
mov r4, #pof(0x1000000)
; CHECK: error: far data address exceeds 24 bits
mov r4, #dpp1(0x1000000)
; CHECK: error: far data address exceeds 24 bits
mov r4, #dpp2(0x1000000)
; CHECK: error: far data address exceeds 24 bits
mov r4, #sof(-1)
; CHECK: error: code address exceeds 24 bits
mov r4, sof(-1)
; CHECK: error: code address exceeds 24 bits
mov sof(0x1000000), r4
; CHECK: error: code address exceeds 24 bits
mov r4, sof(negative_address)
; CHECK: error: code address exceeds 24 bits
mov sof(above_address_space), r4
; CHECK: error: code address exceeds 24 bits
.set above_address_space, 0x1000000
mov r4, #pof(-1)
; CHECK: error: far data address exceeds 24 bits

.short paged(0x123456)
; CHECK: error: paged pointer requires a 32-bit field
.short paged(external)
; CHECK: error: paged pointer requires a 32-bit field

movb rl0, #byte_high
; CHECK: error: value does not fit in the relocation field
addb rh7, #byte_low
; CHECK: error: value does not fit in the relocation field
.set byte_high, 256
.set byte_low, -129

; Direct addresses must not acquire the signed range of data/immediate fields
; when their constants are defined after the instruction.
mov r4, negative_address
; CHECK: error: direct address must be in the range 0..65535
mov negative_address, r4
; CHECK: error: direct address must be in the range 0..65535
scxt r4, negative_address
; CHECK: error: direct address must be in the range 0..65535
add r4, negative_address
; CHECK: error: direct address must be in the range 0..65535
cmpi1 r4, negative_address
; CHECK: error: direct address must be in the range 0..65535
.set negative_address, -1
mov r4, signed_minimum
; CHECK: error: direct address must be in the range 0..65535
mov r4, below_signed_minimum
; CHECK: error: direct address must be in the range 0..65535
.set signed_minimum, -32768
.set below_signed_minimum, -32769
