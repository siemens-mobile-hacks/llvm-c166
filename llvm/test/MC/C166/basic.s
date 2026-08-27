; RUN: llvm-mc -triple=c166-none-elf -show-encoding %s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s | llvm-readobj --file-headers - | FileCheck %s --check-prefix=ELF
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

; The encodings below come from the C166 Family Instruction Set Manual,
; and are part of LLVM's C166 target.

mov r12, r13
; ASM: mov r12, r13{{.*}}encoding: [0xf0,0xcd]
; DIS: f0 cd{{.*}}mov r12, r13

mov r13, #10
; ASM: mov r13, #10{{.*}}encoding: [0xe0,0xad]
; DIS: e0 ad{{.*}}mov r13, #10

mov r4, #0x1234
; ASM: mov r4, #4660{{.*}}encoding: [0xe6,0xf4,0x34,0x12]
; DIS: e6 f4 34 12{{.*}}mov r4, #4660

movb rl4, rh4
; ASM: movb rl4, rh4{{.*}}encoding: [0xf1,0x89]
; DIS: f1 89{{.*}}movb rl4, rh4

movb rh7, #15
; ASM: movb rh7, #15{{.*}}encoding: [0xe1,0xff]
; DIS: e1 ff{{.*}}movb rh7, #15

movb rl4, [r12]
; ASM: movb rl4, [r12]{{.*}}encoding: [0xa9,0x8c]
; DIS: a9 8c{{.*}}movb rl4, [r12]

movb [r12], rl1
; ASM: movb [r12], rl1{{.*}}encoding: [0xb9,0x2c]
; DIS: b9 2c{{.*}}movb [r12], rl1

movbz r4, rl1
; ASM: movbz r4, rl1{{.*}}encoding: [0xc0,0x24]
; DIS: c0 24{{.*}}movbz r4, rl1

movbs r5, rl1
; ASM: movbs r5, rl1{{.*}}encoding: [0xd0,0x25]
; DIS: d0 25{{.*}}movbs r5, rl1

add r12, r15
; ASM: add r12, r15{{.*}}encoding: [0x00,0xcf]
; DIS: 00 cf{{.*}}add r12, r15

addc r5, r15
; ASM: addc r5, r15{{.*}}encoding: [0x10,0x5f]
; DIS: 10 5f{{.*}}addc r5, r15

add r0, #6
; ASM: add r0, #6{{.*}}encoding: [0x08,0x06]
; DIS: 08 06{{.*}}add r0, #6

sub r6, r9
; ASM: sub r6, r9{{.*}}encoding: [0x20,0x69]
; DIS: 20 69{{.*}}sub r6, r9

subc r5, r15
; ASM: subc r5, r15{{.*}}encoding: [0x30,0x5f]
; DIS: 30 5f{{.*}}subc r5, r15

sub r0, #6
; ASM: sub r0, #6{{.*}}encoding: [0x28,0x06]
; DIS: 28 06{{.*}}sub r0, #6

shl r14, #1
; ASM: shl r14, #1{{.*}}encoding: [0x5c,0x1e]
; DIS: 5c 1e{{.*}}shl r14, #1

shl r4, r13
; ASM: shl r4, r13{{.*}}encoding: [0x4c,0x4d]
; DIS: 4c 4d{{.*}}shl r4, r13

shr r5, r14
; ASM: shr r5, r14{{.*}}encoding: [0x6c,0x5e]
; DIS: 6c 5e{{.*}}shr r5, r14

shr r6, #2
; ASM: shr r6, #2{{.*}}encoding: [0x7c,0x26]
; DIS: 7c 26{{.*}}shr r6, #2

ashr r7, r15
; ASM: ashr r7, r15{{.*}}encoding: [0xac,0x7f]
; DIS: ac 7f{{.*}}ashr r7, r15

ashr r4, #1
; ASM: ashr r4, #1{{.*}}encoding: [0xbc,0x14]
; DIS: bc 14{{.*}}ashr r4, #1

