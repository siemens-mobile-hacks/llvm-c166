; RUN: llc -mtriple=c166-none-elf -verify-machineinstrs < %s | FileCheck %s

; Keep enough independently reachable blocks between the first comparisons
; and the late destinations to exceed JMPR's signed 8-bit word displacement.
; The relaxation pass must preserve short local branches while using JMPS for
; the out-of-range edges.
define i16 @large_switch(i16 %value) {
; CHECK-LABEL: large_switch:
; CHECK: jmpr
; CHECK: jmps seg(.LBB{{[0-9_]+}}), sof(.LBB{{[0-9_]+}})
entry:
  switch i16 %value, label %default [
    i16 3, label %case0
    i16 7, label %case1
    i16 12, label %case2
    i16 18, label %case3
    i16 25, label %case4
    i16 33, label %case5
    i16 42, label %case6
    i16 52, label %case7
    i16 63, label %case8
    i16 75, label %case9
    i16 88, label %case10
    i16 102, label %case11
    i16 117, label %case12
    i16 133, label %case13
    i16 150, label %case14
    i16 168, label %case15
    i16 187, label %case16
    i16 207, label %case17
    i16 228, label %case18
    i16 250, label %case19
    i16 273, label %case20
    i16 297, label %case21
    i16 322, label %case22
    i16 348, label %case23
    i16 375, label %case24
    i16 403, label %case25
    i16 432, label %case26
    i16 462, label %case27
    i16 493, label %case28
    i16 525, label %case29
    i16 558, label %case30
    i16 592, label %case31
  ]

case0:  ret i16 101
case1:  ret i16 307
case2:  ret i16 509
case3:  ret i16 701
case4:  ret i16 907
case5:  ret i16 1103
case6:  ret i16 1301
case7:  ret i16 1511
case8:  ret i16 1709
case9:  ret i16 1907
case10: ret i16 2111
case11: ret i16 2309
case12: ret i16 2503
case13: ret i16 2707
case14: ret i16 2903
case15: ret i16 3109
case16: ret i16 3301
case17: ret i16 3511
case18: ret i16 3709
case19: ret i16 3907
case20: ret i16 4111
case21: ret i16 4327
case22: ret i16 4513
case23: ret i16 4703
case24: ret i16 4903
case25: ret i16 5101
case26: ret i16 5303
case27: ret i16 5501
case28: ret i16 5701
case29: ret i16 5903
case30: ret i16 6101
case31: ret i16 6301
default: ret i16 -1
}
