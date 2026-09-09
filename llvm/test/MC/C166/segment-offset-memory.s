; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-readobj -r %t | FileCheck %s --check-prefix=RELOC
; RUN: llvm-objdump -s %t | FileCheck %s --check-prefix=BYTES

; EXTS supplies the segment; memory operands retain the full low 16 bits.
exts #seg(object), #2
mov r4, sof(object+2)
mov sof(object+4), r4
movb rl2, sof(object+1)
movb sof(object+3), rl2
add r4, sof(object+6)
add sof(object+8), r4
movb rl2, sof(0xffffff)
mov r4, sof(min_address)
movb rh7, sof(max_address)
.set min_address, 0
.set max_address, 0xffffff

; RELOC: R_C166_SEG8 object 0x0
; RELOC: R_C166_SOF16 object 0x2
; RELOC: R_C166_SOF16 object 0x4
; RELOC: R_C166_SOF16 object 0x1
; RELOC: R_C166_SOF16 object 0x3
; RELOC: R_C166_SOF16 object 0x6
; RELOC: R_C166_SOF16 object 0x8

; BYTES: Contents of section .text:
; BYTES-NEXT: 0000 d7100000 f2f40000 f6f40000 f3f40000
; BYTES-NEXT: 0010 f7f40000 02f40000 04f40000 f3f4ffff
; BYTES-NEXT: 0020 f2f40000 f3ffffff
