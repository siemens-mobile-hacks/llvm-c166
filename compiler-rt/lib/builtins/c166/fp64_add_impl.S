//===-- fp64_add_impl.S - C166 binary64 addition core ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#if __C166_MEMORY_MODEL__ == 2
#define C166_ABI_CALL(symbol) calla cc_uc, cof(symbol)
#define C166_ABI_JUMP(symbol) jmpa cc_uc, cof(symbol)
#define C166_ABI_RET ret
        .section .c166.near.text,"ax",@progbits
#else
#define C166_ABI_CALL(symbol) calls seg(symbol), sof(symbol)
#define C166_ABI_JUMP(symbol) jmps seg(symbol), sof(symbol)
#define C166_ABI_RET rets
        .section .text.___c166_addsubdf3,"ax",@progbits
#endif

        .p2align 1
        .globl  ___c166_addsubdf3
        .hidden ___c166_addsubdf3
        .type   ___c166_addsubdf3,@function
___c166_addsubdf3:
        // Public arguments use the ordinary internal C ABI:
        //   R12 destination, R13 left, R14 right, R15 right-sign toggle.
        // The finite path keeps both significands in registers.  A uses
        // R7:R8:R9:R5 and B uses R6:R12:R13:R14, most-significant first.
        mov     [-r0], r6
        mov     [-r0], r7
        mov     [-r0], r8
        mov     [-r0], r9
        mov     r10, r12

        mov     r11, r13
        mov     r7, [r11+]
        mov     r8, [r11+]
        mov     r9, [r11+]
        mov     r5, [r11]

        mov     r11, r14
        mov     r6, [r11+]
        mov     r12, [r11+]
        mov     r13, [r11+]
        mov     r14, [r11]

        mov     r1, r7
        shr     r1, #4
        and     r1, #0x07ff
        mov     r2, r6
        shr     r2, #4
        and     r2, #0x07ff

        mov     r3, r7
        and     r3, #0x000f
        or      r3, r8
        or      r3, r9
        or      r3, r5
        mov     r11, r6
        and     r11, #0x000f
        or      r11, r12
        or      r11, r13
        or      r11, r14

        mov     r4, r7
        and     r4, #0x8000
        xor     r15, r6
        and     r15, #0x8000

        // NaNs take precedence over infinities.  Preserve their payload and
        // quiet the selected operand before returning it.
        cmp     r1, #0x07ff
        jmpr    cc_ne, .Lcheck_b_nan
        cmp     r3, #0
        jmpr    cc_ne, .Lreturn_a_nan
.Lcheck_b_nan:
        cmp     r2, #0x07ff
        jmpr    cc_ne, .Lcheck_a_infinity
        cmp     r11, #0
        jmpr    cc_ne, .Lreturn_b_nan

.Lcheck_a_infinity:
        cmp     r1, #0x07ff
        jmpr    cc_ne, .Lcheck_b_infinity
        cmp     r2, #0x07ff
        jmpr    cc_ne, .Lreturn_a
        cmp     r4, r15
        jmpr    cc_ne, .Lreturn_nan
        jmpr    cc_uc, .Lreturn_a
.Lcheck_b_infinity:
        cmp     r2, #0x07ff
        jmpr    cc_eq, .Lreturn_b

        // Exact zeros retain the non-zero operand.  Two zeros use the sign
        // common to both operands, which implements round-to-nearest +0.
        cmp     r1, #0
        jmpr    cc_ne, .Lcheck_b_zero
        cmp     r3, #0
        jmpr    cc_ne, .Lcheck_b_zero
        cmp     r2, #0
        jmpr    cc_ne, .Lreturn_b
        cmp     r11, #0
        jmpr    cc_ne, .Lreturn_b
        and     r4, r15
        mov     r7, r4
        jmpr    cc_uc, .Lreturn_high
.Lcheck_b_zero:
        cmp     r2, #0
        jmpr    cc_ne, .Lfinite
        cmp     r11, #0
        jmpr    cc_eq, .Lreturn_a
        jmpr    cc_uc, .Lfinite

.Lreturn_a_nan:
        bset    r7.3
        jmpr    cc_uc, .Lstore_special
.Lreturn_b_nan:
        bset    r6.3
.Lreturn_b:
        bclr    r6.15
        or      r6, r15
        mov     r7, r6
        mov     r8, r12
        mov     r9, r13
        mov     r5, r14
        jmpr    cc_uc, .Lstore_special
.Lreturn_nan:
        mov     r7, #0x7ff8
.Lreturn_high:
        mov     r8, #0
        mov     r9, #0
        mov     r5, #0
.Lreturn_a:
.Lstore_special:
        add     r10, #8
        mov     [-r10], r5
        mov     [-r10], r9
        mov     [-r10], r8
        mov     [-r10], r7
        C166_ABI_JUMP(.Lreturn)

