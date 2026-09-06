//===-- float_helpers.S - C166 floating-point helpers -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ABI entry points used by the C166 floating-point runtime:
//
//   __load8n/f   source R4 or R5:R4, allocate 8 user-stack bytes, result R10
//   __store8n/f  source R10, destination R4 or R5:R4
//   __cpXYb/w    source R4 or R5:R4, destination R10 or R11:R10, count R3
//
// X and Y are n(ear) or f(ar).  Counts at the internal __cp entry points are
// non-zero.  Far data accesses use the page register supplied by the caller.
//
// A double result uses an eight-byte stack block. __load8x publishes the
// temporary address in R10. The near helper also mirrors it to R4; the far
// helper uses R4 as its source cursor.
//
//===----------------------------------------------------------------------===//

#if __C166_MEMORY_MODEL__ == 2
#define C166_ABI_RET ret
#else
#define C166_ABI_RET rets
#endif

        .text
        .p2align 1

        .globl  __load8n
        .globl  __ldnoal8n
        .type   __load8n,@function
        .type   __ldnoal8n,@function
__load8n:
        sub     r0, #4
        sub     r0, #4
        mov     r10, r0
__ldnoal8n:
        mov     r2, r10
        mov     [r2+], [r4]
        add     r4, #2
        mov     [r2+], [r4]
        add     r4, #2
        mov     [r2+], [r4]
        add     r4, #2
        mov     [r2], [r4]
        mov     r4, r10
        C166_ABI_RET
        .size   __ldnoal8n, .-__ldnoal8n
        .size   __load8n, .-__load8n

        .globl  __store8n
        .type   __store8n,@function
__store8n:
        mov     [r4+], [r10]
        add     r10, #2
        mov     [r4+], [r10]
        add     r10, #2
        mov     [r4+], [r10]
        add     r10, #2
        mov     [r4], [r10]
        C166_ABI_RET
        .size   __store8n, .-__store8n

        .globl  __load8f
        .globl  __ldnoal8f
        .type   __load8f,@function
        .type   __ldnoal8f,@function
__load8f:
        sub     r0, #4
        sub     r0, #4
        mov     r10, r0
__ldnoal8f:
        extp    r5, #1
        mov     r2, [r4+]
        mov     [r10], r2
        add     r10, #2
        extp    r5, #1
        mov     r2, [r4+]
        mov     [r10], r2
        add     r10, #2
        extp    r5, #1
        mov     r2, [r4+]
        mov     [r10], r2
        add     r10, #2
        extp    r5, #1
        mov     r2, [r4]
        mov     [r10], r2
        sub     r10, #6
        C166_ABI_RET
        .size   __ldnoal8f, .-__ldnoal8f
        .size   __load8f, .-__load8f

        .globl  __store8f
        .type   __store8f,@function
__store8f:
        mov     r2, [r10+]
        extp    r5, #1
        mov     [r4], r2
        add     r4, #2
        mov     r2, [r10+]
        extp    r5, #1
        mov     [r4], r2
        add     r4, #2
        mov     r2, [r10+]
        extp    r5, #1
        mov     [r4], r2
        add     r4, #2
        mov     r2, [r10]
        extp    r5, #1
        mov     [r4], r2
        C166_ABI_RET
        .size   __store8f, .-__store8f

        .macro  SAVE_R2
        mov     [-r0], r2
        .endm

        .macro  RESTORE_R2
        mov     r2, [r0+]
        .endm

        .globl  __cpnnb
        .type   __cpnnb,@function
__cpnnb:
.Lcpnnb_loop:
        movb    [r10+], [r4]
        add     r4, #1
        sub     r3, #1
        jmpr    cc_ne, .Lcpnnb_loop
        C166_ABI_RET
        .size   __cpnnb, .-__cpnnb

        .globl  __cpnnw
        .type   __cpnnw,@function
__cpnnw:
.Lcpnnw_loop:
        mov     [r10+], [r4]
        add     r4, #2
        sub     r3, #1
        jmpr    cc_ne, .Lcpnnw_loop
        C166_ABI_RET
        .size   __cpnnw, .-__cpnnw

        .globl  __cpnfb
        .type   __cpnfb,@function
__cpnfb:
        SAVE_R2
.Lcpnfb_loop:
        movb    rl2, [r4]
        extp    r11, #1
        movb    [r10], rl2
        add     r4, #1
        add     r10, #1
        sub     r3, #1
        jmpr    cc_ne, .Lcpnfb_loop
        RESTORE_R2
        C166_ABI_RET
        .size   __cpnfb, .-__cpnfb

        .globl  __cpnfw
        .type   __cpnfw,@function
__cpnfw:
        SAVE_R2
.Lcpnfw_loop:
        mov     r2, [r4]
        extp    r11, #1
        mov     [r10], r2
        add     r4, #2
        add     r10, #2
        sub     r3, #1
        jmpr    cc_ne, .Lcpnfw_loop
        RESTORE_R2
        C166_ABI_RET
        .size   __cpnfw, .-__cpnfw

        .globl  __cpfnb
        .type   __cpfnb,@function
__cpfnb:
        SAVE_R2
.Lcpfnb_loop:
        extp    r5, #1
        movb    rl2, [r4]
        movb    [r10], rl2
        add     r4, #1
        add     r10, #1
        sub     r3, #1
        jmpr    cc_ne, .Lcpfnb_loop
        RESTORE_R2
        C166_ABI_RET
        .size   __cpfnb, .-__cpfnb

        .globl  __cpfnw
        .type   __cpfnw,@function
__cpfnw:
        SAVE_R2
.Lcpfnw_loop:
        extp    r5, #1
        mov     r2, [r4]
        mov     [r10], r2
        add     r4, #2
        add     r10, #2
        sub     r3, #1
        jmpr    cc_ne, .Lcpfnw_loop
        RESTORE_R2
        C166_ABI_RET
        .size   __cpfnw, .-__cpfnw

#if __C166_MEMORY_MODEL__ == 2
        .section .c166.near.text,"ax",@progbits
#else
        .section .text.___c166_df_limbs_increment,"ax",@progbits
#endif
        .p2align 1
        .globl  ___c166_df_limbs_increment
        .type   ___c166_df_limbs_increment,@function
___c166_df_limbs_increment:
        mov     r1, #0
.Ldf_increment_loop:
        mov     r2, r12
        add     r2, r1
        mov     r3, [r2]
        add     r3, #1
        mov     [r2], r3
        jmpr    cc_uge, .Ldf_increment_done
        cmpi2   r1, #6
        jmpr    cc_ne, .Ldf_increment_loop
.Ldf_increment_done:
        C166_ABI_RET
        .size   ___c166_df_limbs_increment, .-___c166_df_limbs_increment
