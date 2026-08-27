; REQUIRES: c166
; C166-ABI: medium.elf.near_placement
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: ld.lld --section-start=.c166.near.text=0xfffc --section-start=.custom.near=0x8000 --section-start=.text=0x180000 -e _near_start %t.o -o %t
; RUN: llvm-readobj --file-headers --symbols %t | FileCheck %s --check-prefix=OK
; RUN: not ld.lld --section-start=.c166.near.text=0xfffe --section-start=.text=0x180000 -e _near_start %t.o -o %t.cross 2>&1 | FileCheck %s --check-prefix=RANGE
; RUN: not ld.lld --section-start=.c166.near.text=0x10000 --section-start=.text=0x180000 -e _near_start %t.o -o %t.high 2>&1 | FileCheck %s --check-prefix=RANGE
; RUN: echo 'SECTIONS { .renamed 0x10000 : { *(.c166.near.text) } .text 0x180000 : { *(.text) } }' > %t.rename.lds
; RUN: not ld.lld -T %t.rename.lds -e _near_start %t.o -o %t.renamed 2>&1 | FileCheck %s --check-prefix=RENAMED
; RUN: not ld.lld --section-start=.c166.near.text=0x8000 --section-start=.custom.near=0x10000 --section-start=.text=0x180000 -e _near_start %t.o -o %t.custom 2>&1 | FileCheck %s --check-prefix=CUSTOM

.c166_model medium

.section .c166.near.text,"ax",@progbits
.globl _near_start
.type _near_start,@function
_near_start:
  nop
  ret

.section .custom.near,"ax",@progbits
.globl _custom_near
.type _custom_near,@function
.c166_function near, _custom_near
_custom_near:
  ret
.size _custom_near, .-_custom_near

.text
.globl _huge_function
.type _huge_function,@function
_huge_function:
  rets

; OK: Flags [ (0x221)
; OK: EF_C166_CODE_NEAR
; OK: Name: _near_start
; OK-NEXT: Value: 0xFFFC
; OK: Name: _huge_function
; OK-NEXT: Value: 0x180000

; RANGE: error: {{.*}}.c166.near.text{{.*}}: Medium near code range [0x{{FFFE|10000}}, 0x{{10002|10004}}) is outside the first 64 KiB code segment
; RENAMED: error: {{.*}}.c166.near.text{{.*}}: Medium near code range [0x10000, 0x10004) is outside the first 64 KiB code segment
; CUSTOM: error: near function '_custom_near': Medium near code range [0x10000, 0x10002) is outside the first 64 KiB code segment
