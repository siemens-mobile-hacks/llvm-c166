//===-- fixunssfsi.S - C166 binary32 to unsigned integer conversion -----===//
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
        .section .text.___fixunssfsi,"ax",@progbits
#endif

        .p2align 1
        .globl  ___fixunssfsi
        .type   ___fixunssfsi,@function
___fixunssfsi:
        // Logical binary32 bits arrive in R12:R13 (low:high).  Return the
        // unsigned result in R4:R5 (low:high).
        mov     r4, #0
        mov     r5, #0
        jb      r13.15, .Lreturn

        mov     r1, r13
        shr     r1, #7
        cmp     r1, #127
        jmpr    cc_ult, .Lreturn
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
        C166_ABI_RET

.Lshift_right_word:
        add     r1, #16
        neg     r1
        mov     r4, r13
        shr     r4, r1
        C166_ABI_RET

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
        C166_ABI_RET

.Lcopy:
        mov     r4, r12
        mov     r5, r13
.Lreturn:
        C166_ABI_RET

.Lsaturate:
        mov     r4, #-1
        mov     r5, r4
        C166_ABI_RET

        .size   ___fixunssfsi, .-___fixunssfsi
