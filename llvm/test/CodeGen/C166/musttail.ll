; RUN: not llc -mtriple=c166-none-elf -code-model=large -filetype=obj %s \
; RUN:   -o %t.large.o 2>&1 | FileCheck %s
; RUN: not llc -mtriple=c166-none-elf -code-model=medium -filetype=obj %s \
; RUN:   -o %t.medium.o 2>&1 | FileCheck %s
; RUN: not llc -mtriple=c166-none-elf -code-model=small -filetype=obj %s \
; RUN:   -o %t.small.o 2>&1 | FileCheck %s

declare i16 @callee(i16)

define i16 @caller(i16 %value) {
entry:
  %result = musttail call i16 @callee(i16 %value)
  ret i16 %result
}

; CHECK: error: C166 does not support musttail calls
; CHECK-NOT: LLVM ERROR
