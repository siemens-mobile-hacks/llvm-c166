//===-- fp64_unpack.S - C166 binary64 unpacking -------------------------===//
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
        .section .text.___c166_df_unpack,"ax",@progbits
#endif

        .p2align 1
        .globl  ___c166_df_unpack
        .hidden ___c166_df_unpack
        .type   ___c166_df_unpack,@function
___c166_df_unpack:
        // Source words are most-significant first.  The internal limb order
        // written to R12 is least-significant first.  R5:R4 returns the
        // sign/kind word and the effective exponent, respectively.
        mov     r10, [r13]
        mov     r11, [r13 + #2]
        mov     r14, [r13 + #4]
        mov     r15, [r13 + #6]

        mov     r1, r10
        and     r1, #0x000f
        or      r1, r11
        or      r1, r14
        or      r1, r15

        mov     r5, r10
        and     r5, #0x8000
        mov     r4, r10
        shr     r4, #4
        and     r4, #0x07ff

        cmp     r4, #0x07ff
        jmpr    cc_eq, .Lspecial
        and     r10, #0x000f
        cmp     r4, #0
        jmpr    cc_ne, .Lnormal
        cmp     r1, #0
        jmpr    cc_eq, .Lzero

        mov     r4, #1
.Lnormalize:
        sub     r4, #1
        shl     r15, #1
        addc    r14, r14
        addc    r11, r11
        addc    r10, r10
        jnb     r10.4, .Lnormalize

.Lnormal:
        bset    r10.4

.Lstore:
        mov     [r12], r15
        mov     [r12 + #2], r14
        mov     [r12 + #4], r11
        mov     [r12 + #6], r10
        C166_ABI_RET

.Lzero:
        bset    r5.0
        jmpr    cc_uc, .Lstore

.Lspecial:
        cmp     r1, #0
        jmpr    cc_eq, .Linfinity
        bset    r10.3
        bset    r5.0
        bset    r5.1
        jmpr    cc_uc, .Lstore

.Linfinity:
        bset    r5.1
        jmpr    cc_uc, .Lstore

        .size   ___c166_df_unpack, .-___c166_df_unpack