cmp r14, r11
; ASM: cmp r14, r11{{.*}}encoding: [0x40,0xeb]
; DIS: 40 eb{{.*}}cmp r14, r11

and r4, r5
; ASM: and r4, r5{{.*}}encoding: [0x60,0x45]
; DIS: 60 45{{.*}}and r4, r5

or r10, r7
; ASM: or r10, r7{{.*}}encoding: [0x70,0xa7]
; DIS: 70 a7{{.*}}or r10, r7

xor r3, r2
; ASM: xor r3, r2{{.*}}encoding: [0x50,0x32]
; DIS: 50 32{{.*}}xor r3, r2

mov r4, [r0]
; ASM: mov r4, [r0]{{.*}}encoding: [0xa8,0x40]
; DIS: a8 40{{.*}}mov r4, [r0]

extp r13, #1
; ASM: extp r13, #1{{.*}}encoding: [0xdc,0x4d]
; DIS: dc 4d{{.*}}extp r13, #1

extp r13, #2
; ASM: extp r13, #2{{.*}}encoding: [0xdc,0x5d]
; DIS: dc 5d{{.*}}extp r13, #2

exts r13, #1
; ASM: exts r13, #1{{.*}}encoding: [0xdc,0x0d]
; DIS: dc 0d{{.*}}exts r13, #1

exts r13, #2
; ASM: exts r13, #2{{.*}}encoding: [0xdc,0x1d]
; DIS: dc 1d{{.*}}exts r13, #2

extp 4, #1
; ASM: extp 4, #1{{.*}}encoding: [0xd7,0x40,0x04,0x00]
; DIS: d7 40 04 00{{.*}}extp 4, #1

extp 4, #2
; ASM: extp 4, #2{{.*}}encoding: [0xd7,0x50,0x04,0x00]
; DIS: d7 50 04 00{{.*}}extp 4, #2

mov r4, 9028
; ASM: mov r4, 9028{{.*}}encoding: [0xf2,0xf4,0x44,0x23]
; DIS: f2 f4 44 23{{.*}}mov r4, 9028

mov r15, dpp1
; ASM: mov r15, dpp1{{.*}}encoding: [0xf2,0xff,0x02,0xfe]
; DIS: f2 ff 02 fe{{.*}}mov r15, dpp1

mov r4, mdl
; ASM: mov r4, mdl{{.*}}encoding: [0xf2,0xf4,0x0e,0xfe]
; DIS: f2 f4 0e fe{{.*}}mov r4, mdl

mov mdl, r12
; ASM: mov mdl, r12{{.*}}encoding: [0xf6,0xfc,0x0e,0xfe]
; DIS: f6 fc 0e fe{{.*}}mov mdl, r12

mul r12, r13
; ASM: mul r12, r13{{.*}}encoding: [0x0b,0xcd]
; DIS: 0b cd{{.*}}mul r12, r13
mulu r12, r13
; ASM: mulu r12, r13{{.*}}encoding: [0x1b,0xcd]
; DIS: 1b cd{{.*}}mulu r12, r13

jmps 0x12, 0x3456
; ASM: jmps 18, 13398{{.*}}encoding: [0xfa,0x12,0x56,0x34]
; DIS: fa 12 56 34{{.*}}jmps 18, 13398

div r13
; ASM: div r13{{.*}}encoding: [0x4b,0xdd]
; DIS: 4b dd{{.*}}div r13

divu r13
; ASM: divu r13{{.*}}encoding: [0x5b,0xdd]
; DIS: 5b dd{{.*}}divu r13

mov 9028, r12
; ASM: mov 9028, r12{{.*}}encoding: [0xf6,0xfc,0x44,0x23]
; DIS: f6 fc 44 23{{.*}}mov 9028, r12

movbz r4, 9028
; ASM: movbz r4, 9028{{.*}}encoding: [0xc2,0xf4,0x44,0x23]
; DIS: c2 f4 44 23{{.*}}movbz r4, 9028

