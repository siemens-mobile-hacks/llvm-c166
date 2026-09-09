; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

.equ control, 0xbd
mov sfr(control), #42
; CHECK: encoding: [0xe6,0xbd,0x2a,0x00]
; DIS: mov sfr(189), #42
mov sfr(control), 0x4002
; CHECK: encoding: [0xf2,0xbd,0x02,0x40]
; DIS-NEXT: mov sfr(189), 16386
mov 0x4002, sfr(control)
; CHECK: encoding: [0xf6,0xbd,0x02,0x40]
; DIS-NEXT: mov 16386, sfr(189)
add sfr(0xef), #1
; CHECK: encoding: [0x06,0xef,0x01,0x00]
; DIS-NEXT: add sfr(239), #1
movb sfr(0xbd), #255
; CHECK: encoding: [0xe7,0xbd,0xff,0x00]
; DIS-NEXT: movb sfr(189), #255
movbs sfr(0xbd), 0x4003
; CHECK: encoding: [0xd2,0xbd,0x03,0x40]
; DIS-NEXT: movbs sfr(189), 16387
movbz 0x4004, sfr(0xbd)
; CHECK: encoding: [0xc5,0xbd,0x04,0x40]
; DIS-NEXT: movbz 16388, sfr(189)
push sfr(0xbd)
; CHECK: encoding: [0xec,0xbd]
; DIS-NEXT: push sfr(189)
pop sfr(0xbd)
; CHECK: encoding: [0xfc,0xbd]
; DIS-NEXT: pop sfr(189)
scxt sfr(0xbd), #42
; CHECK: encoding: [0xc6,0xbd,0x2a,0x00]
; DIS-NEXT: scxt sfr(189), #42
pcall sfr(0xbd), 0x4000
; CHECK: encoding: [0xe2,0xbd,0x00,0x40]
; DIS-NEXT: pcall sfr(189), 16384
retp sfr(0xbd)
; CHECK: encoding: [0xeb,0xbd]
; DIS-NEXT: retp sfr(189)
mov r4, sfr(0xbd)
; CHECK: encoding: [0xf2,0xf4,0x7a,0xff]
; DIS-NEXT: mov r4, 65402
mov sfr(0xbd), r4
; CHECK: encoding: [0xf6,0xf4,0x7a,0xff]
; DIS-NEXT: mov 65402, r4
mov sfr(0), #42
; CHECK: encoding: [0xe6,0x00,0x2a,0x00]
; DIS-NEXT: mov dpp0, #42
mov sfr(0x7f), #42
; CHECK: encoding: [0xe6,0x7f,0x2a,0x00]
; DIS-NEXT: mov sfr(127), #42
mov sfr(0x80), #42
; CHECK: encoding: [0xe6,0x80,0x2a,0x00]
; DIS-NEXT: mov sfr(128), #42
