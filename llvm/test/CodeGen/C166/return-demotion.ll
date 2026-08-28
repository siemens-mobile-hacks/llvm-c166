; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs \
; RUN:   -filetype=obj -o %t.large.o < %s
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs \
; RUN:   -filetype=obj -o %t.medium.o < %s
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs \
; RUN:   -filetype=obj -o %t.small.o < %s

; Return values which do not fit the C166 scalar result registers must be
; demoted by generic SelectionDAG lowering to a hidden sret pointer.  In
; particular, LowerReturn must receive no memory CCValAssign and therefore
; must never reach its "memory return" fatal path.

%triple = type { i16, i16, i16 }
%packed6 = type <{ i8, i32, i8 }>

define %triple @return_triple(i16 %a, i16 %b, i16 %c) {
; CHECK-LABEL: _return_triple:
; CHECK:       mov r4,
; HUGE:        rets
; NEAR:        ret
  %v0 = insertvalue %triple poison, i16 %a, 0
  %v1 = insertvalue %triple %v0, i16 %b, 1
  %v2 = insertvalue %triple %v1, i16 %c, 2
  ret %triple %v2
}

declare %triple @external_triple(i16, i16, i16)

define i16 @call_triple(i16 %a, i16 %b, i16 %c) {
; CHECK-LABEL: _call_triple:
; HUGE:        calls seg(_external_triple), sof(_external_triple)
; NEAR:        calla cc_uc, cof(_external_triple)
; HUGE:        rets
; NEAR:        ret
  %value = call %triple @external_triple(i16 %a, i16 %b, i16 %c)
  %first = extractvalue %triple %value, 0
  %third = extractvalue %triple %value, 2
  %sum = add i16 %first, %third
  ret i16 %sum
}

declare %packed6 @external_packed6(i16)

define i16 @call_packed6(i16 %seed) {
; A demoted packed result is copied into an align-1 caller temporary.  Even
; though the public result block is word-sized, its destination may be odd;
; every destination access must therefore remain a byte store.
; CHECK-LABEL: _call_packed6:
; HUGE:        calls seg(_external_packed6), sof(_external_packed6)
; NEAR:        calla cc_uc, cof(_external_packed6)
; CHECK-COUNT-6: movb [{{r[0-9]+}}{{.*}}], {{r[lh][0-7]}}
; CHECK-NOT:   mov [{{r[0-9]+}}],
; HUGE:        rets
; NEAR:        ret
  %value = call %packed6 @external_packed6(i16 %seed)
  %first = extractvalue %packed6 %value, 0
  %last = extractvalue %packed6 %value, 2
  %first16 = zext i8 %first to i16
  %last16 = zext i8 %last to i16
  %sum = add i16 %first16, %last16
  ret i16 %sum
}