movbs r5, 9028
; ASM: movbs r5, 9028{{.*}}encoding: [0xd2,0xf5,0x44,0x23]
; DIS: d2 f5 44 23{{.*}}movbs r5, 9028

movb 9028, rl4
; ASM: movb 9028, rl4{{.*}}encoding: [0xf7,0xf8,0x44,0x23]
; DIS: f7 f8 44 23{{.*}}movb 9028, rl4

mov [r12], r14
; ASM: mov [r12], r14{{.*}}encoding: [0xb8,0xec]
; DIS: b8 ec{{.*}}mov [r12], r14

mov [-r5], r4
; ASM: mov [-r5], r4{{.*}}encoding: [0x88,0x45]
; DIS: 88 45{{.*}}mov [-r5], r4

mov r12, [r0 + #2]
; ASM: mov r12, [r0 + #2]{{.*}}encoding: [0xd4,0xc0,0x02,0x00]
; DIS: d4 c0 02 00{{.*}}mov r12, [r0 + #2]

mov [r0 + #2], r13
; ASM: mov [r0 + #2], r13{{.*}}encoding: [0xc4,0xd0,0x02,0x00]
; DIS: c4 d0 02 00{{.*}}mov [r0 + #2], r13

movb rl5, [r0 + #1]
; ASM: movb rl5, [r0 + #1]{{.*}}encoding: [0xf4,0xa0,0x01,0x00]
; DIS: f4 a0 01 00{{.*}}movb rl5, [r0 + #1]

movb [r0 + #2], rl1
; ASM: movb [r0 + #2], rl1{{.*}}encoding: [0xe4,0x20,0x02,0x00]
; DIS: e4 20 02 00{{.*}}movb [r0 + #2], rl1

calls 0x12, 0x3456
; ASM: calls 18, 13398{{.*}}encoding: [0xda,0x12,0x56,0x34]
; DIS: da 12 56 34{{.*}}calls 18, 13398

atomic #3
; ASM: atomic #3{{.*}}encoding: [0xd1,0x20]
; DIS: d1 20{{.*}}atomic #3

bclr psw.11
; ASM: bclr psw.11{{.*}}encoding: [0xbe,0x88]
; DIS: be 88{{.*}}bclr psw.11

bset psw.11
; ASM: bset psw.11{{.*}}encoding: [0xbf,0x88]
; DIS: bf 88{{.*}}bset psw.11

bclr r0.0
; ASM: bclr r0.0{{.*}}encoding: [0x0e,0xf0]
; DIS: 0e f0{{.*}}bclr r0.0

bset r15.15
; ASM: bset r15.15{{.*}}encoding: [0xff,0xff]
; DIS: ff ff{{.*}}bset r15.15

push r5
; ASM: push r5{{.*}}encoding: [0xec,0xf5]
; DIS: ec f5{{.*}}push r5

.Lbranch_from:
jmpr cc_eq, .Lbranch_to
; ASM: jmpr cc_eq, .Lbranch_to{{.*}}encoding: [0x2d,A]
; ASM: fixup A - offset: 1, value: .Lbranch_to, kind: fixup_c166_pc8
; DIS: 2d 01{{.*}}jmpr cc_eq, 1
nop
.Lbranch_to:
jmpr cc_uc, .Lbranch_from
; ASM: jmpr cc_uc, .Lbranch_from{{.*}}encoding: [0x0d,A]
; ASM: fixup A - offset: 1, value: .Lbranch_from, kind: fixup_c166_pc8
; DIS: 0d fd{{.*}}jmpr cc_uc, 253

rets
; ASM: rets{{.*}}encoding: [0xdb,0x00]
; DIS: db 00{{.*}}rets

nop
; ASM: nop{{.*}}encoding: [0xcc,0x00]
; DIS: cc 00{{.*}}nop

; ELF: Class: 32-bit
; ELF: DataEncoding: LittleEndian
; ELF: Type: Relocatable
; ELF: Machine: EM_C166 (0x74)
