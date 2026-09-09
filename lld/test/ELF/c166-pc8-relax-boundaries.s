; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: ld.lld -Ttext=0x10100 -e _start %t.o \
; RUN:   --defsym=min_target=_start-254 \
; RUN:   --defsym=max_target=_start+262 \
; RUN:   --defsym=below_target=_start-244 \
; RUN:   --defsym=above_target=_start+276 -o %t
; RUN: llvm-objdump -d --section=.text %t | FileCheck %s

.text
.globl _start
_start:
  jmpr cc_eq, min_target
  jmpr cc_ne, max_target
  jmpr cc_ult, below_target
  jmpr cc_uge, above_target

; The inclusive signed-word boundaries stay short.
; CHECK-LABEL: <_start>:
; CHECK-NEXT:  10100: 2d 80{{.*}}jmpr cc_eq, 128
; CHECK-NEXT:  10102: cc 00{{.*}}nop
; CHECK-NEXT:  10104: cc 00{{.*}}nop
; CHECK-NEXT:  10106: 3d 7f{{.*}}jmpr cc_ne, 127
; CHECK-NEXT:  10108: cc 00{{.*}}nop
; CHECK-NEXT:  1010a: cc 00{{.*}}nop

; One word beyond either boundary uses the far replacement.
; CHECK-NEXT:  1010c: 9d 02{{.*}}jmpr cc_uge, 2
; CHECK-NEXT:  1010e: fa 01 0c 00{{.*}}jmps 1, 12
; CHECK-NEXT:  10112: 8d 02{{.*}}jmpr cc_ult, 2
; CHECK-NEXT:  10114: fa 01 14 02{{.*}}jmps 1, 532
