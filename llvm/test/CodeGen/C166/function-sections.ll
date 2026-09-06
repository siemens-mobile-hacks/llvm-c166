; REQUIRES: c166-registered-target
; RUN: llc -mtriple=c166-none-elf -code-model=medium -function-sections \
; RUN:   -filetype=obj -o %t.medium.o %s
; RUN: llvm-readobj --sections %t.medium.o | FileCheck %s --check-prefix=MEDIUM
; RUN: llc -mtriple=c166-none-elf -code-model=large -function-sections \
; RUN:   -filetype=obj -o %t.large.o %s
; RUN: llvm-readobj --sections %t.large.o | FileCheck %s --check-prefix=BANK

target triple = "c166-none-elf"

define void @near_one() addrspace(3) {
  ret void
}

define void @near_two() addrspace(3) {
  ret void
}

define void @bank_one() addrspace(257) {
  ret void
}

define void @bank_two() addrspace(257) {
  ret void
}

; MEDIUM-COUNT-2: Name: .c166.near.text (

; BANK-COUNT-2: Name: .text.c166.bank.1 (
