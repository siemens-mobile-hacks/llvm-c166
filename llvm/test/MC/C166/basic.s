; RUN: llvm-mc -triple=c166-none-elf -show-encoding %s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s | llvm-readobj --file-headers - | FileCheck %s --check-prefix=ELF
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s | llvm-readobj --sections - | FileCheck %s --check-prefix=ALIGN
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

; ALIGN:      Name: .text
; ALIGN:      AddressAlignment: 2

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

movb rl4, [r12+]
; ASM: movb rl4, [r12+]{{.*}}encoding: [0x99,0x8c]
; DIS: 99 8c{{.*}}movb rl4, [r12+]

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

add r4, [r0]
; ASM: add r4, [r0]{{.*}}encoding: [0x08,0x48]
; DIS: 08 48{{.*}}add r4, [r0]

addc r5, [r1]
; ASM: addc r5, [r1]{{.*}}encoding: [0x18,0x59]
; DIS: 18 59{{.*}}addc r5, [r1]

add r4, [r0+]
; ASM: add r4, [r0+]{{.*}}encoding: [0x08,0x4c]
; DIS: 08 4c{{.*}}add r4, [r0+]

addc r5, [r1+]
; ASM: addc r5, [r1+]{{.*}}encoding: [0x18,0x5d]
; DIS: 18 5d{{.*}}addc r5, [r1+]

add r0, #6
; ASM: add r0, #6{{.*}}encoding: [0x08,0x06]
; DIS: 08 06{{.*}}add r0, #6

addb rl4, #3
; ASM: addb rl4, #3{{.*}}encoding: [0x09,0x83]
; DIS: 09 83{{.*}}addb rl4, #3

addb rl4, #200
; ASM: addb rl4, #200{{.*}}encoding: [0x07,0xf8,0xc8,0x00]
; DIS: 07 f8 c8 00{{.*}}addb rl4, #200

subb rh4, #255
; ASM: subb rh4, #255{{.*}}encoding: [0x27,0xf9,0xff,0x00]
; DIS: 27 f9 ff 00{{.*}}subb rh4, #255

cmpb rl5, #128
; ASM: cmpb rl5, #128{{.*}}encoding: [0x47,0xfa,0x80,0x00]
; DIS: 47 fa 80 00{{.*}}cmpb rl5, #128

xorb rh5, #195
; ASM: xorb rh5, #195{{.*}}encoding: [0x57,0xfb,0xc3,0x00]
; DIS: 57 fb c3 00{{.*}}xorb rh5, #195

andb rl6, #240
; ASM: andb rl6, #240{{.*}}encoding: [0x67,0xfc,0xf0,0x00]
; DIS: 67 fc f0 00{{.*}}andb rl6, #240

orb rh6, #128
; ASM: orb rh6, #128{{.*}}encoding: [0x77,0xfd,0x80,0x00]
; DIS: 77 fd 80 00{{.*}}orb rh6, #128

sub r6, r9
; ASM: sub r6, r9{{.*}}encoding: [0x20,0x69]
; DIS: 20 69{{.*}}sub r6, r9

subc r5, r15
; ASM: subc r5, r15{{.*}}encoding: [0x30,0x5f]
; DIS: 30 5f{{.*}}subc r5, r15

sub r6, [r2]
; ASM: sub r6, [r2]{{.*}}encoding: [0x28,0x6a]
; DIS: 28 6a{{.*}}sub r6, [r2]

subc r7, [r3]
; ASM: subc r7, [r3]{{.*}}encoding: [0x38,0x7b]
; DIS: 38 7b{{.*}}subc r7, [r3]

sub r6, [r2+]
; ASM: sub r6, [r2+]{{.*}}encoding: [0x28,0x6e]
; DIS: 28 6e{{.*}}sub r6, [r2+]

subc r7, [r3+]
; ASM: subc r7, [r3+]{{.*}}encoding: [0x38,0x7f]
; DIS: 38 7f{{.*}}subc r7, [r3+]

sub r0, #6
; ASM: sub r0, #6{{.*}}encoding: [0x28,0x06]
; DIS: 28 06{{.*}}sub r0, #6

subb rh4, #2
; ASM: subb rh4, #2{{.*}}encoding: [0x29,0x92]
; DIS: 29 92{{.*}}subb rh4, #2

add r8, #0x1234
; ASM: add r8, #4660{{.*}}encoding: [0x06,0xf8,0x34,0x12]
; DIS: 06 f8 34 12{{.*}}add r8, #4660

addc r5, #7
; ASM: addc r5, #7{{.*}}encoding: [0x18,0x57]
; DIS: 18 57{{.*}}addc r5, #7

