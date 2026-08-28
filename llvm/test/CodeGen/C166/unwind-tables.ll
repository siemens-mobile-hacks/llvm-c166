; RUN: not llc -mtriple=c166-none-elf -code-model=large -filetype=obj %s \
; RUN:   -o %t.large.o 2>&1 | FileCheck %s
; RUN: not llc -mtriple=c166-none-elf -code-model=medium -filetype=obj %s \
; RUN:   -o %t.medium.o 2>&1 | FileCheck %s
; RUN: not llc -mtriple=c166-none-elf -code-model=small -filetype=obj %s \
; RUN:   -o %t.small.o 2>&1 | FileCheck %s

; The ABI's virtual return-address register is 301. Runtime
; .eh_frame uses a version-1 CIE with a one-byte return-register field, so the
; target must diagnose an LLVM uwtable request rather than asserting in MC.

define void @runtime_unwind_request() uwtable {
entry:
  ret void
}

; CHECK: error: C166 does not support runtime .eh_frame unwind tables; use -g for DWARF .debug_frame information
