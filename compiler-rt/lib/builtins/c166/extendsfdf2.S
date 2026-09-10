//===-- extendsfdf2.S - C166 binary32 to binary64 conversion ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#if __C166_MEMORY_MODEL__ == 2
#define C166_ABI_RET ret
        .section .c166.near.text,"ax",@progbits
#else
#define C166_ABI_RET rets
        .section .text.___extendsfdf2,"ax",@progbits
#endif

        .p2align 1
        .globl  ___extendsfdf2
        .type   ___extendsfdf2,@function
___extendsfdf2:
        mov     r10, r0
        add     r10, #4
        mov     r4, [r0]
        mov     r5, [r0 + #2]

        mov     r2, r4
        and     r2, #0x8000
        mov     r1, r4
        shr     r1, #7
        and     r1, #0xff
        and     r4, #0x7f

        cmp     r1, #0
        jmpr    cc_ne, .Lnonzero_exponent
        mov     r3, r4
        or      r3, r5
        jmpr    cc_eq, .Lpack

        mov     r1, #897
.Lnormalize:
        jb      r4.7, .Lpack
        shl     r5, #1
        addc    r4, r4
        sub     r1, #1
        jmpr    cc_uc, .Lnormalize

.Lnonzero_exponent:
        cmp     r1, #255
        jmpr    cc_eq, .Lspecial
        add     r1, #896
        jmpr    cc_uc, .Lpack

.Lspecial:
        mov     r1, #2047

.Lpack:
        and     r4, #0x7f
        mov     r3, r4
        shr     r3, #3
        shl     r1, #4
        or      r3, r1
        or      r3, r2
        mov     [r10], r3

        shl     r4, #13
        mov     r3, r5
        shr     r3, #3
        or      r3, r4
        mov     [r10 + #2], r3
        shl     r5, #13
        mov     [r10 + #4], r5
        mov     r3, #0
        mov     [r10 + #6], r3
        mov     r4, r10
        C166_ABI_RET

        .size   ___extendsfdf2, .-___extendsfdf2