addc r5, #0x1234
; ASM: addc r5, #4660{{.*}}encoding: [0x16,0xf5,0x34,0x12]
; DIS: 16 f5 34 12{{.*}}addc r5, #4660

sub r0, #128
; ASM: sub r0, #128{{.*}}encoding: [0x26,0xf0,0x80,0x00]
; DIS: 26 f0 80 00{{.*}}sub r0, #128

subc r6, #7
; ASM: subc r6, #7{{.*}}encoding: [0x38,0x67]
; DIS: 38 67{{.*}}subc r6, #7

subc r6, #0x1234
; ASM: subc r6, #4660{{.*}}encoding: [0x36,0xf6,0x34,0x12]
; DIS: 36 f6 34 12{{.*}}subc r6, #4660

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

prior r4, r5
; ASM: prior r4, r5{{.*}}encoding: [0x2b,0x45]
; DIS: 2b 45{{.*}}prior r4, r5

cmp r14, r11
; ASM: cmp r14, r11{{.*}}encoding: [0x40,0xeb]
; DIS: 40 eb{{.*}}cmp r14, r11

cmp r8, [r0]
; ASM: cmp r8, [r0]{{.*}}encoding: [0x48,0x88]
; DIS: 48 88{{.*}}cmp r8, [r0]

cmp r8, [r0+]
; ASM: cmp r8, [r0+]{{.*}}encoding: [0x48,0x8c]
; DIS: 48 8c{{.*}}cmp r8, [r0+]

cmp r4, #0
; ASM: cmp r4, #0{{.*}}encoding: [0x48,0x40]
; DIS: 48 40{{.*}}cmp r4, #0

cmp r2, #128
; ASM: cmp r2, #128{{.*}}encoding: [0x46,0xf2,0x80,0x00]
; DIS: 46 f2 80 00{{.*}}cmp r2, #128

cmp r4, #10
; ASM: cmp r4, #10{{.*}}encoding: [0x46,0xf4,0x0a,0x00]
; DIS: 46 f4 0a 00{{.*}}cmp r4, #10

cmpb rl4, #5
; ASM: cmpb rl4, #5{{.*}}encoding: [0x49,0x85]
; DIS: 49 85{{.*}}cmpb rl4, #5

negb rl4
; ASM: negb rl4{{.*}}encoding: [0xa1,0x80]
; DIS: a1 80{{.*}}negb rl4

cplb rh4
; ASM: cplb rh4{{.*}}encoding: [0xb1,0x90]
; DIS: b1 90{{.*}}cplb rh4

and r4, r5
; ASM: and r4, r5{{.*}}encoding: [0x60,0x45]
; DIS: 60 45{{.*}}and r4, r5

xor r9, [r1]
; ASM: xor r9, [r1]{{.*}}encoding: [0x58,0x99]
; DIS: 58 99{{.*}}xor r9, [r1]

and r10, [r2]
; ASM: and r10, [r2]{{.*}}encoding: [0x68,0xaa]
; DIS: 68 aa{{.*}}and r10, [r2]

or r11, [r3]
; ASM: or r11, [r3]{{.*}}encoding: [0x78,0xbb]
; DIS: 78 bb{{.*}}or r11, [r3]

xor r9, [r1+]
; ASM: xor r9, [r1+]{{.*}}encoding: [0x58,0x9d]
; DIS: 58 9d{{.*}}xor r9, [r1+]

and r10, [r2+]
; ASM: and r10, [r2+]{{.*}}encoding: [0x68,0xae]
; DIS: 68 ae{{.*}}and r10, [r2+]

or r11, [r3+]
; ASM: or r11, [r3+]{{.*}}encoding: [0x78,0xbf]
; DIS: 78 bf{{.*}}or r11, [r3+]

and r4, #3
; ASM: and r4, #3{{.*}}encoding: [0x68,0x43]
; DIS: 68 43{{.*}}and r4, #3

xorb rl4, #1
; ASM: xorb rl4, #1{{.*}}encoding: [0x59,0x81]
; DIS: 59 81{{.*}}xorb rl4, #1

andb rl4, #6
; ASM: andb rl4, #6{{.*}}encoding: [0x69,0x86]
; DIS: 69 86{{.*}}andb rl4, #6

orb rh4, #7
; ASM: orb rh4, #7{{.*}}encoding: [0x79,0x97]
; DIS: 79 97{{.*}}orb rh4, #7

and r12, #0x3fff
; ASM: and r12, #16383{{.*}}encoding: [0x66,0xfc,0xff,0x3f]
; DIS: 66 fc ff 3f{{.*}}and r12, #16383

or r10, r7
; ASM: or r10, r7{{.*}}encoding: [0x70,0xa7]
; DIS: 70 a7{{.*}}or r10, r7

