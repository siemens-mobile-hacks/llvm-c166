; RUN: not llvm-mc -triple=c166 %s 2>&1 | FileCheck %s

; An unsupported memory form must never turn into an immediate operation.
add r4, 65536
; CHECK: error:
addb rl2, -1
; CHECK: error:
add r4, -1
; CHECK: error:
addb rl2, 65536
; CHECK: error:
mov mdc, 65536
; CHECK: error:
cmp r4, 65536
; CHECK: error:
sub r4, -1
; CHECK: error:
shl r4, 1
; CHECK: error:
bfldl psw, 1, #2
; CHECK: error:
atomic 1
; CHECK: error:
extp r4, 1
; CHECK: error:
exts r4, 1
; CHECK: error:
scxt r4, #65536
; CHECK: error:
scxt r4, 65536
; CHECK: error:
