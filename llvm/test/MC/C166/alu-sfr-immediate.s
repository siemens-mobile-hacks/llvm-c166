; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -dr %t | FileCheck %s --check-prefix=DIS
; SFRs always use the full short-register/data16 form, even for data3 values.
add mdl, #1
; CHECK: encoding: [0x06,0x07,0x01,0x00]
; DIS: add mdl, #1
addc mdh, #7
; CHECK: encoding: [0x16,0x06,0x07,0x00]
; DIS: addc mdh, #7
sub mdc, #-1
; CHECK: encoding: [0x26,0x87,0xff,0xff]
; DIS: sub mdc, #65535
subc dpp0, #0
; CHECK: encoding: [0x36,0x00,0x00,0x00]
; DIS: subc dpp0, #0
cmp psw, #0x8000
; CHECK: encoding: [0x46,0x88,0x00,0x80]
; DIS: cmp psw, #32768
xor cp, #-32768
; CHECK: encoding: [0x56,0x08,0x00,0x80]
; DIS: xor cp, #32768
and sp, #65535
; CHECK: encoding: [0x66,0x09,0xff,0xff]
; DIS: and sp, #65535
or dpp3, #256
; CHECK: encoding: [0x76,0x03,0x00,0x01]
; DIS: or dpp3, #256
add mdl, #value+2
; CHECK: fixup A - offset: 2, value: value+2, kind: FK_Data_2
; DIS: add mdl, #0
; DIS-NEXT: R_C166_16 value+0x2
