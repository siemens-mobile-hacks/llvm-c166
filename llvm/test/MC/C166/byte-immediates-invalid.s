; RUN: not llvm-mc -triple=c166 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

movb rl0, #-129
; CHECK: error:
; CHECK-NEXT: movb rl0, #-129
addb rl0, #-129
; CHECK: error:
; CHECK-NEXT: addb rl0, #-129
cmpb rh7, #256
; CHECK: error:
; CHECK-NEXT: cmpb rh7, #256
movb rl0, #paged(object)
; CHECK: error:
; CHECK-NEXT: movb rl0, #paged(object)
