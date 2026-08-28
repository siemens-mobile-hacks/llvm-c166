# REQUIRES: c166
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
# RUN: not ld.lld --error-limit=0 %t.o -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=UNDEF

## Every value-bearing relocation must reject an unresolved strong symbol.
# UNDEF-DAG: undefined symbol: undef_8
# UNDEF-DAG: undefined symbol: undef_16
# UNDEF-DAG: undefined symbol: undef_32
# UNDEF-DAG: undefined symbol: undef_seg8
# UNDEF-DAG: undefined symbol: undef_seg24
# UNDEF-DAG: undefined symbol: undef_sof16
# UNDEF-DAG: undefined symbol: undef_pag10
# UNDEF-DAG: undefined symbol: undef_pof14
# UNDEF-DAG: undefined symbol: undef_pc8
# UNDEF-DAG: undefined symbol: undef_pc16
# UNDEF-DAG: undefined symbol: undef_dpp1
# UNDEF-DAG: undefined symbol: undef_dpp2
# UNDEF-DAG: undefined symbol: undef_cof16

.text
r8:
  .byte 0
  .reloc r8, R_C166_8, undef_8
r16:
  .short 0
  .reloc r16, R_C166_16, undef_16
r32:
  .long 0
  .reloc r32, R_C166_32, undef_32
seg8:
  .byte 0
  .reloc seg8, R_C166_SEG8, undef_seg8
seg24:
  .space 3
  .reloc seg24, R_C166_SEG24, undef_seg24
sof16:
  .short 0
  .reloc sof16, R_C166_SOF16, undef_sof16
pag10:
  .short 0
  .reloc pag10, R_C166_PAG10, undef_pag10
pof14:
  .short 0
  .reloc pof14, R_C166_POF14, undef_pof14
pc8:
  .byte 0
  .reloc pc8, R_C166_PC8, undef_pc8
pc16:
  .short 0
  .reloc pc16, R_C166_PC16, undef_pc16
dpp1:
  .short 0
  .reloc dpp1, R_C166_DPP1_16, undef_dpp1
dpp2:
  .short 0
  .reloc dpp2, R_C166_DPP2_16, undef_dpp2
cof16:
  .short 0
  .reloc cof16, R_C166_COF16, undef_cof16
