; REQUIRES: c166-registered-target
; C166-ABI: calls.code_banks
; C166-ABI: runtime.code_bank_switch_contract
; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs \
; RUN:   -o - %s | FileCheck %s --check-prefix=ASM
; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs \
; RUN:   -filetype=obj -o %t.o %s
; RUN: llvm-readobj --sections %t.o | FileCheck %s --check-prefix=OBJ

target datalayout = "e-m:u-P1-G2-A2-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-p4:16:16-p5:32:16:16:32-p6:32:16:16:32-i32:16-i64:16-f32:16-f64:16-a:0:16-n8:16-S16-ni:2"
target triple = "c166-none-elf"

declare i16 @bank1_target(i16, i16, i16, i16, i16) addrspace(257)
declare i16 @bank2_target(i16, i16, i16, i16, i16) addrspace(258)
declare i16 @plain_target(i16, i16, i16, i16, i16) addrspace(1)

; A banked callee receives the hidden bank word at [R0], so its first normal
; stack argument starts at [R0 + 2].
define i16 @bank1_fifth(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) addrspace(257) {
; ASM:      .section .text.c166.bank.1
; ASM-LABEL: _bank1_fifth:
; ASM:      mov r4, [r0 + #2]
; ASM-NEXT: rets
  ret i16 %e
}

; Address space 256 + N is the IR representation of code bank N.  Cover the
; upper valid bank as well as bank 1.
define i16 @bank255_identity(i16 %value) addrspace(511) {
; ASM:      .section .text.c166.bank.255
; ASM-LABEL: _bank255_identity:
  ret i16 %value
}

define i16 @bank1_same(i16 %value) addrspace(257) {
; ASM:      .section .text.c166.bank.1
; ASM-LABEL: _bank1_same:
; ASM:      sub r0, #4
; ASM:      mov [r0 + #2],
; ASM:      calls seg(_bank1_target), sof(_bank1_target)
; ASM:      add r0, #4
  %result = call addrspace(257) i16 @bank1_target(i16 %value, i16 2, i16 3,
                                                 i16 4, i16 5)
  ret i16 %result
}

define i16 @bank1_cross(i16 %value) addrspace(257) {
; ASM-LABEL: _bank1_cross:
; ASM:      sub r0, #4
; ASM:      mov [r0 + #2],
; ASM:      mov r3, #258
; ASM-NEXT: mov [r0], r3
; ASM:      mov r4, #sof(_bank2_target)
; ASM-NEXT: mov r5, #seg(_bank2_target)
; ASM:      calls seg(__banksw), sof(__banksw)
; ASM:      add r0, #4
  %result = call addrspace(258) i16 @bank2_target(i16 %value, i16 2, i16 3,
                                                 i16 4, i16 5)
  ret i16 %result
}

define i16 @unbanked_to_one(i16 %value) addrspace(1) {
; ASM:      .text
; ASM-LABEL: _unbanked_to_one:
; ASM:      mov r3, #1
; ASM-NEXT: mov [r0], r3
; ASM:      mov r4, #sof(_bank1_target)
; ASM-NEXT: mov r5, #seg(_bank1_target)
; ASM:      calls seg(__banksw), sof(__banksw)
  %result = call addrspace(257) i16 @bank1_target(i16 %value, i16 2, i16 3,
                                                 i16 4, i16 5)
  ret i16 %result
}

define i16 @bank1_indirect_same(ptr addrspace(257) %target,
                                i16 %value) addrspace(257) {
; ASM:      .section .text.c166.bank.1
; ASM-LABEL: _bank1_indirect_same:
; ASM:      sub r0, #4
; ASM:      mov [r0 + #2],
; ASM:      calls seg(__icall), sof(__icall)
; ASM:      add r0, #4
  %result = call addrspace(257) i16 %target(i16 %value, i16 2, i16 3, i16 4,
                                           i16 5)
  ret i16 %result
}

define i16 @bank1_indirect_cross(ptr addrspace(258) %target,
                                 i16 %value) addrspace(257) {
; ASM-LABEL: _bank1_indirect_cross:
; ASM:      sub r0, #4
; ASM:      mov [r0 + #2],
; ASM:      mov r3, #258
; ASM-NEXT: mov [r0], r3
; ASM:      calls seg(__banksw), sof(__banksw)
; ASM:      add r0, #4
  %result = call addrspace(258) i16 %target(i16 %value, i16 2, i16 3, i16 4,
                                           i16 5)
  ret i16 %result
}

define i16 @bank1_to_plain(i16 %value) addrspace(257) {
; ASM-LABEL: _bank1_to_plain:
; ASM:      sub r0, #2
; ASM:      mov [r0],
; ASM:      calls seg(_plain_target), sof(_plain_target)
; ASM:      add r0, #2
  %result = call addrspace(1) i16 @plain_target(i16 %value, i16 2, i16 3,
                                               i16 4, i16 5)
  ret i16 %result
}

; OBJ: Name: .text.c166.bank.1
; OBJ: Name: .text.c166.bank.255
