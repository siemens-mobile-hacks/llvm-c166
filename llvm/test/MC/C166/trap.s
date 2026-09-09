; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

trap #0
; CHECK: trap #0{{.*}}encoding: [0x9b,0x00]
; DIS: 9b 00{{.*}}trap #0
trap #1
; CHECK: trap #1{{.*}}encoding: [0x9b,0x02]
; DIS-NEXT: 9b 02{{.*}}trap #1
TRAP #127
; CHECK: trap #127{{.*}}encoding: [0x9b,0xfe]
; DIS-NEXT: 9b fe{{.*}}trap #127
.equ vector, 64
trap #vector
; CHECK: trap #64{{.*}}encoding: [0x9b,0x80]
; DIS-NEXT: 9b 80{{.*}}trap #64
trap #(32 + 31)
; CHECK: trap #63{{.*}}encoding: [0x9b,0x7e]
; DIS-NEXT: 9b 7e{{.*}}trap #63
