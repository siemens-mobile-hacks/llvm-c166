# REQUIRES: c166
# RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s -o %t.o
# RUN: ld.lld -m c166elf -Ttext=0x180000 -e debug_func %t.o -o %t
# RUN: llvm-dwarfdump --verify %t 2>&1 | FileCheck %s --check-prefix=VERIFY
# RUN: llvm-readobj --sections --relocations %t | FileCheck %s --check-prefix=ELF
# RUN: llvm-dwarfdump --debug-frame %t | FileCheck %s --check-prefix=FRAME
# RUN: llvm-symbolizer --obj=%t 0x180000 0x180002 | FileCheck %s --check-prefix=LINE

# Verify that LLD resolves C166 debug relocations and preserves usable line and
# debug-only call-frame tables. CALLS/RETS use the hardware system
# stack; the CIE therefore describes SP+4 and the virtual CSP:IP register.

        .file   1 "c166-debug-line.s"
        .text
        .globl  debug_func
        .type   debug_func, @function
debug_func:
        .cfi_sections .debug_frame
        .cfi_startproc
        .loc    1 100 1
        mov     r4, r12
        .loc    1 101 1
        rets
        .cfi_endproc
debug_func_end:
        .size   debug_func, .-debug_func

        # Minimal compile unit tying the generated line table to the linked
        # address range, so llvm-symbolizer exercises the final ELF too.
        .section .debug_abbrev,"",@progbits
        .byte   1
        .byte   0x11
        .byte   0
        .byte   0x10, 0x17
        .byte   0x11, 0x01
        .byte   0x12, 0x01
        .byte   0x1b, 0x08
        .byte   0x03, 0x08
        .byte   0, 0
        .byte   0

        .section .debug_info,"",@progbits
        .long   .Lcu_end-.Lcu_start
.Lcu_start:
        .short  5
        .byte   1
        .byte   4
        .long   .debug_abbrev
        .byte   1
        .long   .debug_line
        .long   debug_func
        .long   debug_func_end
        .asciz  "."
        .asciz  "c166-debug-line.s"
.Lcu_end:

# VERIFY: No errors.

# ELF: Name: .debug_frame
# ELF-NOT: Name: .rela.debug_frame
# ELF: Name: .debug_line
# ELF-NOT: Name: .rela.debug_line
# ELF-NOT: Name: .eh_frame

# FRAME: .debug_frame contents:
# FRAME: Return address column: 301
# FRAME: DW_CFA_def_cfa: SP +4
# FRAME: DW_CFA_offset_extended: RA -4
# FRAME: CFA=SP+4:
# FRAME-SAME: RA=[CFA-4]
# FRAME: FDE

# LINE: c166-debug-line.s:100:1
# LINE: c166-debug-line.s:101:1
