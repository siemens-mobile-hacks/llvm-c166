# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --implicit-check-not='{{[[:space:]]div(lu|l|u)?[[:space:]]}}'

# DIV repeats its register nibble. Mismatched copies are not a valid DIV.

.section .bad0,"ax",@progbits
.byte 0x4b, 0x01
# CHECK-LABEL: Disassembly of section .bad0:

.section .bad1,"ax",@progbits
.byte 0x4b, 0x10
# CHECK-LABEL: Disassembly of section .bad1:

.section .bad2,"ax",@progbits
.byte 0x4b, 0x0f
# CHECK-LABEL: Disassembly of section .bad2:

.section .bad3,"ax",@progbits
.byte 0x4b, 0xf0
# CHECK-LABEL: Disassembly of section .bad3:

.section .bad4,"ax",@progbits
.byte 0x4b, 0x1e
# CHECK-LABEL: Disassembly of section .bad4:

.section .bad5,"ax",@progbits
.byte 0x4b, 0xe1
# CHECK-LABEL: Disassembly of section .bad5:

.section .bad6,"ax",@progbits
.byte 0x5b, 0x01
# CHECK-LABEL: Disassembly of section .bad6:

.section .bad7,"ax",@progbits
.byte 0x5b, 0x10
# CHECK-LABEL: Disassembly of section .bad7:

.section .bad8,"ax",@progbits
.byte 0x5b, 0x0f
# CHECK-LABEL: Disassembly of section .bad8:

.section .bad9,"ax",@progbits
.byte 0x5b, 0xf0
# CHECK-LABEL: Disassembly of section .bad9:

.section .bad10,"ax",@progbits
.byte 0x5b, 0x1e
# CHECK-LABEL: Disassembly of section .bad10:

.section .bad11,"ax",@progbits
.byte 0x5b, 0xe1
# CHECK-LABEL: Disassembly of section .bad11:

.section .bad12,"ax",@progbits
.byte 0x6b, 0x01
# CHECK-LABEL: Disassembly of section .bad12:

.section .bad13,"ax",@progbits
.byte 0x6b, 0x10
# CHECK-LABEL: Disassembly of section .bad13:

.section .bad14,"ax",@progbits
.byte 0x6b, 0x0f
# CHECK-LABEL: Disassembly of section .bad14:

.section .bad15,"ax",@progbits
.byte 0x6b, 0xf0
# CHECK-LABEL: Disassembly of section .bad15:

.section .bad16,"ax",@progbits
.byte 0x6b, 0x1e
# CHECK-LABEL: Disassembly of section .bad16:

.section .bad17,"ax",@progbits
.byte 0x6b, 0xe1
# CHECK-LABEL: Disassembly of section .bad17:

.section .bad18,"ax",@progbits
.byte 0x7b, 0x01
# CHECK-LABEL: Disassembly of section .bad18:

.section .bad19,"ax",@progbits
.byte 0x7b, 0x10
# CHECK-LABEL: Disassembly of section .bad19:

.section .bad20,"ax",@progbits
.byte 0x7b, 0x0f
# CHECK-LABEL: Disassembly of section .bad20:

.section .bad21,"ax",@progbits
.byte 0x7b, 0xf0
# CHECK-LABEL: Disassembly of section .bad21:

.section .bad22,"ax",@progbits
.byte 0x7b, 0x1e
# CHECK-LABEL: Disassembly of section .bad22:

.section .bad23,"ax",@progbits
.byte 0x7b, 0xe1
# CHECK-LABEL: Disassembly of section .bad23:
