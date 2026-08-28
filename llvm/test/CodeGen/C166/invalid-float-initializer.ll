; RUN: not llc -mtriple=c166-none-elf -code-model=large -filetype=obj \
; RUN:   -o /dev/null %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=c166-none-elf -code-model=medium -filetype=obj \
; RUN:   -o /dev/null %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=c166-none-elf -code-model=small -filetype=obj \
; RUN:   -o /dev/null %s 2>&1 | FileCheck %s

; A relocation-valued floating ConstantExpr cannot be represented in
; the reversed 16-bit floating word order. It is valid generic LLVM IR
; but is outside the C166 ELF subset, so it must receive a diagnostic rather
; than terminating the compiler.

@anchor = addrspace(5) global i16 0
@symbolic_float = global float bitcast (i32 ptrtoaddr (ptr addrspace(5) @anchor to i32) to float)

; CHECK: error: C166 cannot encode a symbolic floating-point global initializer
