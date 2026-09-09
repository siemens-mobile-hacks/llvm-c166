; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

; Condition encodings 4/5 test V=1/V=0, respectively.
jmpr cc_v, 127
jmpr cc_nv, 128
jmpa cc_v, 0x1234
jmpa cc_nv, 0xfffe

; ASM: jmpr cc_v, 127{{.*}}encoding: [0x4d,0x7f]
; ASM: jmpr cc_nv, 128{{.*}}encoding: [0x5d,0x80]
; ASM: jmpa cc_v, 4660{{.*}}encoding: [0xea,0x40,0x34,0x12]
; ASM: jmpa cc_nv, 65534{{.*}}encoding: [0xea,0x50,0xfe,0xff]
; DIS: 4d 7f{{.*}}jmpr cc_v, 127
; DIS: 5d 80{{.*}}jmpr cc_nv, 128
; DIS: ea 40 34 12{{.*}}jmpa cc_v, 4660
; DIS: ea 50 fe ff{{.*}}jmpa cc_nv, 65534