or r10, #5
; ASM: or r10, #5{{.*}}encoding: [0x78,0xa5]
; DIS: 78 a5{{.*}}or r10, #5

or r10, #0x1234
; ASM: or r10, #4660{{.*}}encoding: [0x76,0xfa,0x34,0x12]
; DIS: 76 fa 34 12{{.*}}or r10, #4660

xor r3, r2
; ASM: xor r3, r2{{.*}}encoding: [0x50,0x32]
; DIS: 50 32{{.*}}xor r3, r2

xor r3, #2
; ASM: xor r3, #2{{.*}}encoding: [0x58,0x32]
; DIS: 58 32{{.*}}xor r3, #2

xor r3, #0x1234
; ASM: xor r3, #4660{{.*}}encoding: [0x56,0xf3,0x34,0x12]
; DIS: 56 f3 34 12{{.*}}xor r3, #4660

mov r4, [r0]
; ASM: mov r4, [r0]{{.*}}encoding: [0xa8,0x40]
; DIS: a8 40{{.*}}mov r4, [r0]

mov [r3], [r2]
; ASM: mov [r3], [r2]{{.*}}encoding: [0xc8,0x32]
; DIS: c8 32{{.*}}mov [r3], [r2]

mov [r3+], [r2]
; ASM: mov [r3+], [r2]{{.*}}encoding: [0xd8,0x32]
; DIS: d8 32{{.*}}mov [r3+], [r2]

mov [r3], [r2+]
; ASM: mov [r3], [r2+]{{.*}}encoding: [0xe8,0x32]
; DIS: e8 32{{.*}}mov [r3], [r2+]

movb [r3], [r2]
; ASM: movb [r3], [r2]{{.*}}encoding: [0xc9,0x32]
; DIS: c9 32{{.*}}movb [r3], [r2]

movb [r3+], [r2]
; ASM: movb [r3+], [r2]{{.*}}encoding: [0xd9,0x32]
; DIS: d9 32{{.*}}movb [r3+], [r2]

movb [r3], [r2+]
; ASM: movb [r3], [r2+]{{.*}}encoding: [0xe9,0x32]
; DIS: e9 32{{.*}}movb [r3], [r2+]

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

add r2, mdl
; ASM: add r2, mdl{{.*}}encoding: [0x02,0xf2,0x0e,0xfe]
; DIS: 02 f2 0e fe{{.*}}add r2, mdl

addc r3, mdh
; ASM: addc r3, mdh{{.*}}encoding: [0x12,0xf3,0x0c,0xfe]
; DIS: 12 f3 0c fe{{.*}}addc r3, mdh

sub r4, mdl
; ASM: sub r4, mdl{{.*}}encoding: [0x22,0xf4,0x0e,0xfe]
; DIS: 22 f4 0e fe{{.*}}sub r4, mdl

subc r5, mdh
; ASM: subc r5, mdh{{.*}}encoding: [0x32,0xf5,0x0c,0xfe]
; DIS: 32 f5 0c fe{{.*}}subc r5, mdh

cmp r6, mdl
; ASM: cmp r6, mdl{{.*}}encoding: [0x42,0xf6,0x0e,0xfe]
; DIS: 42 f6 0e fe{{.*}}cmp r6, mdl

xor r7, mdh
; ASM: xor r7, mdh{{.*}}encoding: [0x52,0xf7,0x0c,0xfe]
; DIS: 52 f7 0c fe{{.*}}xor r7, mdh

and r8, mdl
; ASM: and r8, mdl{{.*}}encoding: [0x62,0xf8,0x0e,0xfe]
; DIS: 62 f8 0e fe{{.*}}and r8, mdl

or r9, mdh
; ASM: or r9, mdh{{.*}}encoding: [0x72,0xf9,0x0c,0xfe]
; DIS: 72 f9 0c fe{{.*}}or r9, mdh

mov mdl, r12
; ASM: mov mdl, r12{{.*}}encoding: [0xf6,0xfc,0x0e,0xfe]
; DIS: f6 fc 0e fe{{.*}}mov mdl, r12

mov mdl, mdh
; ASM: mov mdl, mdh{{.*}}encoding: [0xf2,0x07,0x0c,0xfe]
; DIS: f2 07 0c fe{{.*}}mov mdl, mdh

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

divl r13
; ASM: divl r13{{.*}}encoding: [0x6b,0xdd]
; DIS: 6b dd{{.*}}divl r13

divlu r13
; ASM: divlu r13{{.*}}encoding: [0x7b,0xdd]
; DIS: 7b dd{{.*}}divlu r13

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

mov r6, [r0+]
; ASM: mov r6, [r0+]{{.*}}encoding: [0x98,0x60]
; DIS: 98 60{{.*}}mov r6, [r0+]

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
