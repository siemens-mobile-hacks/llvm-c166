; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE

; A SELECT_CC used directly as a branch condition used to leave an
; unselectable BRCOND in the C166 DAG.
define i16 @branch_on_selected_comparison(i16 %a, i16 %b, i16 %c, i16 %d) {
entry:
  %choose = icmp eq i16 %a, %b
  %less = icmp ult i16 %c, %d
  %less_equal = icmp ule i16 %a, %d
  %condition = select i1 %choose, i1 %less, i1 %less_equal
  br i1 %condition, label %true, label %false

true:
  ret i16 1

false:
  ret i16 0
}

; CHECK-LABEL: branch_on_selected_comparison:
; CHECK:       cmp
; CHECK:       jmpr
; HUGE:        rets
; NEAR:        ret
