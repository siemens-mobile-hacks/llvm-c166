; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
mov [r0], 0x4002
; CHECK: encoding: [0x84,0x00,0x02,0x40]
; DIS: mov [r0], 16386
mov 0xffff, [r15]
; CHECK: encoding: [0x94,0x0f,0xff,0xff]
; DIS-NEXT: mov 65535, [r15]
movb [r15], 0x4003
; CHECK: encoding: [0xa4,0x0f,0x03,0x40]
; DIS-NEXT: movb [r15], 16387
movb 0xffff, [r0]
; CHECK: encoding: [0xb4,0x00,0xff,0xff]
; DIS-NEXT: movb 65535, [r0]
movb [-r0], rh7
; CHECK: encoding: [0x89,0xf0]
; DIS-NEXT: movb [-r0], rh7
movb [-r15], rl0
; CHECK: encoding: [0x89,0x0f]
; DIS-NEXT: movb [-r15], rl0

mov [r0], [r15]
; CHECK: encoding: [0xc8,0x0f]
; DIS-NEXT: mov [r0], [r15]
mov [r15+], [r0]
; CHECK: encoding: [0xd8,0xf0]
; DIS-NEXT: mov [r15+], [r0]
mov [r0], [r15+]
; CHECK: encoding: [0xe8,0x0f]
; DIS-NEXT: mov [r0], [r15+]
movb [r15], [r0]
; CHECK: encoding: [0xc9,0xf0]
; DIS-NEXT: movb [r15], [r0]
movb [r0+], [r15]
; CHECK: encoding: [0xd9,0x0f]
; DIS-NEXT: movb [r0+], [r15]
movb [r15], [r0+]
; CHECK: encoding: [0xe9,0xf0]
; DIS-NEXT: movb [r15], [r0+]
mov r0, [r15+]
; CHECK: encoding: [0x98,0x0f]
; DIS-NEXT: mov r0, [r15+]
movb rh7, [r15+]
; CHECK: encoding: [0x99,0xff]
; DIS-NEXT: movb rh7, [r15+]
mov [-r15], r0
; CHECK: encoding: [0x88,0x0f]
; DIS-NEXT: mov [-r15], r0
mov r15, [r0 + #65535]
; CHECK: encoding: [0xd4,0xf0,0xff,0xff]
; DIS-NEXT: mov r15, [r0 + #65535]
mov [r15 + #65535], r0
; CHECK: encoding: [0xc4,0x0f,0xff,0xff]
; DIS-NEXT: mov [r15 + #65535], r0
movb rh7, [r0 + #65535]
; CHECK: encoding: [0xf4,0xf0,0xff,0xff]
; DIS-NEXT: movb rh7, [r0 + #65535]
movb [r15 + #65535], rl0
; CHECK: encoding: [0xe4,0x0f,0xff,0xff]
; DIS-NEXT: movb [r15 + #65535], rl0
