; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t

MoV R4, #-1
; CHECK: mov r4, #65535{{.*}}encoding: [0xe6,0xf4,0xff,0xff]
JMPA CC_EQ, 0x1234
; CHECK: jmpa cc_eq, 4660{{.*}}encoding: [0xea,0x20,0x34,0x12]
CALLA CC_UC, 0x1234
; CHECK: calla cc_uc, 4660{{.*}}encoding: [0xca,0x00,0x34,0x12]
JMPR CC_EQ, cc_Target
; CHECK: jmpr cc_eq, cc_Target
; CHECK: value: cc_Target
cc_Target:
MOV R4, cc_Data
; CHECK: mov r4, cc_Data
; CHECK: value: cc_Data
