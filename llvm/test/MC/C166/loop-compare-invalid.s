; RUN: split-file %s %t
; RUN: not llvm-mc -triple=c166 %t/parse.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=PARSE
; RUN: not llvm-mc -triple=c166 -filetype=obj %t/forward.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=FORWARD

; Parser errors prevent layout; exercise late range checks independently.
;--- parse.s
.irp op, cmpd1, cmpd2, cmpi1, cmpi2
  \op mdl, #9
  \op rl2, #9
  \op r4, #65536
  \op r4, #-32769
  \op r4, 65536
  \op r4, [r0]
.endr
; PARSE-COUNT-24: error:
; PARSE-NOT: error:

;--- forward.s
.irp op, cmpd1, cmpd2, cmpi1, cmpi2
  \op r4, #too_large
  \op r4, negative_address
.endr
.set too_large, 65536
.set negative_address, -1
; FORWARD-COUNT-8: error:
; FORWARD-NOT: error:
