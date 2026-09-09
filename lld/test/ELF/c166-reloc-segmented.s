# REQUIRES: c166
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o

## Exercise both representable boundaries of every 24-bit segmented-data
## relocation, both PC-relative relocation widths, and positive/negative
## COF16 addends in one linked image.
# RUN: ld.lld %t.o --section-start=.text=0x120100 \
# RUN:   --defsym=zero=0 --defsym=max24=0xffffff \
# RUN:   --defsym=pc8_min_target=pc8_min-255 \
# RUN:   --defsym=pc8_max_target=pc8_max+255 \
# RUN:   --defsym=pc16_min_target=pc16_min-32768 \
# RUN:   --defsym=pc16_max_target=pc16_max+32767 -o %t
# RUN: llvm-objdump -s --section=.text %t | FileCheck %s --check-prefix=DATA
# DATA:      Contents of section .text:
# DATA-NEXT:  120100 00ff0000 ffff00a8 ffab00c0 ffff0040
# DATA-NEXT:  120110 ff7f0080 ffbf0d80 0d7f0080 ff7fca00
# DATA-NEXT:  120120 2401ca00 28010000

## The six unsigned 24-bit address relocations reject both sides instead of
## silently truncating before selecting SEG/SOF/PAG/POF/DPP bits.
# RUN: not ld.lld %t.o --section-start=.text=0x120100 \
# RUN:   --defsym=zero=-1 --defsym=max24=0xffffff \
# RUN:   --defsym=pc8_min_target=pc8_min-255 \
# RUN:   --defsym=pc8_max_target=pc8_max+255 \
# RUN:   --defsym=pc16_min_target=pc16_min-32768 \
# RUN:   --defsym=pc16_max_target=pc16_max+32767 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=U24-LOW
# U24-LOW-DAG: relocation R_C166_SEG8 out of range: {{.*}} is not in [0, 16777215]; references 'zero'
# U24-LOW-DAG: relocation R_C166_SOF16 out of range: {{.*}} is not in [0, 16777215]; references 'zero'
# U24-LOW-DAG: relocation R_C166_PAG10 out of range: {{.*}} is not in [0, 16777215]; references 'zero'
# U24-LOW-DAG: relocation R_C166_POF14 out of range: {{.*}} is not in [0, 16777215]; references 'zero'
# U24-LOW-DAG: relocation R_C166_DPP1_16 out of range: {{.*}} is not in [0, 16777215]; references 'zero'
# U24-LOW-DAG: relocation R_C166_DPP2_16 out of range: {{.*}} is not in [0, 16777215]; references 'zero'

# RUN: not ld.lld %t.o --section-start=.text=0x120100 \
# RUN:   --defsym=zero=0 --defsym=max24=0x1000000 \
# RUN:   --defsym=pc8_min_target=pc8_min-255 \
# RUN:   --defsym=pc8_max_target=pc8_max+255 \
# RUN:   --defsym=pc16_min_target=pc16_min-32768 \
# RUN:   --defsym=pc16_max_target=pc16_max+32767 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=U24-HIGH
# U24-HIGH-DAG: relocation R_C166_SEG8 out of range: 16777216 is not in [0, 16777215]; references 'max24'
# U24-HIGH-DAG: relocation R_C166_SOF16 out of range: 16777216 is not in [0, 16777215]; references 'max24'
# U24-HIGH-DAG: relocation R_C166_PAG10 out of range: 16777216 is not in [0, 16777215]; references 'max24'
# U24-HIGH-DAG: relocation R_C166_POF14 out of range: 16777216 is not in [0, 16777215]; references 'max24'
# U24-HIGH-DAG: relocation R_C166_DPP1_16 out of range: 16777216 is not in [0, 16777215]; references 'max24'
# U24-HIGH-DAG: relocation R_C166_DPP2_16 out of range: 16777216 is not in [0, 16777215]; references 'max24'

## PC8 stores a signed word displacement and additionally requires an even
## target. PC16 stores a signed byte displacement.
# RUN: not ld.lld %t.o --section-start=.text=0x120100 \
# RUN:   --defsym=zero=0 --defsym=max24=0xffffff \
# RUN:   --defsym=pc8_min_target=pc8_min-257 \
# RUN:   --defsym=pc8_max_target=pc8_max+255 \
# RUN:   --defsym=pc16_min_target=pc16_min-32768 \
# RUN:   --defsym=pc16_max_target=pc16_max+32767 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=PC8-LOW
# PC8-LOW: relocation R_C166_PC8 out of range: -129 is not in [-128, 127]

