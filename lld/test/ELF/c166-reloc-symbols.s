# REQUIRES: c166
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf \
# RUN:   %S/Inputs/c166-reloc-symbols-defs.s -o %t.defs.o

## The relocation input deliberately contains every successful ELF symbol
## resolution class: local definition, global definition from another object,
## weak definition from another object, undefined weak, common, and an
## STT_SECTION reference.  Check the input symbol kinds before checking the
## linked values so that equal numeric addresses cannot hide a resolution bug.
# RUN: llvm-readobj --symbols --relocations %t.o %t.defs.o | \
# RUN:   FileCheck %s --check-prefix=INPUT
# INPUT-DAG:  R_C166_8 local_object 0xFFEDCC00
# INPUT-DAG:  R_C166_8 global_object 0xFFEDCC00
# INPUT-DAG:  R_C166_8 weak_object 0xFFEDCC00
# INPUT-DAG:  R_C166_8 weak_undefined_object 0x6
# INPUT-DAG:  R_C166_8 common_object 0xFFEDCC00
# INPUT-DAG:  R_C166_8 .symbols 0xFFEDCC00
# INPUT-DAG:  R_C166_SEG24 local_object 0x0
# INPUT-DAG:  R_C166_SEG24 global_object 0x0
# INPUT-DAG:  R_C166_SEG24 weak_object 0x0
# INPUT-DAG:  R_C166_SEG24 weak_undefined_object 0x123406
# INPUT-DAG:  R_C166_SEG24 common_object 0x0
# INPUT-DAG:  R_C166_SEG24 .symbols 0x0
# INPUT-DAG:  Name: .symbols
# INPUT-DAG:  Type: Section
# INPUT-DAG:  Name: local_object
# INPUT-DAG:  Binding: Local
# INPUT-DAG:  Type: Object
# INPUT-DAG:  Name: weak_undefined_object
# INPUT-DAG:  Binding: Weak
# INPUT-DAG:  Section: Undefined
# INPUT-DAG:  Name: common_object
# INPUT-DAG:  Binding: Global
# INPUT-DAG:  Type: Object
# INPUT-DAG:  Section: Common

## Absolute and segmented relocations use symbols around 0x123400.  Negative
## addends on the narrow fields prove that LLD resolves the symbol before
## applying field-width checks instead of rejecting or truncating the raw S.
# RUN: ld.lld --gc-sections -e _start %t.o %t.defs.o \
# RUN:   --section-start=.text=0x1000 \
# RUN:   --section-start=.symbols=0x123400 --section-start=.bss=0x123410 \
# RUN:   -o %t.abs
# RUN: llvm-objdump -s --section=.text %t.abs | \
# RUN:   FileCheck %s --check-prefix=ABS
# ABS:      Contents of section .text:
# ABS-NEXT:  {{[0-9a-f]+}} 00020406 10000034 02340434 06341034
# ABS-NEXT:  {{[0-9a-f]+}} 00340034 12000234 12000434 12000600
# ABS-NEXT:  {{[0-9a-f]+}} 00001034 12000034 12001212 12561212
# ABS-NEXT:  {{[0-9a-f]+}} 00340234 04340634 10340034 48a848a8
# ABS-NEXT:  {{[0-9a-f]+}} 48a860a8 48a848a8 00f402f4 04f406f4
# ABS-NEXT:  {{[0-9a-f]+}} 10f400f4 00740274 04740674 10740074
# ABS-NEXT:  {{[0-9a-f]+}} 00b402b4 04b406b4 10b400b4 12003412
# ABS-NEXT:  {{[0-9a-f]+}} 02341204 34120634 12103412 0034

## PC8, PC16, and COF16 need nearby code symbols.  The use object's .text is
## laid out first; its local target is at +0x30, the aligned definitions from
## the second object are at +0x34/+0x36, common is fixed at +0x80, and the
## undefined weak reference uses an addend to name +0x60.  The section-symbol
## case names the beginning of the output .text section.
# RUN: ld.lld --gc-sections -e _pc_start %t.o %t.defs.o \
# RUN:   --section-start=.pc=0x120000 --section-start=.text=0x120032 \
# RUN:   --section-start=.symbols=0x123400 --section-start=.bss=0x120080 \
# RUN:   -o %t.pc
# RUN: llvm-objdump -s --section=.pc %t.pc | \
# RUN:   FileCheck %s --check-prefix=PC
# PC:      Contents of section .pc:
# PC-NEXT:  120000 0d170d18 0d180d2c 0d3b0dfa 24002600
# PC-NEXT:  120010 26004e00 6c00eaff ca003000 ca003400
# PC-NEXT:  120020 ca003600 ca006000 ca008000 ca000000
# PC-NEXT:  120030 0000

