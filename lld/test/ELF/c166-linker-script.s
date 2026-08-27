# REQUIRES: c166
# RUN: split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/base.s -o %t/base.o
# RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t/overflow.s -o %t/overflow.o

## Prove exact script placement at the last legal start address for 8-byte
## near/xnear/paged objects. The relocation checks must accept objects ending
## exactly at, but not beyond, the 16 KiB page boundary.
# RUN: ld.lld -T %t/valid.ld -e _start %t/base.o -o %t/valid
# RUN: llvm-readobj --sections --symbols --relocations %t/valid | \
# RUN:   FileCheck %s --check-prefix=VALID
# RUN: llvm-objdump -d %t/valid | FileCheck %s --check-prefix=DIS

# VALID:      Name: .text
# VALID:      Address: 0x120000
# VALID:      Name: .c166.near.data
# VALID:      Address: 0x203FF8
# VALID:      Name: .c166.xnear.data
# VALID:      Address: 0x403FF8
# VALID:      Name: .paged
# VALID:      Address: 0x603FF8
# VALID:      Relocations [
# VALID-NEXT: ]
# VALID-DAG:  Name: near_object
# VALID-DAG:  Value: 0x203FF8
# VALID-DAG:  Name: xnear_object
# VALID-DAG:  Value: 0x403FF8
# VALID-DAG:  Name: paged_object
# VALID-DAG:  Value: 0x603FF8

# DIS-LABEL: <_start>:
# DIS-NEXT:  120000: f2 f4 f8 bf  mov r4, dpp2(16376)
# DIS-NEXT:  120004: f2 f5 f8 7f  mov r5, dpp1(16376)
# DIS-NEXT:  120008: d7 40 80 01  extp 384, #1
# DIS-NEXT:  12000c: f2 f6 f8 3f  mov r6, 16376
# DIS-NEXT:  120010: db 00        rets

## C166 uses one-byte ELF segment alignment, but explicitly overlapping output
## sections must still be diagnosed instead of silently sharing a VMA/LMA.
# RUN: not ld.lld -T %t/overlap.ld -e _start %t/base.o -o /dev/null 2>&1 | \
# RUN:   FileCheck %s --check-prefix=OVERLAP
# OVERLAP: error: section .c166.near.data virtual address range overlaps with .c166.xnear.data
# OVERLAP-NEXT: >>> .c166.near.data range is [0x201000, 0x201007]
# OVERLAP-NEXT: >>> .c166.xnear.data range is [0x201004, 0x20100B]

## MEMORY region accounting is part of the supported C166 linker-script
## contract. Both a legal exact fit and the first overflowing byte are tested.
# RUN: ld.lld -T %t/memory-ok.ld -e _start %t/base.o -o %t/memory-ok
# RUN: not ld.lld -T %t/memory-overflow.ld -e _start %t/base.o \
# RUN:   -o /dev/null 2>&1 | FileCheck %s --check-prefix=REGION
# REGION: error: section '.c166.near.data' will not fit in region 'DPP2': overflowed by 1 bytes

## A script can spell a 32-bit VMA, but every C166 address-bearing relocation
## must reject the first byte beyond the architectural 24-bit address space.
# RUN: not ld.lld -T %t/overflow.ld -e overflow_start %t/overflow.o \
# RUN:   -o /dev/null 2>&1 | FileCheck %s --check-prefix=ADDRESS
# ADDRESS: error: {{.*}}relocation R_C166_SEG8 out of range: 16777216 is not in [0, 16777215]

#--- base.s
.text
.globl _start
.type _start,@function
_start:
  mov r4, dpp2(near_object)
  mov r5, dpp1(xnear_object)
  extp pag(paged_object), #1
  mov r6, pof(paged_object)
  rets

.section .c166.near.data,"aw",@progbits
.globl near_object
.type near_object,@object
near_object:
  .space 8
.size near_object, .-near_object

.section .c166.xnear.data,"aw",@progbits
.globl xnear_object
.type xnear_object,@object
xnear_object:
  .space 8
.size xnear_object, .-xnear_object

.section .paged,"aw",@progbits
.globl paged_object
.type paged_object,@object
paged_object:
  .space 8
.size paged_object, .-paged_object

.section .near.tail,"aw",@progbits
  .space 8

#--- overflow.s
.text
.globl overflow_start
overflow_start:
seg_site:
  .byte 0
  .reloc seg_site, R_C166_SEG8, overflow_target

.section .overflow,"a",@progbits
.globl overflow_target
overflow_target:
  .short 0

#--- valid.ld
SECTIONS {
  .text 0x120000 : { *(.text) }
  .c166.near.data 0x203ff8 : { *(.c166.near.data) }
  .c166.xnear.data 0x403ff8 : { *(.c166.xnear.data) }
  .paged 0x603ff8 : { *(.paged) }
  .near.tail 0x700000 : { *(.near.tail) }
}

#--- overlap.ld
SECTIONS {
  .text 0x120000 : { *(.text) }
  .c166.near.data 0x201000 : { *(.c166.near.data) }
  .c166.xnear.data 0x201004 : { *(.c166.xnear.data) }
  .paged 0x603000 : { *(.paged) }
  .near.tail 0x700000 : { *(.near.tail) }
}

#--- memory-ok.ld
MEMORY {
  ROM (rx) : ORIGIN = 0x120000, LENGTH = 0x100
  DPP2 (rw) : ORIGIN = 0x203ff0, LENGTH = 0x10
  DPP1 (rw) : ORIGIN = 0x403ff8, LENGTH = 0x8
  PAGED (rw) : ORIGIN = 0x603ff8, LENGTH = 0x8
}
SECTIONS {
  .text : { *(.text) } > ROM
  .c166.near.data : { *(.c166.near.data) *(.near.tail) } > DPP2
  .c166.xnear.data : { *(.c166.xnear.data) } > DPP1
  .paged : { *(.paged) } > PAGED
}

#--- memory-overflow.ld
MEMORY {
  ROM (rx) : ORIGIN = 0x120000, LENGTH = 0x100
  DPP2 (rw) : ORIGIN = 0x203ff0, LENGTH = 0x0f
  DPP1 (rw) : ORIGIN = 0x403ff8, LENGTH = 0x8
  PAGED (rw) : ORIGIN = 0x603ff8, LENGTH = 0x8
}
SECTIONS {
  .text : { *(.text) } > ROM
  .c166.near.data : { *(.c166.near.data) *(.near.tail) } > DPP2
  .c166.xnear.data : { *(.c166.xnear.data) } > DPP1
  .paged : { *(.paged) } > PAGED
}

#--- overflow.ld
SECTIONS {
  .text 0x120000 : { *(.text) }
  .overflow 0x1000000 : { *(.overflow) }
}
