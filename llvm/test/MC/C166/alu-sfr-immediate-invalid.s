; RUN: not llvm-mc -triple=c166 %s -o /dev/null 2>&1 | FileCheck %s
add mdl, #65536
; CHECK: error:
sub mdh, #-32769
; CHECK: error:
cmp mdc, #paged(1)
; CHECK: error:
and mdl, -1
; CHECK: error:
or mdh, #1, #2
; CHECK: error:
