; RUN: llvm-mc -triple=c166-none-elf -show-encoding %s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

cmpd1 r14, #0
; ASM: cmpd1 r14, #0{{.*}}encoding: [0xa0,0x0e]
; DIS: a0 0e{{.*}}cmpd1 r14, #0

cmpd1 r1, #0x1234
; ASM: cmpd1 r1, #4660{{.*}}encoding: [0xa6,0xf1,0x34,0x12]
; DIS: a6 f1 34 12{{.*}}cmpd1 r1, #4660

cmpd2 r2, #3
; ASM: cmpd2 r2, #3{{.*}}encoding: [0xb0,0x32]
; DIS: b0 32{{.*}}cmpd2 r2, #3

cmpd2 r3, #0x5678
; ASM: cmpd2 r3, #22136{{.*}}encoding: [0xb6,0xf3,0x78,0x56]
; DIS: b6 f3 78 56{{.*}}cmpd2 r3, #22136

cmpi1 r4, #5
; ASM: cmpi1 r4, #5{{.*}}encoding: [0x80,0x54]
; DIS: 80 54{{.*}}cmpi1 r4, #5

cmpi1 r5, #0xabcd
; ASM: cmpi1 r5, #43981{{.*}}encoding: [0x86,0xf5,0xcd,0xab]
; DIS: 86 f5 cd ab{{.*}}cmpi1 r5, #43981

cmpi2 r6, #7
; ASM: cmpi2 r6, #7{{.*}}encoding: [0x90,0x76]
; DIS: 90 76{{.*}}cmpi2 r6, #7

cmpi2 r7, #0x9abc
; ASM: cmpi2 r7, #39612{{.*}}encoding: [0x96,0xf7,0xbc,0x9a]
; DIS: 96 f7 bc 9a{{.*}}cmpi2 r7, #39612
