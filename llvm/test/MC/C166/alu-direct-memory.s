; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS

; Word reg,mem forms, distinct from reg,#data16 (opcodes x6).
add r4, 3
; CHECK: encoding: [0x02,0xf4,0x03,0x00]
; DIS: add r4, 3
addc r4, 0x4000
; CHECK: encoding: [0x12,0xf4,0x00,0x40]
; DIS: addc r4, 16384
sub r4, 0x8000
; CHECK: encoding: [0x22,0xf4,0x00,0x80]
; DIS: sub r4, 32768
subc r4, 0xc000
; CHECK: encoding: [0x32,0xf4,0x00,0xc0]
; DIS: subc r4, 49152
cmp r4, 0xffff
; CHECK: encoding: [0x42,0xf4,0xff,0xff]
; DIS: cmp r4, 65535
xor r0, 0
; CHECK: encoding: [0x52,0xf0,0x00,0x00]
; DIS: xor r0, 0
and r15, 0x3fff
; CHECK: encoding: [0x62,0xff,0xff,0x3f]
; DIS: and r15, 16383
or r4, 256
; CHECK: encoding: [0x72,0xf4,0x00,0x01]
; DIS: or r4, 256

add r4, object+2
; CHECK: fixup A - offset: 2, value: object+2, kind: fixup_c166_address16
; DIS: add r4, 0
; DIS-NEXT: R_C166_16 object+0x2
