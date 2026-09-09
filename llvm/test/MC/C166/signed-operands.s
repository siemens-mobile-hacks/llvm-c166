; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

mov r4, #-1
; CHECK: mov r4, #65535{{.*}}encoding: [0xe6,0xf4,0xff,0xff]
; DIS: mov r4, #65535
mov r4, #-32768
; CHECK: encoding: [0xe6,0xf4,0x00,0x80]
add r4, #-1
; CHECK: encoding: [0x06,0xf4,0xff,0xff]
scxt r4, #-1
; CHECK: encoding: [0xc6,0xf4,0xff,0xff]
mov mdc, #-1
; CHECK: encoding: [0xe6,0x87,0xff,0xff]

mov r4, [r5 - 2]
; CHECK: mov r4, [r5 + #65534]{{.*}}encoding: [0xd4,0x45,0xfe,0xff]
; DIS: mov r4, [r5 + #65534]
mov r4, [r5 + #-2]
; CHECK: encoding: [0xd4,0x45,0xfe,0xff]
mov [r5 - #2], r4
; CHECK: encoding: [0xc4,0x45,0xfe,0xff]
movb rl2, [r5 - 2]
; CHECK: encoding: [0xf4,0x45,0xfe,0xff]
movb [r5 - 2], rl2
; CHECK: encoding: [0xe4,0x45,0xfe,0xff]
mov r4, [r5 - 32768]
; CHECK: encoding: [0xd4,0x45,0x00,0x80]

mov r4, #forward
; CHECK: fixup A - offset: 2, value: forward, kind: FK_Data_2
.set forward, -1
; DIS: mov r4, #65535
