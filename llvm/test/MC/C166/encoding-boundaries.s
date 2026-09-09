; RUN: llvm-mc -triple=c166-none-elf -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=ALU
; RUN: llvm-mc -triple=c166-none-elf -mcpu=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166-none-elf -mcpu=generic -show-encoding %s | FileCheck %s

; Exact boundary encodings from the C166 Family Instruction Set Manual.
; Both CPU names support the extension and atomic instruction boundaries.

; Byte ALU records and the two ends of the byte-register encoding space.
addb rl0, rh7
; CHECK: addb rl0, rh7{{.*}}encoding: [0x01,0x0f]
subb rh7, rl0
; CHECK: subb rh7, rl0{{.*}}encoding: [0x21,0xf0]
cmpb rl0, rh7
; CHECK: cmpb rl0, rh7{{.*}}encoding: [0x41,0x0f]
xorb rh7, rl0
; CHECK: xorb rh7, rl0{{.*}}encoding: [0x51,0xf0]
andb rl0, rh7
; CHECK: andb rl0, rh7{{.*}}encoding: [0x61,0x0f]
orb rh7, rl0
; CHECK: orb rh7, rl0{{.*}}encoding: [0x71,0xf0]

; Compact byte immediates and unary byte operations.
addb rl0, #0
; CHECK: addb rl0, #0{{.*}}encoding: [0x09,0x00]
addb rh7, #7
; CHECK: addb rh7, #7{{.*}}encoding: [0x09,0xf7]
subb rl0, #0
; CHECK: subb rl0, #0{{.*}}encoding: [0x29,0x00]
subb rh7, #7
; CHECK: subb rh7, #7{{.*}}encoding: [0x29,0xf7]
cmpb rl0, #0
; CHECK: cmpb rl0, #0{{.*}}encoding: [0x49,0x00]
cmpb rh7, #7
; CHECK: cmpb rh7, #7{{.*}}encoding: [0x49,0xf7]
xorb rh7, #7
; CHECK: xorb rh7, #7{{.*}}encoding: [0x59,0xf7]
andb rh7, #7
; CHECK: andb rh7, #7{{.*}}encoding: [0x69,0xf7]
orb rh7, #7
; CHECK: orb rh7, #7{{.*}}encoding: [0x79,0xf7]
addb rl0, #8
; CHECK: addb rl0, #8{{.*}}encoding: [0x07,0xf0,0x08,0x00]
orb rh7, #255
; CHECK: orb rh7, #255{{.*}}encoding: [0x77,0xff,0xff,0x00]
negb rl0
; CHECK: negb rl0{{.*}}encoding: [0xa1,0x00]
negb rh7
; CHECK: negb rh7{{.*}}encoding: [0xa1,0xf0]
cplb rl0
; CHECK: cplb rl0{{.*}}encoding: [0xb1,0x00]
cplb rh7
; CHECK: cplb rh7{{.*}}encoding: [0xb1,0xf0]

; Short immediate minima/maxima.
add r0, #0
; CHECK: add r0, #0{{.*}}encoding: [0x08,0x00]
add r15, #7
; CHECK: add r15, #7{{.*}}encoding: [0x08,0xf7]
sub r0, #0
; CHECK: sub r0, #0{{.*}}encoding: [0x28,0x00]
sub r15, #7
; CHECK: sub r15, #7{{.*}}encoding: [0x28,0xf7]
mov r0, #0
; CHECK: mov r0, #0{{.*}}encoding: [0xe0,0x00]
mov r15, #15
; CHECK: mov r15, #15{{.*}}encoding: [0xe0,0xff]
movb rl0, #0
; CHECK: movb rl0, #0{{.*}}encoding: [0xe1,0x00]
movb rh7, #15
; CHECK: movb rh7, #15{{.*}}encoding: [0xe1,0xff]
shl r0, #0
; CHECK: shl r0, #0{{.*}}encoding: [0x5c,0x00]
shl r15, #15
; CHECK: shl r15, #15{{.*}}encoding: [0x5c,0xff]
shr r0, #0
; CHECK: shr r0, #0{{.*}}encoding: [0x7c,0x00]
shr r15, #15
; CHECK: shr r15, #15{{.*}}encoding: [0x7c,0xff]
ashr r0, #0
; CHECK: ashr r0, #0{{.*}}encoding: [0xbc,0x00]
ashr r15, #15
; CHECK: ashr r15, #15{{.*}}encoding: [0xbc,0xff]
mov r0, #16
; CHECK: mov r0, #16{{.*}}encoding: [0xe6,0xf0,0x10,0x00]
mov r15, #65535
; CHECK: mov r15, #65535{{.*}}encoding: [0xe6,0xff,0xff,0xff]

