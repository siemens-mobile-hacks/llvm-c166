; RUN: not llc -mtriple=c166-none-elf -filetype=obj %s -o %t.o 2>&1 | FileCheck %s

; The ABI's virtual return-address register is 301. Runtime
; .eh_frame uses a version-1 CIE with a one-byte return-register field, so the
; target must diagnose an LLVM uwtable request rather than asserting in MC.

define void @runtime_unwind_request() uwtable {
entry:
  ret void
}

; CHECK: error: C166 does not support runtime .eh_frame unwind tables; use -g for DWARF .debug_frame information
