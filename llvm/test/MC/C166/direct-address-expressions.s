; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -s -j .text -j .data %t | FileCheck %s --check-prefix=BYTES
; RUN: llvm-readobj -r %t | FileCheck %s --check-prefix=RELOC

mov r4, minimum
mov maximum, r4
scxt r4, maximum
.set minimum, 0
.set maximum, 65535
; BYTES: 0000 f2f40000 f6f4ffff d6f4ffff

; A negative addend is valid when the final symbol address is positive.
.section .text.reloc,"ax",@progbits
mov r4, external-1
scxt r4, external-1
; RELOC:      0x2 R_C166_16 external 0xFFFFFFFF
; RELOC-NEXT: 0x6 R_C166_16 external 0xFFFFFFFF

; The address-only check must not restrict signed data or immediates.
.section .data,"aw",@progbits
.short negative
.set negative, -1
; BYTES: Contents of section .data:
; BYTES-NEXT: 0000 ffff
