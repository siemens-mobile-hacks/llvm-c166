; RUN: not llvm-mc -triple=c166-none-elf -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

; Values 253..255 remain reserved. Merely naming them must not make LLVM emit
; them.

.text
.space 3
.reloc 0, R_C166_RESERVED_253, target
; CHECK: error: unknown relocation name
.reloc 1, R_C166_RESERVED_254, target
; CHECK: error: unknown relocation name
.reloc 2, R_C166_RESERVED_255, target
; CHECK: error: unknown relocation name
