; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS

jmpr cc_net, 127
; ASM: encoding: [0x1d,0x7f]
; DIS: 1d 7f{{.*}}jmpr cc_net, 127
jmpa cc_net, 0x1234
; ASM: encoding: [0xea,0x10,0x34,0x12]
; DIS: ea 10 34 12{{.*}}jmpa cc_net, 4660
jmpr cc_net, .Llocal
.Llocal:
; DIS: 1d 00{{.*}}jmpr cc_net, 0
; DIS-NEXT: {{.*}}R_C166_PC8 .text+0x8
jmpr cc_net, external
; DIS: 1d 00{{.*}}jmpr cc_net, 0
; DIS-NEXT: {{.*}}R_C166_PC8_RELAX external
; DIS-NEXT: cc 00{{.*}}nop
; DIS-NEXT: cc 00{{.*}}nop
; DIS-NEXT: cc 00{{.*}}nop

.section .text.long,"ax",@progbits
jmpr cc_net, .Lfar
.space 256
.Lfar:
nop
; DIS: 1d 00{{.*}}jmpr cc_net, 0
; DIS-NEXT: {{.*}}R_C166_PC8_RELAX .text.long+0x108
