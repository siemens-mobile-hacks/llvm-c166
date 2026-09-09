; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs \
; RUN:   -stop-after=finalize-isel < %s | FileCheck %s --check-prefix=ISEL
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs \
; RUN:   -stop-after=finalize-isel < %s | FileCheck %s --check-prefix=ISEL
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs \
; RUN:   -stop-after=finalize-isel < %s | FileCheck %s --check-prefix=ISEL
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE

; DAG combine recognizes this expression as ROTL. C166 has native shifts but
; no rotate instruction, so target lowering must expand the combined node.
define i16 @rotl5(i16 %value) {
; CHECK-LABEL: rotl5:
; CHECK-DAG:   shl
; CHECK-DAG:   shr
; CHECK:       or
; HUGE:        rets
; NEAR:        ret
  %left = shl i16 %value, 5
  %right = lshr i16 %value, 11
  %result = or i16 %left, %right
  ret i16 %result
}

define i32 @rotl32_5(i32 %value) {
; CHECK-LABEL: rotl32_5:
; CHECK-NOT:   calls
; CHECK:       shr {{r[0-9]+}}, #11
; CHECK-NEXT:  shl {{r[0-9]+}}, #5
; CHECK-NEXT:  or {{r[0-9]+}}, {{r[0-9]+}}
; CHECK:       shr {{r[0-9]+}}, #11
; CHECK-NEXT:  shl {{r[0-9]+}}, #5
; CHECK-NEXT:  or {{r[0-9]+}}, {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = call i32 @llvm.fshl.i32(i32 %value, i32 %value, i32 5)
  ret i32 %result
}

define i32 @rotl32_16(i32 %value) {
; ISEL-LABEL: name: rotl32_16
; ISEL:       REG_SEQUENCE
; ISEL-NOT:   ROTL32ri5
; ISEL:       RET{{S?}}
; CHECK-LABEL: rotl32_16:
; CHECK-NOT:   calls
; CHECK-NOT:   shl
; CHECK-NOT:   shr
; CHECK:       mov r4, r13
; CHECK-NEXT:  mov r5, r12
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %result = call i32 @llvm.fshl.i32(i32 %value, i32 %value, i32 16)
  ret i32 %result
}

define i32 @rotl32_21(i32 %value) {
; CHECK-LABEL: rotl32_21:
; CHECK-NOT:   calls
; CHECK:       shr {{r[0-9]+}}, #11
; CHECK-NEXT:  shl {{r[0-9]+}}, #5
; CHECK-NEXT:  or {{r[0-9]+}}, {{r[0-9]+}}
; CHECK:       shr {{r[0-9]+}}, #11
; CHECK-NEXT:  shl {{r[0-9]+}}, #5
; CHECK-NEXT:  or {{r[0-9]+}}, {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = call i32 @llvm.fshl.i32(i32 %value, i32 %value, i32 21)
  ret i32 %result
}

define i32 @rotl32_variable(i32 %value, i16 %amount) {
; CHECK-LABEL: rotl32_variable:
; CHECK:       ___ashlsi3
; CHECK:       ___lshrsi3
; CHECK:       or
; HUGE:        rets
; NEAR:        ret
  %wide = zext i16 %amount to i32
  %result = call i32 @llvm.fshl.i32(i32 %value, i32 %value, i32 %wide)
  ret i32 %result
}

declare i32 @llvm.fshl.i32(i32, i32, i32)
