; REQUIRES: c166
; RUN: split-file %s %t
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/tiny.s -o %t.tiny.o
; RUN: ld.lld -e _start --section-start=.c166.near.text=0x8000 --section-start=.c166.small.data=0xfffc %t.tiny.o -o %t.tiny
; RUN: llvm-readobj --file-headers --symbols %t.tiny | FileCheck %s --check-prefix=TINY
; RUN: not ld.lld -e _start --section-start=.c166.near.text=0x10000 --section-start=.c166.small.data=0x8000 %t.tiny.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=TINY-CODE-RANGE
; RUN: not ld.lld -e _start --section-start=.c166.near.text=0x8000 --section-start=.c166.small.data=0xfffe %t.tiny.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=TINY-DATA-RANGE
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/tiny-huge-function.s -o %t.tiny-huge-function.o
; RUN: not ld.lld %t.tiny-huge-function.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=TINY-HUGE-FUNCTION
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/tiny-far-data.s -o %t.tiny-far-data.o
; RUN: not ld.lld %t.tiny-far-data.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=TINY-FAR-DATA
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/huge.s -o %t.huge.o
; RUN: ld.lld -e _start --section-start=.data=0xfffe %t.huge.o -o %t.huge
; RUN: llvm-readobj --file-headers --symbols %t.huge | FileCheck %s --check-prefix=HUGE
; RUN: not ld.lld -e _start --section-start=.data=0xfffffe %t.huge.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=HUGE-RANGE

; TINY: Flags [ (0x211)
; TINY: Name: normal_data
; TINY-NEXT: Value: 0xFFFC
; TINY-CODE-RANGE: Tiny near code range [0x10000, 0x10002) is outside the first 64 KiB code segment
; TINY-DATA-RANGE: Tiny normal data range [0xFFFE, 0x10002) violates its 64 KiB placement
; TINY-HUGE-FUNCTION: huge function '_huge_function' is not available in the Tiny memory model
; TINY-FAR-DATA: data symbol 'far_data' uses a data class which is not available in the Tiny memory model

; HUGE: Flags [ (0x141)
; HUGE: Name: huge_data
; HUGE-NEXT: Value: 0xFFFE
; HUGE-RANGE: Huge huge data range [0xFFFFFE, 0x1000002) violates its 16384 KiB placement

;--- tiny.s
.c166_model tiny
.section .c166.near.text,"ax",@progbits
.globl _start
.type _start,@function
.c166_function near, _start
_start:
  ret
.size _start, .-_start

.section .c166.small.data,"aw",@progbits
.globl normal_data
.type normal_data,@object
.c166_data near, normal_data
normal_data:
  .short 0, 0
.size normal_data, .-normal_data

;--- tiny-huge-function.s
.c166_model tiny
.text
.globl _huge_function
.type _huge_function,@function
.c166_function huge, _huge_function
_huge_function:
  rets
.size _huge_function, .-_huge_function

;--- tiny-far-data.s
.c166_model tiny
.data
.globl far_data
.type far_data,@object
.c166_data far, far_data
far_data:
  .short 0
.size far_data, .-far_data

;--- huge.s
.c166_model huge
.text
.globl _start
.type _start,@function
.c166_function huge, _start
_start:
  rets
.size _start, .-_start

.data
.globl huge_data
.type huge_data,@object
.c166_data huge, huge_data
huge_data:
  .short 0, 0
.size huge_data, .-huge_data
