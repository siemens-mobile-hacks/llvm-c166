; RUN: llc -mtriple=c166-none-elf -code-model=large \
; RUN:   -stop-after=finalize-isel -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,LARGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium \
; RUN:   -stop-after=finalize-isel -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,MEDIUM

declare void @callee()

define void @direct() {
; CHECK-LABEL: name: direct
; LARGE: CALLS @callee, @callee, csr_c166_callpreserved, implicit $sp, implicit $csp
; LARGE-NEXT: RETS
; MEDIUM: CALLA @callee, csr_c166_callpreserved, implicit $sp
; MEDIUM-NEXT: RET
  call void @callee()
  ret void
}

define void @indirect(ptr addrspace(3) %fn) {
; CHECK-LABEL: name: indirect
; CHECK: CALLI %{{[0-9]+}}, csr_c166_callpreserved, implicit $sp
; LARGE-NEXT: RETS
; MEDIUM-NEXT: RET
  call addrspace(3) void %fn()
  ret void
}

define cc 129 void @interrupt() {
; CHECK-LABEL: name: interrupt
; CHECK: RETI implicit-def dead $sp, implicit-def dead $csp, implicit-def dead $psw, implicit-def dead $c, implicit $sp
  ret void
}
