; RUN: llc -mtriple=c166-none-elf -verify-machineinstrs < %s | FileCheck %s

declare i16 @llvm.fshr.i16(i16, i16, i16)

; FSHR(a, b, 1) is (a << 15) | (b >> 1), not the reverse.
; The separate b >> 1 must not be mistaken for a matching high-word shift.
; For a=2, b=1 the result must be zero, not 0x8001.
define i32 @fshr_pair(i16 %a, i16 %b) {
; CHECK-LABEL: _fshr_pair:
; CHECK: shr r13, #1
; CHECK-NEXT: shl r12, #15
; CHECK-NEXT: or r12, r13
; CHECK-NEXT: mov r4, r12
; CHECK-NEXT: mov r5, r13
  %lo = call i16 @llvm.fshr.i16(i16 %a, i16 %b, i16 1)
  %hi = lshr i16 %b, 1
  %l = zext i16 %lo to i32
  %h = zext i16 %hi to i32
  %hs = shl i32 %h, 16
  %result = or i32 %hs, %l
  ret i32 %result
}
