; REQUIRES: c166
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %s -o %t.o
; RUN: llvm-mc -filetype=obj -triple=c166-none-elf %S/Inputs/c166-target.s -o %t.target.o
; RUN: ld.lld -Ttext=0x12340 -e _start %t.o %t.target.o -o %t
; RUN: llvm-readobj --file-header --symbols %t | FileCheck %s --check-prefix=ELF
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
; RUN: not ld.lld -Ttext=0x12340 -e _start %t.o -o %t.undef 2>&1 | FileCheck %s --check-prefix=UNDEF
; RUN: not ld.lld -Ttext=0x1000000 -e _start %t.o %t.target.o -o %t.overflow 2>&1 | FileCheck %s --check-prefix=OVERFLOW

.globl _start
.type _start,@function
_start:
  calls seg(_target), sof(_target)

; ELF:      Class: 32-bit
; ELF:      DataEncoding: LittleEndian
; ELF:      OS/ABI: Standalone
; ELF:      Type: Executable
; ELF:      Machine: EM_C166 (0x74)
; ELF:      Flags [ (0x121)
; ELF-DAG:    EF_C166_CODE_HUGE (0x100)
; ELF-DAG:    EF_C166_CORE_8X166 (0x1)
; ELF-DAG:    EF_C166_DATA_FAR (0x20)
; ELF-DAG:  Name: _start
; ELF-DAG:  Value: 0x12340
; ELF-DAG:  Name: _target
; ELF-DAG:  Value: 0x12344

; DIS-LABEL: <_start>:
; DIS-NEXT:  12340: da 01 44 23  calls 1, 9028
; DIS-LABEL: <_target>:
; DIS-NEXT:  12344: db 00        rets

; UNDEF: error: undefined symbol: _target
; OVERFLOW: error: {{.*}}relocation R_C166_SEG24 out of range: 16777220 is not in [0, 16777215]
