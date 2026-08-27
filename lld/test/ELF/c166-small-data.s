; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: ld.lld -e normal_data \
; RUN:   --section-start=.c166.small.rodata=0xfff8 \
; RUN:   --section-start=.custom.normal=0xfffc \
; RUN:   --section-start=.custom.far=0xbffc \
; RUN:   --section-start=.custom.huge=0xfffff8 \
; RUN:   --section-start=.custom.shuge=0x1fffc %t.o -o %t
; RUN: llvm-readobj --file-headers --symbols %t | FileCheck %s --check-prefix=OK
; RUN: not ld.lld -e normal_data \
; RUN:   --section-start=.c166.small.rodata=0xfffe \
; RUN:   --section-start=.custom.normal=0x8000 \
; RUN:   --section-start=.custom.far=0x10000 \
; RUN:   --section-start=.custom.huge=0x20000 \
; RUN:   --section-start=.custom.shuge=0x30000 %t.o -o %t.bad 2>&1 | FileCheck %s --check-prefix=CANONICAL
; RUN: not ld.lld -e normal_data \
; RUN:   --section-start=.c166.small.rodata=0x7000 \
; RUN:   --section-start=.custom.normal=0xfffe \
; RUN:   --section-start=.custom.far=0x10000 \
; RUN:   --section-start=.custom.huge=0x20000 \
; RUN:   --section-start=.custom.shuge=0x30000 %t.o -o %t.bad 2>&1 | FileCheck %s --check-prefix=NORMAL
; RUN: not ld.lld -e normal_data \
; RUN:   --section-start=.c166.small.rodata=0x7000 \
; RUN:   --section-start=.custom.normal=0x8000 \
; RUN:   --section-start=.custom.far=0xbffe \
; RUN:   --section-start=.custom.huge=0x20000 \
; RUN:   --section-start=.custom.shuge=0x30000 %t.o -o %t.bad 2>&1 | FileCheck %s --check-prefix=FAR
; RUN: not ld.lld -e normal_data \
; RUN:   --section-start=.c166.small.rodata=0x7000 \
; RUN:   --section-start=.custom.normal=0x8000 \
; RUN:   --section-start=.custom.far=0x10000 \
; RUN:   --section-start=.custom.huge=0xfffffe \
; RUN:   --section-start=.custom.shuge=0x30000 %t.o -o %t.bad 2>&1 | FileCheck %s --check-prefix=HUGE
; RUN: not ld.lld -e normal_data \
; RUN:   --section-start=.c166.small.rodata=0x7000 \
; RUN:   --section-start=.custom.normal=0x8000 \
; RUN:   --section-start=.custom.far=0x10000 \
; RUN:   --section-start=.custom.huge=0x20000 \
; RUN:   --section-start=.custom.shuge=0x3fffe %t.o -o %t.bad 2>&1 | FileCheck %s --check-prefix=SHUGE

; OK: Flags [ (0x111)
; OK: Name: normal_data
; OK: Value: 0xFFFC
; OK: Name: far_data
; OK: Value: 0xBFFC
; OK: Name: huge_data
; OK: Value: 0xFFFFF8
; OK: Name: shuge_data
; OK: Value: 0x1FFFC

; CANONICAL: error: {{.*}}.c166.small.rodata{{.*}}Small normal data range [0xFFFE, 0x10002) violates its 64 KiB placement
; NORMAL: error: data symbol 'normal_data': Small normal data range [0xFFFE, 0x10002) violates its 64 KiB placement
; FAR: error: data symbol 'far_data': Small far data range [0xBFFE, 0xC002) violates its 16 KiB placement
; HUGE: error: data symbol 'huge_data': Small huge data range [0xFFFFFE, 0x1000002) violates its 16384 KiB placement
; SHUGE: error: data symbol 'shuge_data': Small shuge data range [0x3FFFE, 0x40002) violates its 64 KiB placement

.c166_model small

.section .c166.small.rodata,"a",@progbits
.short 0, 0

.section .custom.normal,"aw",@progbits
.globl normal_data
.type normal_data,@object
.c166_data near, normal_data
normal_data:
.short 0, 0
.size normal_data, .-normal_data

.section .custom.far,"aw",@progbits
.globl far_data
.type far_data,@object
.c166_data far, far_data
far_data:
.short 0, 0
.size far_data, .-far_data

.section .custom.huge,"aw",@progbits
.globl huge_data
.type huge_data,@object
.c166_data huge, huge_data
huge_data:
.short 0, 0
.size huge_data, .-huge_data

.section .custom.shuge,"aw",@progbits
.globl shuge_data
.type shuge_data,@object
.c166_data shuge, shuge_data
shuge_data:
.short 0, 0
.size shuge_data, .-shuge_data
