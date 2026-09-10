//===-- truncdfsf2.S - C166 binary64 to binary32 conversion ------------===//
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
        .section .text.___truncdfsf2,"ax",@progbits
#endif

        .p2align 1
        .globl  ___truncdfsf2
        .type   ___truncdfsf2,@function
___truncdfsf2:
        // Binary64 arguments are MSW-first on the user stack.  The softened
        // binary32 result is the logical integer pair R4:R5 (low:high).
        mov     r13, r0
        mov     r2, [r13+]
        mov     r1, [r13+]
        mov     r10, [r13+]
        mov     r11, [r13]

        mov     r12, r2
        and     r12, #0xf
        mov     r3, r2
        bclr    r3.15
        shr     r3, #4

        cmp     r3, #897
        jmpr    cc_ult, .Lbelow_normal
        cmp     r3, #1150
        jmpr    cc_ule, .Lextract_fraction

        cmp     r3, #2047
        jmpr    cc_ne, .Linfinity
        mov     r13, r12
        or      r13, r1
        or      r13, r10
        or      r13, r11
        jmpr    cc_eq, .Linfinity
        mov     r3, #255

.Lextract_fraction:
        shl     r12, #3
        mov     r5, r1
        shr     r5, #13
        or      r5, r12
        shl     r1, #3
        mov     r4, r10
        shr     r4, #13
        or      r4, r1

        cmp     r3, #255
        jmpr    cc_eq, .Lquiet_nan
        add     r3, #-896
        shl     r3, #7
        or      r5, r3
        jnb     r10.12, .Lreturn
        mov     r13, r10
        shl     r13, #4
        or      r13, r11
        jmpr    cc_ne, .Lround_normal
        jnb     r4.0, .Lreturn
.Lround_normal:
        add     r4, #1
        addc    r5, #0

.Lreturn:
        bmov    r5.15, r2.15
        C166_ABI_RET

.Lquiet_nan:
        bset    r5.6
        or      r5, #0x7f80
        jmpr    cc_uc, .Lreturn

.Linfinity:
        mov     r4, #0
        mov     r5, #0x7f80
        jmpr    cc_uc, .Lreturn

.Lbelow_normal:
        cmp     r3, #873
        jmpr    cc_ult, .Lzero
        // Shift the explicit 53-bit significand far enough to form a
        // binary32 subnormal, retaining separate guard and sticky state.
        bset    r12.4
        mov     r13, #926
        sub     r13, r3
        mov     r14, #0
        mov     r15, #0

.Lshift_subnormal:
        or      r14, r15
        mov     r15, r11
        and     r15, #1
        shr     r11, #1
        bmov    r11.15, r10.0
        shr     r10, #1
        bmov    r10.15, r1.0
        shr     r1, #1
        bmov    r1.15, r12.0
        shr     r12, #1
        sub     r13, #1
        jmpr    cc_ne, .Lshift_subnormal

        mov     r4, r11
        mov     r5, r10
        cmp     r15, #0
        jmpr    cc_eq, .Lsubnormal_exponent
        or      r14, r4
        jnb     r14.0, .Lsubnormal_exponent
        add     r4, #1
        addc    r5, #0

.Lsubnormal_exponent:
        jmpr    cc_uc, .Lreturn

.Lzero:
        mov     r4, #0
        mov     r5, #0
        jmpr    cc_uc, .Lreturn

        .size   ___truncdfsf2, .-___truncdfsf2
