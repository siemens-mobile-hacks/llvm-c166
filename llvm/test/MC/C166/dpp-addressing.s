; RUN: llvm-mc -triple=c166-none-elf -show-encoding %s | FileCheck %s --check-prefix=ENC
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s -o - | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

; DPP1/DPP2 are encoded in bits 31:30.  The disassembler must preserve the
; qualifier: printing these operands as plain 14-bit addresses makes a
; disassemble/reassemble round trip silently select DPP0 instead.

mov r0, dpp1(0)
; ENC: mov r0, dpp1(0){{.*}}encoding: [0xf2,0xf0,A,0x40'A']
; DIS: f2 f0 00 40{{.*}}mov r0, dpp1(0)

mov r15, dpp2(16383)
; ENC: mov r15, dpp2(16383){{.*}}encoding: [0xf2,0xff,A,0x80'A']
; DIS: f2 ff ff bf{{.*}}mov r15, dpp2(16383)

mov dpp1(0), r0
; ENC: mov dpp1(0), r0{{.*}}encoding: [0xf6,0xf0,A,0x40'A']
; DIS: f6 f0 00 40{{.*}}mov dpp1(0), r0

mov dpp2(16383), r15
; ENC: mov dpp2(16383), r15{{.*}}encoding: [0xf6,0xff,A,0x80'A']
; DIS: f6 ff ff bf{{.*}}mov dpp2(16383), r15

movbz r0, dpp1(0)
; ENC: movbz r0, dpp1(0){{.*}}encoding: [0xc2,0xf0,A,0x40'A']
; DIS: c2 f0 00 40{{.*}}movbz r0, dpp1(0)

movbz r15, dpp2(16383)
; ENC: movbz r15, dpp2(16383){{.*}}encoding: [0xc2,0xff,A,0x80'A']
; DIS: c2 ff ff bf{{.*}}movbz r15, dpp2(16383)

movbs r0, dpp1(0)
; ENC: movbs r0, dpp1(0){{.*}}encoding: [0xd2,0xf0,A,0x40'A']
; DIS: d2 f0 00 40{{.*}}movbs r0, dpp1(0)

movbs r15, dpp2(16383)
; ENC: movbs r15, dpp2(16383){{.*}}encoding: [0xd2,0xff,A,0x80'A']
; DIS: d2 ff ff bf{{.*}}movbs r15, dpp2(16383)

movb dpp1(0), rl0
; ENC: movb dpp1(0), rl0{{.*}}encoding: [0xf7,0xf0,A,0x40'A']
; DIS: f7 f0 00 40{{.*}}movb dpp1(0), rl0

movb dpp2(16383), rh7
; ENC: movb dpp2(16383), rh7{{.*}}encoding: [0xf7,0xff,A,0x80'A']
; DIS: f7 ff ff bf{{.*}}movb dpp2(16383), rh7
