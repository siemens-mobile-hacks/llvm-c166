; RUN: llc -mtriple=c166-none-elf -code-model=large < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=large -filetype=obj < %s \
; RUN:   -o %t.large.o
; RUN: llc -mtriple=c166-none-elf -code-model=medium -filetype=obj < %s \
; RUN:   -o %t.medium.o
; RUN: llc -mtriple=c166-none-elf -code-model=small -filetype=obj < %s \
; RUN:   -o %t.small.o
; RUN: llvm-readobj --file-header %t.large.o \
; RUN:   | FileCheck %s --check-prefixes=OBJ,LARGE-OBJ
; RUN: llvm-readobj --file-header %t.medium.o \
; RUN:   | FileCheck %s --check-prefixes=OBJ,MEDIUM-OBJ
; RUN: llvm-readobj --file-header %t.small.o \
; RUN:   | FileCheck %s --check-prefixes=OBJ,SMALL-OBJ
; RUN: llvm-readobj --relocations %t.large.o \
; RUN:   | FileCheck %s --check-prefix=HUGE-RELOC
; RUN: llvm-readobj --relocations %t.medium.o \
; RUN:   | FileCheck %s --check-prefix=NEAR-RELOC
; RUN: llvm-readobj --relocations %t.small.o \
; RUN:   | FileCheck %s --check-prefix=HUGE-RELOC
; RUN: llc -mtriple=c166-none-elf -code-model=large \
; RUN:   -stop-before=c166-asm-printer < %s \
; RUN:   | FileCheck %s --check-prefixes=MIR,HUGE-MIR
; RUN: llc -mtriple=c166-none-elf -code-model=medium \
; RUN:   -stop-before=c166-asm-printer < %s \
; RUN:   | FileCheck %s --check-prefixes=MIR,NEAR-MIR
; RUN: llc -mtriple=c166-none-elf -code-model=small \
; RUN:   -stop-before=c166-asm-printer < %s \
; RUN:   | FileCheck %s --check-prefixes=MIR,HUGE-MIR
; C166-ABI: calls.direct
; C166-ABI: args.first_four_words
; C166-ABI: args.packed_two_word
; C166-ABI: args.stack_stop
; C166-ABI: args.stack_word_order
; C166-ABI: args.packed_byval_padding
; C166-ABI: calls.stack_stop_push_order
; C166-ABI: returns.byte
; C166-ABI: returns.word
; C166-ABI: returns.long_word_order
;
; C166 passes argument words through R12-R15 without imposing even-register
; alignment on a two-word argument. Scalar return values use R4.
;
; OBJ:      Format: elf32-c166
; OBJ:      Machine: EM_C166 (0x74)
; LARGE-OBJ:  Flags [ (0x121)
; LARGE-OBJ-DAG: EF_C166_CODE_HUGE (0x100)
; LARGE-OBJ-DAG: EF_C166_CORE_8X166 (0x1)
; LARGE-OBJ-DAG: EF_C166_DATA_FAR (0x20)
; MEDIUM-OBJ: Flags [ (0x221)
; MEDIUM-OBJ-DAG: EF_C166_CODE_NEAR (0x200)
; MEDIUM-OBJ-DAG: EF_C166_CORE_8X166 (0x1)
; MEDIUM-OBJ-DAG: EF_C166_DATA_FAR (0x20)
; SMALL-OBJ:  Flags [ (0x111)
; SMALL-OBJ-DAG: EF_C166_CODE_HUGE (0x100)
; SMALL-OBJ-DAG: EF_C166_CORE_8X166 (0x1)
; SMALL-OBJ-DAG: EF_C166_DATA_NEAR (0x10)
; HUGE-RELOC:      R_C166_SEG8 _callee_mix
; HUGE-RELOC-NEXT: R_C166_SOF16 _callee_mix
; NEAR-RELOC:      R_C166_COF16 _callee_mix
; MIR-LABEL:  name: forward_mix
; HUGE-MIR:   CALLS @callee_mix, @callee_mix, $r12, $r14r13, $r15, csr_c166_callpreserved
; NEAR-MIR:   CALLA @callee_mix, $r12, $r14r13, $r15, csr_c166_callpreserved
; MIR-NOT:    implicit-def $r0
; MIR-LABEL:  name: reorder_mix

define i16 @return_three() {
; CHECK-LABEL: _return_three:
; CHECK:       mov r4, #3
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  ret i16 3
}

define i16 @identity_word(i16 %value) {
; CHECK-LABEL: _identity_word:
; CHECK:       mov r4, r12
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  ret i16 %value
}

define i16 @add_words(i16 %left, i16 %right) {
; CHECK-LABEL: _add_words:
; CHECK:       mov r4, r12
; CHECK-NEXT:  add r4, r13
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %sum = add i16 %left, %right
  ret i16 %sum
}

define i16 @word_after_unaligned_dword(i16 %first, i32 %second, i16 %third) {
; CHECK-LABEL: _word_after_unaligned_dword:
; CHECK:       mov r4, r15
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  ret i16 %third
}

define i8 @identity_byte(i8 %value) {
; CHECK-LABEL: _identity_byte:
; CHECK:       mov r4, r12
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  ret i8 %value
}

define i8 @return_byte_three() {
; CHECK-LABEL: _return_byte_three:
; CHECK:       mov r4, #3
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  ret i8 3
}

define i8 @add_bytes(i8 %left, i8 %right) {
; CHECK-LABEL: _add_bytes:
; CHECK:       mov r4, r12
; CHECK-NEXT:  add r4, r13
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %sum = add i8 %left, %right
  ret i8 %sum
}

define i32 @identity_dword(i32 %value) {
; CHECK-LABEL: _identity_dword:
; CHECK:       mov r4, r12
; CHECK-NEXT:  mov r5, r13
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  ret i32 %value
}

define i32 @add_dword(i32 %lhs, i32 %rhs) {
; CHECK-LABEL: _add_dword:
; CHECK:       mov r4, r12
; CHECK-NEXT:  mov r5, r13
; CHECK-NEXT:  add r4, r14
; CHECK-NEXT:  addc r5, r15
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %sum = add i32 %lhs, %rhs
  ret i32 %sum
}

define i32 @add_word_to_dword(i32 %lhs, i16 %rhs) {
; CHECK-LABEL: _add_word_to_dword:
; CHECK:       mov r4, r12
; CHECK-NEXT:  mov r5, r13
; CHECK-NEXT:  mov [[RHS:r[0-9]+]], r14
; CHECK-NEXT:  mov [[ZERO:r[0-9]+]], #0
; CHECK-NEXT:  add r4, [[RHS]]
; CHECK-NEXT:  addc r5, [[ZERO]]
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %wide = zext i16 %rhs to i32
  %sum = add i32 %lhs, %wide
  ret i32 %sum
}

define i16 @load_far(ptr addrspace(2) %address) {
; CHECK-LABEL: _load_far:
; CHECK:       extp r13, #1
; CHECK-NEXT:  mov r4, [r12]
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %value = load i16, ptr addrspace(2) %address, align 2
  ret i16 %value
}

define void @store_far(ptr addrspace(2) %address, i16 %value) {
; CHECK-LABEL: _store_far:
; CHECK:       extp r13, #1
; CHECK-NEXT:  mov [r12], r14
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  store i16 %value, ptr addrspace(2) %address, align 2
  ret void
}

define i16 @fifth_word_on_stack(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) {
; CHECK-LABEL: _fifth_word_on_stack:
; CHECK:       mov r4, [r0]
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  ret i16 %e
}

define i16 @two_stack_words(i32 %a, i32 %b, i16 %c, i16 %d) {
; CHECK-LABEL: _two_stack_words:
; CHECK-DAG:   mov r4, [r0]
; CHECK-DAG:   mov [[TMP:r[0-9]+]], [r0 + #2]
; CHECK:       add r4, [[TMP]]
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %sum = add i16 %c, %d
  ret i16 %sum
}

define i32 @dword_forced_to_stack(i16 %a, i16 %b, i16 %c, i32 %d) {
; CHECK-LABEL: _dword_forced_to_stack:
; CHECK-DAG:   mov r4, [r0]
; CHECK-DAG:   mov r5, [r0 + #2]
; HUGE:        rets
; NEAR:        ret
  ret i32 %d
}

define i16 @stack_stop_after_dword(i16 %a, i16 %b, i16 %c, i32 %d, i16 %e) {
; CHECK-LABEL: _stack_stop_after_dword:
; CHECK-DAG:   mov r4, [r0]
; CHECK-DAG:   mov [[TMP:r[0-9]+]], [r0 + #4]
; CHECK:       add r4, [[TMP]]
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %low = trunc i32 %d to i16
  %sum = add i16 %low, %e
  ret i16 %sum
}

declare i32 @callee_mix(i16, i32, i16)

define i32 @forward_mix(i16 %a, i32 %b, i16 %c) {
; CHECK-LABEL: _forward_mix:
; HUGE:        calls seg(_callee_mix), sof(_callee_mix)
; NEAR:        calla cc_uc, cof(_callee_mix)
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %result = call i32 @callee_mix(i16 %a, i32 %b, i16 %c)
  ret i32 %result
}

define i32 @reorder_mix(i16 %a, i16 %b, i32 %c) {
; CHECK-LABEL: _reorder_mix:
; CHECK:       mov r1, r12
; CHECK-NEXT:  mov r12, r13
; CHECK-NEXT:  mov r13, r14
; CHECK-NEXT:  mov r14, r15
; CHECK-NEXT:  mov r15, r1
; HUGE-NEXT:   calls seg(_callee_mix), sof(_callee_mix)
; NEAR-NEXT:   calla cc_uc, cof(_callee_mix)
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %result = call i32 @callee_mix(i16 %b, i32 %c, i16 %a)
  ret i32 %result
}

declare i16 @callee_word(i16)

define i16 @forward_word(i16 %value) {
; CHECK-LABEL: _forward_word:
; HUGE:        calls seg(_callee_word), sof(_callee_word)
; NEAR:        calla cc_uc, cof(_callee_word)
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %result = call i16 @callee_word(i16 %value)
  ret i16 %result
}

declare i8 @callee_byte(i8)

define i8 @forward_byte(i8 %value) {
; CHECK-LABEL: _forward_byte:
; HUGE:        calls seg(_callee_byte), sof(_callee_byte)
; NEAR:        calla cc_uc, cof(_callee_byte)
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %result = call i8 @callee_byte(i8 %value)
  ret i8 %result
}

declare i16 @callee_five_words(i16, i16, i16, i16, i16)

define i16 @forward_five_words(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) {
; CHECK-LABEL: _forward_five_words:
; CHECK:       sub r0, #2
; CHECK-NEXT:  mov [[ARG:r[0-9]+]], [r0 + #2]
; CHECK-NEXT:  mov [r0], [[ARG]]
; HUGE-NEXT:   calls seg(_callee_five_words), sof(_callee_five_words)
; NEAR-NEXT:   calla cc_uc, cof(_callee_five_words)
; CHECK-NEXT:  add r0, #2
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %result = call i16 @callee_five_words(i16 %a, i16 %b, i16 %c, i16 %d,
                                         i16 %e)
  ret i16 %result
}

declare i16 @callee_stack_stop(i16, i16, i16, i32, i16)

define i16 @forward_stack_stop(i16 %a, i16 %b, i16 %c, i32 %d, i16 %e) {
; CHECK-LABEL: _forward_stack_stop:
; CHECK:       mov [[DHI:r[0-9]+]], [r0 + #2]
; CHECK-NEXT:  mov [[DLO:r[0-9]+]], [r0]
; CHECK-NEXT:  sub r0, #6
; CHECK-NEXT:  mov [r0], [[DLO]]
; CHECK-NEXT:  mov [r0 + #2], [[DHI]]
; CHECK-NEXT:  mov [[E:r[0-9]+]], [r0 + #10]
; CHECK-NEXT:  mov [r0 + #4], [[E]]
; HUGE-NEXT:   calls seg(_callee_stack_stop), sof(_callee_stack_stop)
; NEAR-NEXT:   calla cc_uc, cof(_callee_stack_stop)
; CHECK-NEXT:  add r0, #6
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %result = call i16 @callee_stack_stop(i16 %a, i16 %b, i16 %c, i32 %d,
                                         i16 %e)
  ret i16 %result
}

%packed3 = type <{ i8, i16 }>

declare i16 @callee_packed3(ptr addrspace(2) byval(%packed3) align 1, i16)

define i16 @forward_packed3(ptr addrspace(2) %value, i16 %tail) {
; A packed three-byte object may start at an odd address.  Form its first ABI
; word from two byte loads, form the second from the one byte which belongs to
; the object, and leave the high padding byte zero.  A word load here traps on
; C166 and the final such load would also read beyond the source object.
; CHECK-LABEL: _forward_packed3:
; CHECK:       movb [[HIGH_BYTE:r[lh][0-7]]], [{{r[0-9]+}}]
; CHECK-NEXT:  movbz [[HIGH_WORD:r[0-9]+]], [[HIGH_BYTE]]
; CHECK-NEXT:  shl [[HIGH_WORD]], #8
; CHECK:       movb [[LOW_BYTE:r[lh][0-7]]], [{{r[0-9]+}}]
; CHECK-NEXT:  movbz [[FIRST_WORD:r[0-9]+]], [[LOW_BYTE]]
; CHECK-NEXT:  or [[FIRST_WORD]], [[HIGH_WORD]]
; CHECK:       movb [[LAST_BYTE:r[lh][0-7]]], [{{r[0-9]+}}]
; CHECK-NEXT:  movbz [[LAST_WORD:r[0-9]+]], [[LAST_BYTE]]
; CHECK-NOT:   mov {{r[0-9]+}}, [{{r[0-9]+}}]
; CHECK:       sub r0, #6
; CHECK-DAG:   mov [r0], [[FIRST_WORD]]
; CHECK-DAG:   mov [r0 + #2], [[LAST_WORD]]
; CHECK-DAG:   mov [r0 + #4], r14
; HUGE:        calls seg(_callee_packed3), sof(_callee_packed3)
; NEAR:        calla cc_uc, cof(_callee_packed3)
; CHECK:       add r0, #6
; HUGE-NEXT:   rets
; NEAR-NEXT:   ret
  %result = call i16 @callee_packed3(
      ptr addrspace(2) byval(%packed3) align 1 %value, i16 %tail)
  ret i16 %result
}
