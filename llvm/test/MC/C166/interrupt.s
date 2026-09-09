; RUN: llvm-mc -triple=c166-none-elf -show-encoding %s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS
; C166-ABI: interrupt

; PUSH/POP use the direct-register encoding for both GPRs and SFRs.

push r12
; ASM: push r12{{.*}}encoding: [0xec,0xfc]
; DIS: ec fc{{.*}}push r12

pop r12
; ASM: pop r12{{.*}}encoding: [0xfc,0xfc]
; DIS: fc fc{{.*}}pop r12

push dpp0
; ASM: push dpp0{{.*}}encoding: [0xec,0x00]
; DIS: ec 00{{.*}}push dpp0

pop dpp2
; ASM: pop dpp2{{.*}}encoding: [0xfc,0x02]
; DIS: fc 02{{.*}}pop dpp2

push mdc
; ASM: push mdc{{.*}}encoding: [0xec,0x87]
; DIS: ec 87{{.*}}push mdc

scxt mdc, #16
; ASM: scxt mdc, #16{{.*}}encoding: [0xc6,0x87,0x10,0x00]
; DIS: c6 87 10 00{{.*}}scxt mdc, #16

scxt r1, #4660
; ASM: scxt r1, #4660{{.*}}encoding: [0xc6,0xf1,0x34,0x12]
; DIS: c6 f1 34 12{{.*}}scxt r1, #4660

pop mdl
; ASM: pop mdl{{.*}}encoding: [0xfc,0x07]
; DIS: fc 07{{.*}}pop mdl

reti
; ASM: reti{{.*}}encoding: [0xfb,0x88]
; DIS: fb 88{{.*}}reti

push sp
; ASM: push sp{{.*}}encoding: [0xec,0x09]
; DIS: ec 09{{.*}}push sp
pop sp
; ASM: pop sp{{.*}}encoding: [0xfc,0x09]
; DIS: fc 09{{.*}}pop sp
pop r0
; ASM: pop r0{{.*}}encoding: [0xfc,0xf0]
; DIS: fc f0{{.*}}pop r0
pop r15
; ASM: pop r15{{.*}}encoding: [0xfc,0xff]
; DIS: fc ff{{.*}}pop r15
scxt psw, #0
; ASM: scxt psw, #0{{.*}}encoding: [0xc6,0x88,0x00,0x00]
; DIS: c6 88 00 00{{.*}}scxt psw, #0
scxt sp, 65535
; ASM: scxt sp, 65535{{.*}}encoding: [0xd6,0x09,0xff,0xff]
; DIS: d6 09 ff ff{{.*}}scxt sp, 65535