.Lfinite:
        // Convert both finite operands to normalized 53-bit significands.
        and     r7, #0x000f
        cmp     r1, #0
        jmpr    cc_ne, .La_normal
        mov     r1, #1
.La_normalize:
        sub     r1, #1
        shl     r5, #1
        addc    r9, r9
        addc    r8, r8
        addc    r7, r7
        jnb     r7.4, .La_normalize
.La_normal:
        bset    r7.4

        and     r6, #0x000f
        cmp     r2, #0
        jmpr    cc_ne, .Lb_normal
        mov     r2, #1
.Lb_normalize:
        sub     r2, #1
        shl     r14, #1
        addc    r13, r13
        addc    r12, r12
        addc    r6, r6
        jnb     r6.4, .Lb_normalize
.Lb_normal:
        bset    r6.4

        // Reserve guard, round, and sticky bits at the bottom of each value.
        mov     r3, #3
.Lguard_shift:
        shl     r5, #1
        addc    r9, r9
        addc    r8, r8
        addc    r7, r7
        shl     r14, #1
        addc    r13, r13
        addc    r12, r12
        addc    r6, r6
        sub     r3, #1
        jmpr    cc_ne, .Lguard_shift

        // Put the larger magnitude in A so alignment and subtraction have a
        // single path.
        cmp     r1, r2
        jmpr    cc_sgt, .Lordered
        jmpr    cc_slt, .Lswap
        cmp     r7, r6
        jmpr    cc_ugt, .Lordered
        jmpr    cc_ult, .Lswap
        cmp     r8, r12
        jmpr    cc_ugt, .Lordered
        jmpr    cc_ult, .Lswap
        cmp     r9, r13
        jmpr    cc_ugt, .Lordered
        jmpr    cc_ult, .Lswap
        cmp     r5, r14
        jmpr    cc_uge, .Lordered

.Lswap:
        mov     r3, r1
        mov     r1, r2
        mov     r2, r3
        mov     r3, r4
        mov     r4, r15
        mov     r15, r3
        mov     r3, r5
        mov     r5, r14
        mov     r14, r3
        mov     r3, r9
        mov     r9, r13
        mov     r13, r3
        mov     r3, r8
        mov     r8, r12
        mov     r12, r3
        mov     r3, r7
        mov     r7, r6
        mov     r6, r3

.Lordered:
        sub     r2, r1
        neg     r2
        jmpr    cc_eq, .Lcombine
        cmp     r2, #64
        jmpr    cc_ult, .Lalign_setup
        mov     r6, #0
        mov     r12, #0
        mov     r13, #0
        mov     r14, #1
        jmpr    cc_uc, .Lcombine

.Lalign_setup:
        mov     r3, #0
.Lalign:
        or      r3, r14
        shr     r14, #1
        bmov    r14.15, r13.0
        shr     r13, #1
        bmov    r13.15, r12.0
        shr     r12, #1
        bmov    r12.15, r6.0
        shr     r6, #1
        sub     r2, #1
        jmpr    cc_ne, .Lalign
        and     r3, #1
        or      r14, r3

.Lcombine:
        cmp     r4, r15
        jmpr    cc_ne, .Lsubtract
        add     r5, r14
        addc    r9, r13
        addc    r8, r12
        addc    r7, r6
        jnb     r7.8, .Lpack

        mov     r3, r5
        and     r3, #1
        shr     r5, #1
        bmov    r5.15, r9.0
        shr     r9, #1
        bmov    r9.15, r8.0
        shr     r8, #1
        bmov    r8.15, r7.0
        shr     r7, #1
        or      r5, r3
        add     r1, #1
        jmpr    cc_uc, .Lpack

.Lsubtract:
        sub     r5, r14
        subc    r9, r13
        subc    r8, r12
        subc    r7, r6
        mov     r3, r5
        or      r3, r9
        or      r3, r8
        or      r3, r7
        jmpr    cc_ne, .Lnormalize_result
        mov     r7, #0
        C166_ABI_JUMP(.Lreturn_high)

.Lnormalize_result:
        jb      r7.7, .Lpack
.Lnormalize_loop:
        shl     r5, #1
        addc    r9, r9
        addc    r8, r8
        addc    r7, r7
        sub     r1, #1
        jnb     r7.7, .Lnormalize_loop

.Lpack:
        mov     [-r0], r7
        mov     [-r0], r8
        mov     [-r0], r9
        mov     [-r0], r5
        mov     r12, r10
        mov     r13, r0
        mov     r14, r1
        mov     r15, r4
        C166_ABI_CALL(___c166_df_round_pack)
        add     r0, #8

.Lreturn:
        mov     r9, [r0+]
        mov     r8, [r0+]
        mov     r7, [r0+]
        mov     r6, [r0+]
        C166_ABI_RET
        .size   ___c166_addsubdf3, .-___c166_addsubdf3