# RUN: not ld.lld %t.o --section-start=.text=0x120100 \
# RUN:   --defsym=zero=0 --defsym=max24=0xffffff \
# RUN:   --defsym=pc8_min_target=pc8_min-255 \
# RUN:   --defsym=pc8_max_target=pc8_max+257 \
# RUN:   --defsym=pc16_min_target=pc16_min-32768 \
# RUN:   --defsym=pc16_max_target=pc16_max+32767 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=PC8-HIGH
# PC8-HIGH: relocation R_C166_PC8 out of range: 128 is not in [-128, 127]

# RUN: not ld.lld %t.o --section-start=.text=0x120100 \
# RUN:   --defsym=zero=0 --defsym=max24=0xffffff \
# RUN:   --defsym=pc8_min_target=pc8_min \
# RUN:   --defsym=pc8_max_target=pc8_max+255 \
# RUN:   --defsym=pc16_min_target=pc16_min-32768 \
# RUN:   --defsym=pc16_max_target=pc16_max+32767 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=PC8-ALIGN
# PC8-ALIGN: R_C166_PC8 target is not word-aligned

# RUN: not ld.lld %t.o --section-start=.text=0x120100 \
# RUN:   --defsym=zero=0 --defsym=max24=0xffffff \
# RUN:   --defsym=pc8_min_target=pc8_min-255 \
# RUN:   --defsym=pc8_max_target=pc8_max+255 \
# RUN:   --defsym=pc16_min_target=pc16_min-32769 \
# RUN:   --defsym=pc16_max_target=pc16_max+32767 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=PC16-LOW
# PC16-LOW: relocation R_C166_PC16 out of range: -32769 is not in [-32768, 32767]

# RUN: not ld.lld %t.o --section-start=.text=0x120100 \
# RUN:   --defsym=zero=0 --defsym=max24=0xffffff \
# RUN:   --defsym=pc8_min_target=pc8_min-255 \
# RUN:   --defsym=pc8_max_target=pc8_max+255 \
# RUN:   --defsym=pc16_min_target=pc16_min-32768 \
# RUN:   --defsym=pc16_max_target=pc16_max+32768 -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=PC16-HIGH
# PC16-HIGH: relocation R_C166_PC16 out of range: 32768 is not in [-32768, 32767]

.text
.globl _start
.globl pc8_min, pc8_max, pc16_min, pc16_max
_start:
seg_zero:
  .byte 0
  .reloc seg_zero, R_C166_SEG8, zero
seg_max:
  .byte 0
  .reloc seg_max, R_C166_SEG8, max24
sof_zero:
  .short 0
  .reloc sof_zero, R_C166_SOF16, zero
sof_max:
  .short 0
  .reloc sof_max, R_C166_SOF16, max24
pag_zero:
  .short 0xa800
  .reloc pag_zero, R_C166_PAG10, zero
pag_max:
  .short 0xa800
  .reloc pag_max, R_C166_PAG10, max24
pof_zero:
  .short 0xc000
  .reloc pof_zero, R_C166_POF14, zero
pof_max:
  .short 0xc000
  .reloc pof_max, R_C166_POF14, max24
dpp1_zero:
  .short 0
  .reloc dpp1_zero, R_C166_DPP1_16, zero
dpp1_max:
  .short 0
  .reloc dpp1_max, R_C166_DPP1_16, max24
dpp2_zero:
  .short 0
  .reloc dpp2_zero, R_C166_DPP2_16, zero
dpp2_max:
  .short 0
  .reloc dpp2_max, R_C166_DPP2_16, max24

  .byte 0x0d
pc8_min:
  .byte 0
  .reloc pc8_min, R_C166_PC8, pc8_min_target
  .byte 0x0d
pc8_max:
  .byte 0
  .reloc pc8_max, R_C166_PC8, pc8_max_target
pc16_min:
  .short 0
  .reloc pc16_min, R_C166_PC16, pc16_min_target
pc16_max:
  .short 0
  .reloc pc16_max, R_C166_PC16, pc16_max_target

cof_neg:
  .long 0xca
  .reloc cof_neg + 2, R_C166_COF16, cof_target - 2
cof_pos:
  .long 0xca
  .reloc cof_pos + 2, R_C166_COF16, cof_target + 2
cof_target:
  .short 0
