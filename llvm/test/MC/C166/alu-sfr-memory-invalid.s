; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
add mdl, -1
; CHECK: error:
sub mdh, 65536
; CHECK: error:
or 65536, mdc
; CHECK: error:
and -1, mdl
; CHECK: error:
add #1, mdl
; CHECK: error:
cmp 1, mdl
; CHECK: error:
