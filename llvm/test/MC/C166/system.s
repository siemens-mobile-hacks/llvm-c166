; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

; Protected instruction encodings from the C166 instruction manual.
diswdt
; CHECK: diswdt{{.*}}encoding: [0xa5,0x5a,0xa5,0xa5]
; DIS: a5 5a a5 a5{{.*}}diswdt
einit
; CHECK: einit{{.*}}encoding: [0xb5,0x4a,0xb5,0xb5]
; DIS: b5 4a b5 b5{{.*}}einit
IDLE
; CHECK: idle{{.*}}encoding: [0x87,0x78,0x87,0x87]
; DIS: 87 78 87 87{{.*}}idle
pwrdn
; CHECK: pwrdn{{.*}}encoding: [0x97,0x68,0x97,0x97]
; DIS: 97 68 97 97{{.*}}pwrdn
srvwdt
; CHECK: srvwdt{{.*}}encoding: [0xa7,0x58,0xa7,0xa7]
; DIS: a7 58 a7 a7{{.*}}srvwdt
srst
; CHECK: srst{{.*}}encoding: [0xb7,0x48,0xb7,0xb7]
; DIS: b7 48 b7 b7{{.*}}srst
