# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --implicit-check-not='{{diswdt|einit|idle|pwrdn|srvwdt|srst}}'

# Isolate damaged instructions in separate sections so decoder recovery
# cannot consume bytes belonging to the next case. Other decodings are legal,
# but no damaged sequence may decode as a protected instruction.

.section .bad0,"ax",@progbits
.byte 0xa5, 0x5b, 0xa5, 0xa5
# CHECK-LABEL: Disassembly of section .bad0:

.section .bad1,"ax",@progbits
.byte 0xa5, 0x5a, 0xa4, 0xa5
# CHECK-LABEL: Disassembly of section .bad1:

.section .bad2,"ax",@progbits
.byte 0xa5, 0x5a, 0xa5, 0xa4
# CHECK-LABEL: Disassembly of section .bad2:

.section .bad3,"ax",@progbits
.byte 0xb5, 0x4b, 0xb5, 0xb5
# CHECK-LABEL: Disassembly of section .bad3:

.section .bad4,"ax",@progbits
.byte 0xb5, 0x4a, 0xb4, 0xb5
# CHECK-LABEL: Disassembly of section .bad4:

.section .bad5,"ax",@progbits
.byte 0xb5, 0x4a, 0xb5, 0xb4
# CHECK-LABEL: Disassembly of section .bad5:

.section .bad6,"ax",@progbits
.byte 0x87, 0x79, 0x87, 0x87
# CHECK-LABEL: Disassembly of section .bad6:

.section .bad7,"ax",@progbits
.byte 0x87, 0x78, 0x86, 0x87
# CHECK-LABEL: Disassembly of section .bad7:

.section .bad8,"ax",@progbits
.byte 0x87, 0x78, 0x87, 0x86
# CHECK-LABEL: Disassembly of section .bad8:

.section .bad9,"ax",@progbits
.byte 0x97, 0x69, 0x97, 0x97
# CHECK-LABEL: Disassembly of section .bad9:

.section .bad10,"ax",@progbits
.byte 0x97, 0x68, 0x96, 0x97
# CHECK-LABEL: Disassembly of section .bad10:

.section .bad11,"ax",@progbits
.byte 0x97, 0x68, 0x97, 0x96
# CHECK-LABEL: Disassembly of section .bad11:

.section .bad12,"ax",@progbits
.byte 0xa7, 0x59, 0xa7, 0xa7
# CHECK-LABEL: Disassembly of section .bad12:

.section .bad13,"ax",@progbits
.byte 0xa7, 0x58, 0xa6, 0xa7
# CHECK-LABEL: Disassembly of section .bad13:

.section .bad14,"ax",@progbits
.byte 0xa7, 0x58, 0xa7, 0xa6
# CHECK-LABEL: Disassembly of section .bad14:

.section .bad15,"ax",@progbits
.byte 0xb7, 0x49, 0xb7, 0xb7
# CHECK-LABEL: Disassembly of section .bad15:

.section .bad16,"ax",@progbits
.byte 0xb7, 0x48, 0xb6, 0xb7
# CHECK-LABEL: Disassembly of section .bad16:

.section .bad17,"ax",@progbits
.byte 0xb7, 0x48, 0xb7, 0xb6
# CHECK-LABEL: Disassembly of section .bad17:
