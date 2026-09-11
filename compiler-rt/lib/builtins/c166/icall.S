//===-- icall.S - C166 indirect far-call veneer -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

        // Keep the indirect-call veneer in its own section.
        .section .c166.icall,"ax",@progbits
        .globl  __icall
        .type   __icall,@function
#if __C166_MEMORY_MODEL__ == 2 || __C166_MEMORY_MODEL__ == 4
        .c166_function near, __icall
#else
        .c166_function huge, __icall
#endif
__icall:
        // The caller places the far function pointer in R4:R5.
        // In the Medium model __icall itself is reached with CALLA, which
        // leaves only the return IP on the system stack.  The huge callee
        // returns with RETS and therefore requires an IP:CSP return frame.
        // Convert the caller's near frame before synthesizing the target
        // address. R1 and R2 are caller-clobbered registers.
#if __C166_MEMORY_MODEL__ == 2 || __C166_MEMORY_MODEL__ == 4
        mov     r2, csp
        atomic  #3
        pop     r1
        push    r2
        push    r1
#endif

        // RETS consumes the offset first and the segment second, so push the
        // words in high-to-low order.  ATOMIC keeps the synthetic return
        // address construction indivisible.
        atomic  #3
        push    r5
        push    r4
        rets
        .size   __icall, .-__icall
