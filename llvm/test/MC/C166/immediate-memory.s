; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS

; C166 instruction manual: SCXT #data16 uses C6; SCXT mem uses D6.
scxt r4, #0x100
; CHECK: scxt r4, #256{{.*}}encoding: [0xc6,0xf4,0x00,0x01]
; DIS: scxt r4, #256
scxt r4, 0x100
; CHECK: scxt r4, 256{{.*}}encoding: [0xd6,0xf4,0x00,0x01]
; DIS: scxt r4, 256
scxt mdc, #16
; CHECK: scxt mdc, #16{{.*}}encoding: [0xc6,0x87,0x10,0x00]
scxt mdc, 16
; CHECK: scxt mdc, 16{{.*}}encoding: [0xd6,0x87,0x10,0x00]
scxt r0, #0
; CHECK: scxt r0, #0{{.*}}encoding: [0xc6,0xf0,0x00,0x00]
scxt r0, 0
; CHECK: scxt r0, 0{{.*}}encoding: [0xd6,0xf0,0x00,0x00]
scxt r15, #65535
; CHECK: scxt r15, #65535{{.*}}encoding: [0xc6,0xff,0xff,0xff]
scxt r15, 65535
; CHECK: scxt r15, 65535{{.*}}encoding: [0xd6,0xff,0xff,0xff]

mov r4, #7
; CHECK: mov r4, #7{{.*}}encoding: [0xe0,0x74]
mov r4, 7
; CHECK: mov r4, 7{{.*}}encoding: [0xf2,0xf4,0x07,0x00]
add r4, #3
; CHECK: add r4, #3{{.*}}encoding: [0x08,0x43]
addb rl2, #3
; CHECK: addb rl2, #3{{.*}}encoding: [0x09,0x43]
add r4, #256
; CHECK: add r4, #256{{.*}}encoding: [0x06,0xf4,0x00,0x01]
addb rl2, #255
; CHECK: addb rl2, #255{{.*}}encoding: [0x07,0xf4,0xff,0x00]
mov mdc, #256
; CHECK: mov mdc, #256{{.*}}encoding: [0xe6,0x87,0x00,0x01]

scxt r4, #object+2
; CHECK: scxt r4, #object+2
; CHECK: fixup A - offset: 2, value: object+2, kind: FK_Data_2
; DIS: scxt r4, #0
; DIS-NEXT: R_C166_16 object+0x2
scxt r4, object+2
; CHECK: scxt r4, object+2
; CHECK: fixup A - offset: 2, value: object+2, kind: fixup_c166_address16
; DIS: scxt r4, 0
; DIS-NEXT: R_C166_16 object+0x2
