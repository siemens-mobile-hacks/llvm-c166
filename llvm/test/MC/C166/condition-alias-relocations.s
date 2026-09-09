; RUN: split-file %s %t
; RUN: llvm-mc -triple=c166 -filetype=obj %t/aliases.s -o %t/aliases.o
; RUN: llvm-mc -triple=c166 -filetype=obj %t/canonical.s -o %t/canonical.o
; RUN: cmp %t/aliases.o %t/canonical.o

; Aliases must preserve both local and external symbolic fixups, relaxation
; slots, and symbol names which happen to look like condition aliases.

;--- aliases.s
.globl _start
_start:
jmpr cc_z, .Llocal
jmpr cc_z, cc_c
jmpa cc_z, cc_z
calla cc_z, cof(cc_nz)
calli cc_z, [r15]
jmpi cc_z, [r0]
jmpr cc_nz, .Llocal
jmpr cc_nz, cc_c
jmpa cc_nz, cc_z
calla cc_nz, cof(cc_nz)
calli cc_nz, [r15]
jmpi cc_nz, [r0]
jmpr cc_c, .Llocal
jmpr cc_c, cc_c
jmpa cc_c, cc_z
calla cc_c, cof(cc_nz)
calli cc_c, [r15]
jmpi cc_c, [r0]
jmpr cc_nc, .Llocal
jmpr cc_nc, cc_c
jmpa cc_nc, cc_z
calla cc_nc, cof(cc_nz)
calli cc_nc, [r15]
jmpi cc_nc, [r0]
.Llocal:
nop

;--- canonical.s
.globl _start
_start:
jmpr cc_eq, .Llocal
jmpr cc_eq, cc_c
jmpa cc_eq, cc_z
calla cc_eq, cof(cc_nz)
calli cc_eq, [r15]
jmpi cc_eq, [r0]
jmpr cc_ne, .Llocal
jmpr cc_ne, cc_c
jmpa cc_ne, cc_z
calla cc_ne, cof(cc_nz)
calli cc_ne, [r15]
jmpi cc_ne, [r0]
jmpr cc_ult, .Llocal
jmpr cc_ult, cc_c
jmpa cc_ult, cc_z
calla cc_ult, cof(cc_nz)
calli cc_ult, [r15]
jmpi cc_ult, [r0]
jmpr cc_uge, .Llocal
jmpr cc_uge, cc_c
jmpa cc_uge, cc_z
calla cc_uge, cof(cc_nz)
calli cc_uge, [r15]
jmpi cc_uge, [r0]
.Llocal:
nop
