; RUN: not llc -mtriple=c166-none-elf -filetype=obj -o /dev/null %s 2>&1 \
; RUN:   | FileCheck %s

; A relocation-valued floating ConstantExpr cannot be represented in
; the reversed 16-bit floating word order. It is valid generic LLVM IR
; but is outside the C166 ELF subset, so it must receive a diagnostic rather
; than terminating the compiler.

@anchor = global i16 0
@symbolic_float = global float bitcast (i32 ptrtoaddr (ptr @anchor to i32) to float)

; CHECK: error: C166 cannot encode a symbolic floating-point global initializer
