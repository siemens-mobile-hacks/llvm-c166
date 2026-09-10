//===-- cmpdf2.S - C166 direct binary64 comparison ---------------------===//
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
        .section .text.___c166_cmpdf2,"ax",@progbits
#endif

        .p2align 1
        .globl  ___c166_cmpdf2
        .type   ___c166_cmpdf2,@function
___c166_cmpdf2:
        mov     r1, [r12+]
        mov     r2, [r13+]
        mov     r3, [r12+]
        mov     r4, [r13+]
        mov     r5, [r12+]
        mov     r10, [r13+]
        mov     r11, [r12]
        mov     r14, [r13]

        mov     r15, r3
        or      r15, r5
        or      r15, r11
        mov     r12, r4
        or      r12, r10
        or      r12, r14

        mov     r13, r1
        bclr    r13.15
        cmp     r13, #32752
        jmpr    cc_ugt, .Lunordered
        jmpr    cc_ne, .Lcheck_right_nan
        cmp     r15, #0
        jmpr    cc_ne, .Lunordered

.Lcheck_right_nan:
        mov     r13, r2
        bclr    r13.15
        cmp     r13, #32752
        jmpr    cc_ugt, .Lunordered
        jmpr    cc_ne, .Lcheck_sign
        cmp     r12, #0
        jmpr    cc_ne, .Lunordered

.Lcheck_sign:
        bcmp    r1.15, r2.15
        jmpr    cc_n, .Lopposite_signs

        cmp     r1, r2
        jmpr    cc_ne, .Ldifferent
        cmp     r3, r4
        jmpr    cc_ne, .Ldifferent
        cmp     r5, r10
        jmpr    cc_ne, .Ldifferent
        cmp     r11, r14
        jmpr    cc_ne, .Ldifferent

.Lequal:
        mov     r4, #2
        C166_ABI_RET

.Lopposite_signs:
        mov     r13, r1
        bclr    r13.15
        or      r13, r15
        mov     r15, r2
        bclr    r15.15
        or      r15, r12
        or      r13, r15
        jmpr    cc_eq, .Lequal
        jb      r1.15, .Lless

.Lgreater:
        mov     r4, #4
        C166_ABI_RET

.Ldifferent:
        jmpr    cc_ult, .Lraw_less
        jb      r1.15, .Lless
        jmpr    cc_uc, .Lgreater

.Lraw_less:
        jnb     r1.15, .Lless
        jmpr    cc_uc, .Lgreater

.Lless:
        mov     r4, #1
        C166_ABI_RET

.Lunordered:
        mov     r4, #8
        C166_ABI_RET

        .size   ___c166_cmpdf2, .-___c166_cmpdf2