; Extension/atomic count and register/page boundaries.
extp r0, #1
; CHECK: extp r0, #1{{.*}}encoding: [0xdc,0x40]
extp r15, #4
; CHECK: extp r15, #4{{.*}}encoding: [0xdc,0x7f]
exts r0, #1
; CHECK: exts r0, #1{{.*}}encoding: [0xdc,0x00]
exts r15, #4
; CHECK: exts r15, #4{{.*}}encoding: [0xdc,0x3f]
extp 0, #1
; CHECK: extp 0, #1{{.*}}encoding: [0xd7,0x40,0x00,0x00]
extp 1023, #4
; CHECK: extp 1023, #4{{.*}}encoding: [0xd7,0x70,0xff,0x03]
atomic #1
; CHECK: atomic #1{{.*}}encoding: [0xd1,0x00]
atomic #4
; CHECK: atomic #4{{.*}}encoding: [0xd1,0x30]
push r0
; CHECK: push r0{{.*}}encoding: [0xec,0xf0]
push r15
; CHECK: push r15{{.*}}encoding: [0xec,0xff]

; Indirect/base-displacement memory boundaries.
mov r0, [r0]
; CHECK: mov r0, [r0]{{.*}}encoding: [0xa8,0x00]
mov r15, [r15]
; CHECK: mov r15, [r15]{{.*}}encoding: [0xa8,0xff]
movb rl0, [r0]
; CHECK: movb rl0, [r0]{{.*}}encoding: [0xa9,0x00]
movb rh7, [r15]
; CHECK: movb rh7, [r15]{{.*}}encoding: [0xa9,0xff]
mov [r0], r0
; CHECK: mov [r0], r0{{.*}}encoding: [0xb8,0x00]
mov [r15], r15
; CHECK: mov [r15], r15{{.*}}encoding: [0xb8,0xff]
movb [r0], rl0
; CHECK: movb [r0], rl0{{.*}}encoding: [0xb9,0x00]
movb [r15], rh7
; CHECK: movb [r15], rh7{{.*}}encoding: [0xb9,0xff]
mov [-r0], r15
; CHECK: mov [-r0], r15{{.*}}encoding: [0x88,0xf0]
mov [-r15], r0
; CHECK: mov [-r15], r0{{.*}}encoding: [0x88,0x0f]
mov r0, [r0 + #0]
; CHECK: mov r0, [r0 + #0]{{.*}}encoding: [0xd4,0x00,0x00,0x00]
mov r15, [r15 + #65535]
; CHECK: mov r15, [r15 + #65535]{{.*}}encoding: [0xd4,0xff,0xff,0xff]
mov [r0 + #0], r0
; CHECK: mov [r0 + #0], r0{{.*}}encoding: [0xc4,0x00,0x00,0x00]
mov [r15 + #65535], r15
; CHECK: mov [r15 + #65535], r15{{.*}}encoding: [0xc4,0xff,0xff,0xff]
movb rl0, [r0 + #0]
; CHECK: movb rl0, [r0 + #0]{{.*}}encoding: [0xf4,0x00,0x00,0x00]
movb rh7, [r15 + #65535]
; CHECK: movb rh7, [r15 + #65535]{{.*}}encoding: [0xf4,0xff,0xff,0xff]
movb [r0 + #0], rl0
; CHECK: movb [r0 + #0], rl0{{.*}}encoding: [0xe4,0x00,0x00,0x00]
movb [r15 + #65535], rh7
; CHECK: movb [r15 + #65535], rh7{{.*}}encoding: [0xe4,0xff,0xff,0xff]

; Direct 14-bit address and direct-register boundaries.
mov r0, 0
; CHECK: mov r0, 0{{.*}}encoding: [0xf2,0xf0,0x00,0x00]
mov r15, 16383
; CHECK: mov r15, 16383{{.*}}encoding: [0xf2,0xff,0xff,0x3f]
mov 0, r0
; CHECK: mov 0, r0{{.*}}encoding: [0xf6,0xf0,0x00,0x00]
mov 16383, r15
; CHECK: mov 16383, r15{{.*}}encoding: [0xf6,0xff,0xff,0x3f]
movbz r0, 0
; CHECK: movbz r0, 0{{.*}}encoding: [0xc2,0xf0,0x00,0x00]
movbs r15, 16383
; CHECK: movbs r15, 16383{{.*}}encoding: [0xd2,0xff,0xff,0x3f]
movb 0, rl0
; CHECK: movb 0, rl0{{.*}}encoding: [0xf7,0xf0,0x00,0x00]
movb 16383, rh7
; CHECK: movb 16383, rh7{{.*}}encoding: [0xf7,0xff,0xff,0x3f]
mov r0, dpp0
; CHECK: mov r0, dpp0{{.*}}encoding: [0xf2,0xf0,0x00,0xfe]
mov r1, dpp1
; CHECK: mov r1, dpp1{{.*}}encoding: [0xf2,0xf1,0x02,0xfe]
mov r2, dpp2
; CHECK: mov r2, dpp2{{.*}}encoding: [0xf2,0xf2,0x04,0xfe]
mov r3, dpp3
; CHECK: mov r3, dpp3{{.*}}encoding: [0xf2,0xf3,0x06,0xfe]
mov r2, csp
; CHECK: mov r2, csp{{.*}}encoding: [0xf2,0xf2,0x08,0xfe]
mov r4, cp
; CHECK: mov r4, cp{{.*}}encoding: [0xf2,0xf4,0x10,0xfe]
mov r5, sp
; CHECK: mov r5, sp{{.*}}encoding: [0xf2,0xf5,0x12,0xfe]
mov r6, stkov
; CHECK: mov r6, stkov{{.*}}encoding: [0xf2,0xf6,0x14,0xfe]
mov r7, stkun
; CHECK: mov r7, stkun{{.*}}encoding: [0xf2,0xf7,0x16,0xfe]
mov r8, psw
; CHECK: mov r8, psw{{.*}}encoding: [0xf2,0xf8,0x10,0xff]
mov psw, r15
; CHECK: mov psw, r15{{.*}}encoding: [0xf6,0xff,0x10,0xff]

; MOV reg,#data16 uses the short SFR address byte documented by the C166
; Family Instruction Set Manual.  These are the complete startup-state SFRs.
mov dpp0, #0
; CHECK: mov dpp0, #0{{.*}}encoding: [0xe6,0x00,0x00,0x00]
mov dpp1, #1
; CHECK: mov dpp1, #1{{.*}}encoding: [0xe6,0x01,0x01,0x00]
mov dpp2, #2
; CHECK: mov dpp2, #2{{.*}}encoding: [0xe6,0x02,0x02,0x00]
mov dpp3, #3
; CHECK: mov dpp3, #3{{.*}}encoding: [0xe6,0x03,0x03,0x00]
mov cp, #64512
; CHECK: mov cp, #64512{{.*}}encoding: [0xe6,0x08,0x00,0xfc]
mov sp, #64512
; CHECK: mov sp, #64512{{.*}}encoding: [0xe6,0x09,0x00,0xfc]
mov stkov, #64000
; CHECK: mov stkov, #64000{{.*}}encoding: [0xe6,0x0a,0x00,0xfa]
mov stkun, #64512
; CHECK: mov stkun, #64512{{.*}}encoding: [0xe6,0x0b,0x00,0xfc]
mov psw, #0
; CHECK: mov psw, #0{{.*}}encoding: [0xe6,0x88,0x00,0x00]
diswdt
; CHECK: diswdt{{.*}}encoding: [0xa5,0x5a,0xa5,0xa5]

; Every JMPR condition record, including both PC8 endpoint bit patterns.
jmpr cc_uc, 0
; CHECK: jmpr cc_uc, 0{{.*}}encoding: [0x0d,0x00]
jmpr cc_eq, 255
; CHECK: jmpr cc_eq, 255{{.*}}encoding: [0x2d,0xff]
jmpr cc_ne, 0
; CHECK: jmpr cc_ne, 0{{.*}}encoding: [0x3d,0x00]
jmpr cc_ult, 255
; CHECK: jmpr cc_ult, 255{{.*}}encoding: [0x8d,0xff]
jmpr cc_uge, 0
; CHECK: jmpr cc_uge, 0{{.*}}encoding: [0x9d,0x00]
jmpr cc_sgt, 255
; CHECK: jmpr cc_sgt, 255{{.*}}encoding: [0xad,0xff]
jmpr cc_sle, 0
; CHECK: jmpr cc_sle, 0{{.*}}encoding: [0xbd,0x00]
jmpr cc_slt, 255
; CHECK: jmpr cc_slt, 255{{.*}}encoding: [0xcd,0xff]
jmpr cc_sge, 0
; CHECK: jmpr cc_sge, 0{{.*}}encoding: [0xdd,0x00]
jmpr cc_ugt, 255
; CHECK: jmpr cc_ugt, 255{{.*}}encoding: [0xed,0xff]
jmpr cc_ule, 0
; CHECK: jmpr cc_ule, 0{{.*}}encoding: [0xfd,0x00]

; Near/far direct and indirect control-flow boundaries.
jmps 0, 0
; CHECK: jmps 0, 0{{.*}}encoding: [0xfa,0x00,0x00,0x00]
jmps 255, 65535
; CHECK: jmps 255, 65535{{.*}}encoding: [0xfa,0xff,0xff,0xff]
jmpa cc_uc, 0
; CHECK: jmpa cc_uc, 0{{.*}}encoding: [0xea,0x00,0x00,0x00]
jmpa cc_uc, 65535
; CHECK: jmpa cc_uc, 65535{{.*}}encoding: [0xea,0x00,0xff,0xff]
jmpi cc_uc, [r0]
; CHECK: jmpi cc_uc, [r0]{{.*}}encoding: [0x9c,0x00]
jmpi cc_uc, [r15]
; CHECK: jmpi cc_uc, [r15]{{.*}}encoding: [0x9c,0x0f]
calls 0, 0
; CHECK: calls 0, 0{{.*}}encoding: [0xda,0x00,0x00,0x00]
calls 255, 65535
; CHECK: calls 255, 65535{{.*}}encoding: [0xda,0xff,0xff,0xff]
calla cc_uc, 0
; CHECK: calla cc_uc, 0{{.*}}encoding: [0xca,0x00,0x00,0x00]
calla cc_uc, 65535
; CHECK: calla cc_uc, 65535{{.*}}encoding: [0xca,0x00,0xff,0xff]
calli cc_uc, [r0]
; CHECK: calli cc_uc, [r0]{{.*}}encoding: [0xab,0x00]
calli cc_uc, [r15]
; CHECK: calli cc_uc, [r15]{{.*}}encoding: [0xab,0x0f]
ret
; CHECK: ret{{.*}}encoding: [0xcb,0x00]

; Remaining compact ALU immediate/register boundaries.
addc r0, #0
; CHECK: addc r0, #0{{.*}}encoding: [0x18,0x00]
; ALU: addc r0, #0
addc r15, #7
; CHECK: addc r15, #7{{.*}}encoding: [0x18,0xf7]
; ALU: addc r15, #7
subc r0, #0
; CHECK: subc r0, #0{{.*}}encoding: [0x38,0x00]
; ALU: subc r0, #0
subc r15, #7
; CHECK: subc r15, #7{{.*}}encoding: [0x38,0xf7]
; ALU: subc r15, #7
cmp r0, #0
; CHECK: cmp r0, #0{{.*}}encoding: [0x48,0x00]
; ALU: cmp r0, #0
cmp r15, #7
; CHECK: cmp r15, #7{{.*}}encoding: [0x48,0xf7]
; ALU: cmp r15, #7
xor r0, #0
; CHECK: xor r0, #0{{.*}}encoding: [0x58,0x00]
; ALU: xor r0, #0
xor r15, #7
; CHECK: xor r15, #7{{.*}}encoding: [0x58,0xf7]
; ALU: xor r15, #7
and r0, #0
; CHECK: and r0, #0{{.*}}encoding: [0x68,0x00]
; ALU: and r0, #0
and r15, #7
; CHECK: and r15, #7{{.*}}encoding: [0x68,0xf7]
; ALU: and r15, #7
or r0, #0
; CHECK: or r0, #0{{.*}}encoding: [0x78,0x00]
; ALU: or r0, #0
or r15, #7
; CHECK: or r15, #7{{.*}}encoding: [0x78,0xf7]
; ALU: or r15, #7
addcb rl0, #0
; CHECK: addcb rl0, #0{{.*}}encoding: [0x19,0x00]
; ALU: addcb rl0, #0
addcb rh7, #7
; CHECK: addcb rh7, #7{{.*}}encoding: [0x19,0xf7]
; ALU: addcb rh7, #7
subcb rl0, #0
; CHECK: subcb rl0, #0{{.*}}encoding: [0x39,0x00]
; ALU: subcb rl0, #0
subcb rh7, #7
; CHECK: subcb rh7, #7{{.*}}encoding: [0x39,0xf7]
; ALU: subcb rh7, #7
xorb rl0, #0
; CHECK: xorb rl0, #0{{.*}}encoding: [0x59,0x00]
; ALU: xorb rl0, #0
andb rl0, #0
; CHECK: andb rl0, #0{{.*}}encoding: [0x69,0x00]
; ALU: andb rl0, #0
orb rl0, #0
; CHECK: orb rl0, #0{{.*}}encoding: [0x79,0x00]
; ALU: orb rl0, #0
