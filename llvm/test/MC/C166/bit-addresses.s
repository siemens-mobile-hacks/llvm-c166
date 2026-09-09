; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

; bitoff 0..127 denotes RAM at FD00..FDFE, not the lower SFR half.
bset 0 . 7
; CHECK: bset 0 . 7{{.*}}encoding: [0x7f,0x00]
; DIS: bset 0 . 7
bset 127 . 15
; CHECK: bset 127 . 15{{.*}}encoding: [0xff,0x7f]
; DIS-NEXT: bset 127 . 15
bclr 128 . 0
; CHECK: bclr 128 . 0{{.*}}encoding: [0x0e,0x80]
; DIS-NEXT: bclr 128 . 0
bset 239 . 1
; CHECK: bset 239 . 1{{.*}}encoding: [0x1f,0xef]
; DIS-NEXT: bset 239 . 1
bfldl 9, #1, #2
; CHECK: bfldl 9, #1, #2{{.*}}encoding: [0x0a,0x09,0x01,0x02]
; DIS-NEXT: bfldl 9, #1, #2
bset mdc.0
; CHECK: bset mdc.0{{.*}}encoding: [0x0f,0x87]
; DIS-NEXT: bset mdc.0
bset psw.15
; CHECK: bset psw.15{{.*}}encoding: [0xff,0x88]
; DIS-NEXT: bset psw.15
bset r15.15
; CHECK: bset r15.15{{.*}}encoding: [0xff,0xff]
; DIS-NEXT: bset r15.15
