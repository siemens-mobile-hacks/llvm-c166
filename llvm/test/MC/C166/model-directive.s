; REQUIRES: c166-registered-target
; RUN: split-file %s %t
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/default.s -o %t.default.o
; RUN: llvm-readobj --file-headers %t.default.o | FileCheck %s --check-prefix=LARGE
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/medium.s -o %t.medium.o
; RUN: llvm-readobj --file-headers %t.medium.o | FileCheck %s --check-prefix=MEDIUM
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/small.s -o %t.small.o
; RUN: llvm-readobj --file-headers %t.small.o | FileCheck %s --check-prefix=SMALL
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf -target-abi=medium %t/default.s -o %t.target-medium.o
; RUN: llvm-readobj --file-headers %t.target-medium.o | FileCheck %s --check-prefix=MEDIUM
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf -target-abi=small %t/default.s -o %t.target-small.o
; RUN: llvm-readobj --file-headers %t.target-small.o | FileCheck %s --check-prefix=SMALL
; RUN: llvm-readobj --symbols %t.medium.o | FileCheck %s --check-prefix=SYMBOL
; RUN: llvm-mc -filetype=asm -triple=c166-none-elf %t/medium.s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -filetype=null -triple=c166-none-elf %t/medium.s
; RUN: not llvm-mc -filetype=obj -triple=c166-none-elf %t/invalid.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=INVALID
; RUN: not llvm-mc -filetype=obj -triple=c166-none-elf %t/conflict.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=CONFLICT
; RUN: not llvm-mc -filetype=obj -triple=c166-none-elf %t/function-conflict.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=FUNCTION-CONFLICT
; RUN: not llvm-mc -filetype=obj -triple=c166-none-elf %t/data-conflict.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=DATA-CONFLICT
; RUN: not llvm-mc -filetype=obj -triple=c166-none-elf -target-abi=medium %t/model-conflict.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=CONFLICT

; LARGE: Flags [ (0x121)
; LARGE-NEXT: EF_C166_CODE_HUGE (0x100)
; LARGE-NEXT: EF_C166_CORE_8X166 (0x1)
; LARGE-NEXT: EF_C166_DATA_FAR (0x20)

; MEDIUM: Flags [ (0x221)
; MEDIUM-NEXT: EF_C166_CODE_NEAR (0x200)
; MEDIUM-NEXT: EF_C166_CORE_8X166 (0x1)
; MEDIUM-NEXT: EF_C166_DATA_FAR (0x20)

; SMALL: Flags [ (0x111)
; SMALL-NEXT: EF_C166_CODE_HUGE (0x100)
; SMALL-NEXT: EF_C166_CORE_8X166 (0x1)
; SMALL-NEXT: EF_C166_DATA_NEAR (0x10)

; ASM: .c166_model medium
; ASM: .c166_function near, near_function
; ASM: .c166_data near, near_data
; ASM: .c166_data xnear, xnear_data
; ASM: .c166_data far, far_data
; ASM: .c166_data huge, huge_data
; ASM: .c166_data shuge, shuge_data
; SYMBOL: Name: near_function
; SYMBOL: Other [ (0x20)
; SYMBOL-NEXT: STO_C166_CODE_NEAR (0x20)
; SYMBOL: Name: near_data
; SYMBOL: Other [ (0x20)
; SYMBOL-NEXT: STO_C166_DATA_NEAR (0x20)
; SYMBOL: Name: xnear_data
; SYMBOL: Other [ (0x40)
; SYMBOL-NEXT: STO_C166_DATA_XNEAR (0x40)
; SYMBOL: Name: far_data
; SYMBOL: Other [ (0x60)
; SYMBOL-NEXT: STO_C166_DATA_FAR (0x60)
; SYMBOL: Name: huge_data
; SYMBOL: Other [ (0x80)
; SYMBOL-NEXT: STO_C166_DATA_HUGE (0x80)
; SYMBOL: Name: shuge_data
; SYMBOL: Other [ (0xA0)
; SYMBOL-NEXT: STO_C166_DATA_SHUGE (0xA0)
; INVALID: error: unsupported C166 memory model 'tiny'
; CONFLICT: error: conflicting C166 memory model directives
; FUNCTION-CONFLICT: error: conflicting C166 function class directives for 'function'
; DATA-CONFLICT: error: conflicting C166 data class directives for 'data'

;--- default.s
ret

;--- medium.s
.c166_model medium
.c166_model medium
.section .custom.near,"ax",@progbits
.globl near_function
.type near_function,@function
.c166_function near, near_function
near_function:
ret
.size near_function, .-near_function

.data
.globl near_data
.type near_data,@object
.c166_data near, near_data
near_data:
.short 0
.size near_data, .-near_data
.globl xnear_data
.type xnear_data,@object
.c166_data xnear, xnear_data
xnear_data:
.short 0
.size xnear_data, .-xnear_data
.globl far_data
.type far_data,@object
.c166_data far, far_data
far_data:
.short 0
.size far_data, .-far_data
.globl huge_data
.type huge_data,@object
.c166_data huge, huge_data
huge_data:
.short 0
.size huge_data, .-huge_data
.globl shuge_data
.type shuge_data,@object
.c166_data shuge, shuge_data
shuge_data:
.short 0
.size shuge_data, .-shuge_data

;--- small.s
.c166_model small
rets

;--- invalid.s
.c166_model tiny
ret

;--- conflict.s
.c166_model medium
.c166_model large
ret

;--- model-conflict.s
.c166_model large
ret

;--- function-conflict.s
.c166_function near, function
.c166_function huge, function
function:
ret

;--- data-conflict.s
.c166_data near, data
.c166_data far, data
data:
.short 0
