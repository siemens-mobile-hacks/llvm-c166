//===-- fp64_round_pack.S - C166 binary64 rounding and packing ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#if __C166_MEMORY_MODEL__ == 2
#define C166_ABI_CALL(symbol) calla cc_uc, cof(symbol)
#define C166_ABI_RET ret
        .section .c166.near.text,"ax",@progbits
#else
#define C166_ABI_CALL(symbol) calls seg(symbol), sof(symbol)
#define C166_ABI_RET rets
        .section .text.___c166_df_round_pack,"ax",@progbits
#endif

        .p2align 1
        .globl  ___c166_df_round_pack
        .hidden ___c166_df_round_pack
        .type   ___c166_df_round_pack,@function
___c166_df_round_pack:
        // R12 is the packed destination, R13 points to four little-endian
        // significand limbs, R14 is the effective exponent, and R15 is the
        // sign bit.  R6-R8 retain the values needed across the sticky shift.
        mov     [-r0], r6
        mov     [-r0], r7
        mov     [-r0], r8
        mov     r6, r12
        mov     r7, r13
        mov     r8, r15

        cmp     r14, #0x07ff
        jmpr    cc_slt, .Lfinite
        or      r8, #0x7ff0
        mov     r4, r8
        mov     r10, #0
        mov     r11, #0
        mov     r12, #0
        jmpr    cc_uc, .Lstore

.Lfinite:
        cmp     r14, #0
        jmpr    cc_sgt, .Lpack
        mov     r13, #1
        sub     r13, r14
        mov     r12, r7
        C166_ABI_CALL(___c166_df_limbs_shift_right_sticky)
        mov     r14, #0

.Lpack:
        mov     r1, [r7+]
        mov     r2, [r7+]
        mov     r3, [r7+]
        mov     r4, [r7]

        mov     r10, r1
        shr     r10, #3
        mov     r5, r2
        shl     r5, #13
        or      r10, r5

        mov     r11, r2
        shr     r11, #3
        mov     r5, r3
        shl     r5, #13
        or      r11, r5

        mov     r12, r3
        shr     r12, #3
        mov     r5, r4
        shl     r5, #13
        or      r12, r5

        shr     r4, #3
        and     r4, #0x000f
        shl     r14, #4
        or      r4, r14
        or      r4, r8

        // Round to nearest, ties to even.  Bit 2 is the guard bit; bits 3,
        // 1, and 0 make a guarded result strictly greater than half or make
        // an exact half-way result odd.
        jnb     r1.2, .Lstore
        and     r1, #0x000b
        jmpr    cc_eq, .Lstore
        add     r10, #1
        addc    r11, #0
        addc    r12, #0
        addc    r4, #0

.Lstore:
        add     r6, #8
        mov     [-r6], r10
        mov     [-r6], r11
        mov     [-r6], r12
        mov     [-r6], r4

.Lreturn:
        mov     r8, [r0+]
        mov     r7, [r0+]
        mov     r6, [r0+]
        C166_ABI_RET
        .size   ___c166_df_round_pack, .-___c166_df_round_pack