.weak weak_undefined_object
.weak weak_undefined_code

.section .symbols,"aw",@progbits
.p2align 1
.type local_object,@object
local_object:
  .short 0
.size local_object, .-local_object

.text
.globl _start
_start:
r8_local:
  .byte 0
  .reloc r8_local, R_C166_8, local_object - 0x123400
r8_global:
  .byte 0
  .reloc r8_global, R_C166_8, global_object - 0x123400
r8_weak:
  .byte 0
  .reloc r8_weak, R_C166_8, weak_object - 0x123400
r8_weak_undefined:
  .byte 0
  .reloc r8_weak_undefined, R_C166_8, weak_undefined_object + 6
r8_common:
  .byte 0
  .reloc r8_common, R_C166_8, common_object - 0x123400
r8_section:
  .byte 0
  .reloc r8_section, R_C166_8, .symbols - 0x123400

.macro reloc_six_16 prefix, kind, initial=0
\prefix\()_local:
  .short \initial
  .reloc \prefix\()_local, \kind, local_object - 0x120000
\prefix\()_global:
  .short \initial
  .reloc \prefix\()_global, \kind, global_object - 0x120000
\prefix\()_weak:
  .short \initial
  .reloc \prefix\()_weak, \kind, weak_object - 0x120000
\prefix\()_weak_undefined:
  .short \initial
  .reloc \prefix\()_weak_undefined, \kind, weak_undefined_object + 0x3406
\prefix\()_common:
  .short \initial
  .reloc \prefix\()_common, \kind, common_object - 0x120000
\prefix\()_section:
  .short \initial
  .reloc \prefix\()_section, \kind, .symbols - 0x120000
.endm

reloc_six_16 r16, R_C166_16

.macro reloc_six_32 prefix, kind
\prefix\()_local:
  .long 0
  .reloc \prefix\()_local, \kind, local_object
\prefix\()_global:
  .long 0
  .reloc \prefix\()_global, \kind, global_object
\prefix\()_weak:
  .long 0
  .reloc \prefix\()_weak, \kind, weak_object
\prefix\()_weak_undefined:
  .long 0
  .reloc \prefix\()_weak_undefined, \kind, weak_undefined_object + 6
\prefix\()_common:
  .long 0
  .reloc \prefix\()_common, \kind, common_object
\prefix\()_section:
  .long 0
  .reloc \prefix\()_section, \kind, .symbols
.endm

reloc_six_32 r32, R_C166_32

.macro reloc_six_seg8 prefix, kind
\prefix\()_local:
  .byte 0
  .reloc \prefix\()_local, \kind, local_object
\prefix\()_global:
  .byte 0
  .reloc \prefix\()_global, \kind, global_object
\prefix\()_weak:
  .byte 0
  .reloc \prefix\()_weak, \kind, weak_object
\prefix\()_weak_undefined:
  .byte 0
  .reloc \prefix\()_weak_undefined, \kind, weak_undefined_object + 0x560000
\prefix\()_common:
  .byte 0
  .reloc \prefix\()_common, \kind, common_object
\prefix\()_section:
  .byte 0
  .reloc \prefix\()_section, \kind, .symbols
.endm

reloc_six_seg8 seg8, R_C166_SEG8

.macro reloc_six_native16 prefix, kind, initial=0
\prefix\()_local:
  .short \initial
  .reloc \prefix\()_local, \kind, local_object
\prefix\()_global:
  .short \initial
  .reloc \prefix\()_global, \kind, global_object
\prefix\()_weak:
  .short \initial
  .reloc \prefix\()_weak, \kind, weak_object
\prefix\()_weak_undefined:
  .short \initial
  .reloc \prefix\()_weak_undefined, \kind, weak_undefined_object + 0x3406
\prefix\()_common:
  .short \initial
  .reloc \prefix\()_common, \kind, common_object
\prefix\()_section:
  .short \initial
  .reloc \prefix\()_section, \kind, .symbols
.endm

reloc_six_native16 sof16, R_C166_SOF16

.macro reloc_six_page prefix, kind, initial, weakadd
\prefix\()_local:
  .short \initial
  .reloc \prefix\()_local, \kind, local_object
