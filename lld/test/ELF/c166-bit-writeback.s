# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x10200 --defsym=near_set=_start+64 --defsym=near_clear=_start-20 --defsym=far_set=0x20400 --defsym=far_clear=0x10000 -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
# RELOC: 0x0 R_C166_PC8_RELAX near_set
# RELOC-NEXT: 0xA R_C166_PC8_RELAX near_clear
# RELOC-NEXT: 0x14 R_C166_PC8_RELAX far_set
# RELOC-NEXT: 0x1E R_C166_PC8_RELAX far_clear
.globl _start
_start:
jbc bit_set, near_set
jnbs bit_word . bit_index, near_clear
jbc r4.5, far_set
jnbs bit_clear, far_clear
.equ bit_set, 0xf45
.equ bit_clear, 0xf53
.equ bit_word, 0x88
.equ bit_index, 11
# DIS:      10200: aa f4 1e 50{{.*}}jbc r4.5, 30
# DIS-NEXT: 10204: cc 00{{.*}}nop
# DIS-NEXT: 10206: cc 00{{.*}}nop
# DIS-NEXT: 10208: cc 00{{.*}}nop
# DIS-NEXT: 1020a: ba 88 ef b0{{.*}}jnbs psw.11, 239
# DIS-NEXT: 1020e: cc 00{{.*}}nop
# DIS-NEXT: 10210: cc 00{{.*}}nop
# DIS-NEXT: 10212: cc 00{{.*}}nop
# DIS-NEXT: 10214: aa f4 01 50{{.*}}jbc r4.5, 1
# DIS-NEXT: 10218: 0d 02{{.*}}jmpr cc_uc, 2
# DIS-NEXT: 1021a: fa 02 00 04{{.*}}jmps 2, 1024
# DIS-NEXT: 1021e: ba f5 01 30{{.*}}jnbs r5.3, 1
# DIS-NEXT: 10222: 0d 02{{.*}}jmpr cc_uc, 2
# DIS-NEXT: 10224: fa 01 00 00{{.*}}jmps 1, 0
