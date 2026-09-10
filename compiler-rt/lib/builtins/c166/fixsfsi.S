//===-- fixsfsi.S - C166 binary32 to signed integer conversion ----------===//
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
        .section .text.___fixsfsi,"ax",@progbits
#endif

        .p2align 1
        .globl  ___fixsfsi
        .type   ___fixsfsi,@function
___fixsfsi:
        // Logical binary32 bits arrive in R12:R13 (low:high).  Return the
        // signed result in R4:R5 (low:high).
        mov     r10, r13
        mov     r1, r13
        bclr    r1.15
        shr     r1, #7
        cmp     r1, #127
        jmpr    cc_ult, .Lzero
        cmp     r1, #159
        jmpr    cc_uge, .Lsaturate

        and     r13, #0x7f
        bset    r13.7
        add     r1, #-150
        jmpr    cc_sgt, .Lshift_left
        jmpr    cc_eq, .Lcopy
        cmp     r1, #-16
        jmpr    cc_sle, .Lshift_right_word

        mov     r3, r13
        shl     r3, r1
        neg     r1
        mov     r4, r12
        shr     r4, r1
        or      r4, r3
        mov     r5, r13
        shr     r5, r1
        jmpr    cc_uc, .Lapply_sign

.Lshift_right_word:
        add     r1, #16
        neg     r1
        mov     r4, r13
        shr     r4, r1
        mov     r5, #0
        jmpr    cc_uc, .Lapply_sign

.Lshift_left:
        mov     r3, r12
        neg     r1
        shr     r3, r1
        neg     r1
        mov     r4, r12
        shl     r4, r1
        mov     r5, r13
        shl     r5, r1
        or      r5, r3
        jmpr    cc_uc, .Lapply_sign

.Lcopy:
        mov     r4, r12
        mov     r5, r13
.Lapply_sign:
        jnb     r10.15, .Lreturn
        neg     r4
        addc    r5, #0
        neg     r5
.Lreturn:
        C166_ABI_RET

.Lzero:
        mov     r4, #0
        mov     r5, #0
        C166_ABI_RET

.Lsaturate:
        mov     r5, r10
        shr     r5, #15
        mov     r4, r5
        sub     r4, #1
        neg     r5
        xor     r5, #0x7fff
        C166_ABI_RET

        .size   ___fixsfsi, .-___fixsfsi