\prefix\()_global:
  .short \initial
  .reloc \prefix\()_global, \kind, global_object
\prefix\()_weak:
  .short \initial
  .reloc \prefix\()_weak, \kind, weak_object
\prefix\()_weak_undefined:
  .short \initial
  .reloc \prefix\()_weak_undefined, \kind, weak_undefined_object + \weakadd
\prefix\()_common:
  .short \initial
  .reloc \prefix\()_common, \kind, common_object
\prefix\()_section:
  .short \initial
  .reloc \prefix\()_section, \kind, .symbols
.endm

reloc_six_page pag10, R_C166_PAG10, 0xa800, 0x180000
reloc_six_page pof14, R_C166_POF14, 0xc000, 0x3406
reloc_six_page dpp1, R_C166_DPP1_16, 0, 0x3406
reloc_six_page dpp2, R_C166_DPP2_16, 0, 0x3406

.macro reloc_six_seg24 prefix, kind
\prefix\()_local:
  .space 3
  .reloc \prefix\()_local, \kind, local_object
\prefix\()_global:
  .space 3
  .reloc \prefix\()_global, \kind, global_object
\prefix\()_weak:
  .space 3
  .reloc \prefix\()_weak, \kind, weak_object
\prefix\()_weak_undefined:
  .space 3
  .reloc \prefix\()_weak_undefined, \kind, weak_undefined_object + 0x123406
\prefix\()_common:
  .space 3
  .reloc \prefix\()_common, \kind, common_object
\prefix\()_section:
  .space 3
  .reloc \prefix\()_section, \kind, .symbols
.endm

reloc_six_seg24 seg24, R_C166_SEG24

## The PC-relative groups are kept in a dedicated input section so the first
## absolute-relocation image above can ignore them and the link can place the
## relocation sites independently from the definitions in .text.
.section .pc,"ax",@progbits
.globl _pc_start
_pc_start:
.type local_code,@function

.macro reloc_six_pc8
pc8_local_opcode:
  .byte 0x0d
pc8_local:
  .byte 0
  .reloc pc8_local, R_C166_PC8, local_code
pc8_global_opcode:
  .byte 0x0d
pc8_global:
  .byte 0
  .reloc pc8_global, R_C166_PC8, global_code
pc8_weak_opcode:
  .byte 0x0d
pc8_weak:
  .byte 0
  .reloc pc8_weak, R_C166_PC8, weak_code
pc8_weak_undefined_opcode:
  .byte 0x0d
pc8_weak_undefined:
  .byte 0
  .reloc pc8_weak_undefined, R_C166_PC8, weak_undefined_code + 0x120060
pc8_common_opcode:
  .byte 0x0d
pc8_common:
  .byte 0
  .reloc pc8_common, R_C166_PC8, common_code
pc8_section_opcode:
  .byte 0x0d
pc8_section:
  .byte 0
  .reloc pc8_section, R_C166_PC8, .pc
.endm

reloc_six_pc8

.macro reloc_six_pc16
pc16_local:
  .short 0
  .reloc pc16_local, R_C166_PC16, local_code
pc16_global:
  .short 0
  .reloc pc16_global, R_C166_PC16, global_code
pc16_weak:
  .short 0
  .reloc pc16_weak, R_C166_PC16, weak_code
pc16_weak_undefined:
  .short 0
  .reloc pc16_weak_undefined, R_C166_PC16, weak_undefined_code + 0x120060
pc16_common:
  .short 0
  .reloc pc16_common, R_C166_PC16, common_code
pc16_section:
  .short 0
  .reloc pc16_section, R_C166_PC16, .pc
.endm

reloc_six_pc16

.macro reloc_six_cof16
cof_local:
  .long 0xca
  .reloc cof_local + 2, R_C166_COF16, local_code
cof_global:
  .long 0xca
  .reloc cof_global + 2, R_C166_COF16, global_code
cof_weak:
  .long 0xca
  .reloc cof_weak + 2, R_C166_COF16, weak_code
cof_weak_undefined:
  .long 0xca
  .reloc cof_weak_undefined + 2, R_C166_COF16, weak_undefined_code + 0x120060
cof_common:
  .long 0xca
  .reloc cof_common + 2, R_C166_COF16, common_code
cof_section:
  .long 0xca
  .reloc cof_section + 2, R_C166_COF16, .pc
.endm

reloc_six_cof16

local_code:
  .short 0
.size local_code, .-local_code
