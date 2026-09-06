; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: ld.lld -Ttext=0x10000 -e _start %t.o \
; RUN:   --defsym=near_target=_start+64 \
; RUN:   --defsym=far_target=_start+512 -o %t
; RUN: llvm-objdump -d --section=.text %t | FileCheck %s

.text
.globl _start
_start:
  bcmp r1.15, r2.15
  jmpr cc_n, near_target
  bcmp r3.0, r4.0
  jmpr cc_nn, far_target

; The in-range branch keeps its condition and relaxation padding.
; CHECK-LABEL: <_start>:
; CHECK-NEXT:  10000: 2a f2 f1 ff{{.*}}bcmp r1.15, r2.15
; CHECK-NEXT:  10004: 6d 1d{{.*}}jmpr cc_n, 29
; CHECK-NEXT:  10006: cc 00{{.*}}nop
; CHECK-NEXT:  10008: cc 00{{.*}}nop

; The out-of-range branch uses the inverse N condition around a far jump.
; CHECK-NEXT:  1000a: 2a f4 f3 00{{.*}}bcmp r3.0, r4.0
; CHECK-NEXT:  1000e: 6d 02{{.*}}jmpr cc_n, 2
; CHECK-NEXT:  10010: fa 01 00 02{{.*}}jmps 1, 512
