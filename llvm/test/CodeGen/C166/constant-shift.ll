; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=large -stop-after=finalize-isel \
; RUN:   < %s | FileCheck %s --check-prefix=ISEL

define i32 @shl1(i32 %value) {
; CHECK-LABEL: shl1:
; CHECK-NOT:   calls
; CHECK:       shl {{r[0-9]+}}, #1
; CHECK-NEXT:  addc {{r[0-9]+}}, {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = shl i32 %value, 1
  ret i32 %result
}

define i32 @shl2(i32 %value) {
; CHECK-LABEL: shl2:
; CHECK-NOT:   calls
; CHECK:       shl {{r[0-9]+}}, #1
; CHECK-NEXT:  addc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  shl {{r[0-9]+}}, #1
; CHECK-NEXT:  addc {{r[0-9]+}}, {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = shl i32 %value, 2
  ret i32 %result
}

define i32 @shl3(i32 %value) {
; ISEL-LABEL: name: shl3
; ISEL:       SHL32ri5
; CHECK-LABEL: shl3:
; CHECK-NOT:   calls
; CHECK:       shl [[LOW:r[0-9]+]], #3
; CHECK:       shr {{r[0-9]+}}, #13
; CHECK:       shl [[HIGH:r[0-9]+]], #3
; CHECK:       or [[HIGH]], {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = shl i32 %value, 3
  ret i32 %result
}

define i32 @lshr3(i32 %value) {
; ISEL-LABEL: name: lshr3
; ISEL:       SRL32ri5
; CHECK-LABEL: lshr3:
; CHECK-NOT:   calls
; CHECK:       shr [[LOW:r[0-9]+]], #3
; CHECK:       shl {{r[0-9]+}}, #13
; CHECK:       shr [[HIGH:r[0-9]+]], #3
; CHECK:       or [[LOW]], {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = lshr i32 %value, 3
  ret i32 %result
}

define i32 @ashr3(i32 %value) {
; ISEL-LABEL: name: ashr3
; ISEL:       SRA32ri5
; CHECK-LABEL: ashr3:
; CHECK-NOT:   calls
; CHECK:       shr [[LOW:r[0-9]+]], #3
; CHECK:       shl {{r[0-9]+}}, #13
; CHECK:       ashr [[HIGH:r[0-9]+]], #3
; CHECK:       or [[LOW]], {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = ashr i32 %value, 3
  ret i32 %result
}

define i32 @shl15(i32 %value) {
; ISEL-LABEL: name: shl15
; ISEL:       SHL32ri5
; CHECK-LABEL: shl15:
; CHECK-NOT:   calls
; CHECK:       shl [[LOW:r[0-9]+]], #15
; CHECK:       shr {{r[0-9]+}}, #1
; CHECK:       shl [[HIGH:r[0-9]+]], #15
; CHECK:       or [[HIGH]], {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = shl i32 %value, 15
  ret i32 %result
}

define i32 @lshr15(i32 %value) {
; ISEL-LABEL: name: lshr15
; ISEL:       SRL32ri5
; CHECK-LABEL: lshr15:
; CHECK-NOT:   calls
; CHECK:       shr [[LOW:r[0-9]+]], #15
; CHECK:       shl {{r[0-9]+}}, #1
; CHECK:       shr [[HIGH:r[0-9]+]], #15
; CHECK:       or [[LOW]], {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = lshr i32 %value, 15
  ret i32 %result
}

define i32 @ashr15(i32 %value) {
; ISEL-LABEL: name: ashr15
; ISEL:       SRA32ri5
; CHECK-LABEL: ashr15:
; CHECK-NOT:   calls
; CHECK:       shr [[LOW:r[0-9]+]], #15
; CHECK:       shl {{r[0-9]+}}, #1
; CHECK:       ashr [[HIGH:r[0-9]+]], #15
; CHECK:       or [[LOW]], {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = ashr i32 %value, 15
  ret i32 %result
}

define i32 @shl_variable(i32 %value, i16 %amount) {
; CHECK-LABEL: shl_variable:
; CHECK:       ___ashlsi3
  %wide = zext i16 %amount to i32
  %result = shl i32 %value, %wide
  ret i32 %result
}

define i32 @lshr_variable(i32 %value, i16 %amount) {
; CHECK-LABEL: lshr_variable:
; CHECK:       ___lshrsi3
  %wide = zext i16 %amount to i32
  %result = lshr i32 %value, %wide
  ret i32 %result
}

define i32 @ashr_variable(i32 %value, i16 %amount) {
; CHECK-LABEL: ashr_variable:
; CHECK:       ___ashrsi3
  %wide = zext i16 %amount to i32
  %result = ashr i32 %value, %wide
  ret i32 %result
}

define i16 @shl_masked_i16(i16 %value, i16 %amount) {
; CHECK-LABEL: shl_masked_i16:
; CHECK-NOT:   and
; CHECK:       shl {{r[0-9]+}}, {{r[0-9]+}}
  %masked = and i16 %amount, 15
  %result = shl i16 %value, %masked
  ret i16 %result
}

define i16 @lshr_masked_i16(i16 %value, i16 %amount) {
; CHECK-LABEL: lshr_masked_i16:
; CHECK-NOT:   and
; CHECK:       shr {{r[0-9]+}}, {{r[0-9]+}}
  %masked = and i16 %amount, 15
  %result = lshr i16 %value, %masked
  ret i16 %result
}

define i16 @ashr_masked_i16(i16 %value, i16 %amount) {
; CHECK-LABEL: ashr_masked_i16:
; CHECK-NOT:   and
; CHECK:       ashr {{r[0-9]+}}, {{r[0-9]+}}
  %masked = and i16 %amount, 15
  %result = ashr i16 %value, %masked
  ret i16 %result
}

define i16 @shl_masked_i16_shared(i16 %value, i16 %amount,
                                  ptr addrspace(1) %masked_out) {
; CHECK-LABEL: shl_masked_i16_shared:
; CHECK-DAG:   and [[MASKED:r[0-9]+]], #15
; CHECK-DAG:   shl {{r[0-9]+}}, {{r[0-9]+}}
; CHECK:       mov [{{r[0-9]+}}], [[MASKED]]
  %masked = and i16 %amount, 15
  %result = shl i16 %value, %masked
  store i16 %masked, ptr addrspace(1) %masked_out, align 2
  ret i16 %result
}

define i16 @shl_masked_i16_wrong_mask(i16 %value, i16 %amount) {
; CHECK-LABEL: shl_masked_i16_wrong_mask:
; CHECK:       and [[MASKED:r[0-9]+]], #7
; CHECK:       shl {{r[0-9]+}}, [[MASKED]]
  %masked = and i16 %amount, 7
  %result = shl i16 %value, %masked
  ret i16 %result
}

define i16 @lshr_low_field(i16 %value) {
; CHECK-LABEL: lshr_low_field:
; CHECK-NOT:   and
; CHECK:       shl [[FIELD:r[0-9]+]], #6
; CHECK-NEXT:  shr [[FIELD]], #9
  %shift = lshr i16 %value, 3
  %result = and i16 %shift, 127
  ret i16 %result
}

define i16 @lshr_middle_field(i16 %value) {
; CHECK-LABEL: lshr_middle_field:
; CHECK-NOT:   and
; CHECK:       shl [[FIELD:r[0-9]+]], #2
; CHECK-NEXT:  shr [[FIELD]], #7
  %shift = lshr i16 %value, 5
  %result = and i16 %shift, 511
  ret i16 %result
}

define i16 @lshr_field_shared_shift(i16 %value,
                                    ptr addrspace(1) %shift_out) {
; CHECK-LABEL: lshr_field_shared_shift:
; CHECK:       shr [[SHIFT:r[0-9]+]], #3
; CHECK:       mov [{{r[0-9]+}}], [[SHIFT]]
; CHECK-NEXT:  and [[SHIFT]], #127
  %shift = lshr i16 %value, 3
  %result = and i16 %shift, 127
  store i16 %shift, ptr addrspace(1) %shift_out, align 2
  ret i16 %result
}

define i16 @lshr_field_short_mask(i16 %value) {
; CHECK-LABEL: lshr_field_short_mask:
; CHECK:       shr [[FIELD:r[0-9]+]], #3
; CHECK-NEXT:  and [[FIELD]], #7
  %shift = lshr i16 %value, 3
  %result = and i16 %shift, 7
  ret i16 %result
}

define i16 @lshr_field_byte_mask(i16 %value) {
; CHECK-LABEL: lshr_field_byte_mask:
; CHECK:       shr [[FIELD:r[0-9]+]], #3
; CHECK-NEXT:  movbz {{r[0-9]+}}, rl{{[0-7]}}
  %shift = lshr i16 %value, 3
  %result = and i16 %shift, 255
  ret i16 %result
}

define i32 @lshr1(i32 %value) {
; ISEL-LABEL: name: lshr1
; ISEL:       SRL32ri1
; CHECK-LABEL: lshr1:
; CHECK-NOT:   calls
; CHECK:       shr [[LOW:r[0-9]+]], #1
; CHECK-NEXT:  bmov [[LOW]].15, [[HIGH:r[0-9]+]].0
; CHECK-NEXT:  shr [[HIGH]], #1
; HUGE:        rets
; NEAR:        ret
  %result = lshr i32 %value, 1
  ret i32 %result
}

define i32 @ashr1(i32 %value) {
; ISEL-LABEL: name: ashr1
; ISEL:       SRA32ri1
; CHECK-LABEL: ashr1:
; CHECK-NOT:   calls
; CHECK:       shr [[LOW:r[0-9]+]], #1
; CHECK-NEXT:  bmov [[LOW]].15, [[HIGH:r[0-9]+]].0
; CHECK-NEXT:  ashr [[HIGH]], #1
; HUGE:        rets
; NEAR:        ret
  %result = ashr i32 %value, 1
  ret i32 %result
}

define void @lshr_pair(ptr addrspace(1) %low_out,
                       ptr addrspace(1) %high_out, i16 %low, i16 %high) {
; ISEL-LABEL: name: lshr_pair
; ISEL:       SRLPAIRri1
; CHECK-LABEL: lshr_pair:
; CHECK:       shr [[LOW:r[0-9]+]], #1
; CHECK-NEXT:  bmov [[LOW]].15, [[HIGH:r[0-9]+]].0
; CHECK-NEXT:  shr [[HIGH]], #1
  %low_shift = lshr i16 %low, 1
  %carry = shl i16 %high, 15
  %new_low = or i16 %low_shift, %carry
  %new_high = lshr i16 %high, 1
  store i16 %new_low, ptr addrspace(1) %low_out, align 2
  store i16 %new_high, ptr addrspace(1) %high_out, align 2
  ret void
}

define void @lshr_pair_fshl(ptr addrspace(1) %low_out,
                            ptr addrspace(1) %high_out, i16 %low, i16 %high) {
; ISEL-LABEL: name: lshr_pair_fshl
; ISEL:       SRLPAIRri1
; CHECK-LABEL: lshr_pair_fshl:
; CHECK:       shr [[LOW:r[0-9]+]], #1
; CHECK-NEXT:  bmov [[LOW]].15, [[HIGH:r[0-9]+]].0
; CHECK-NEXT:  shr [[HIGH]], #1
  %new_low = call i16 @llvm.fshl.i16(i16 %high, i16 %low, i16 15)
  %new_high = lshr i16 %high, 1
  store i16 %new_low, ptr addrspace(1) %low_out, align 2
  store i16 %new_high, ptr addrspace(1) %high_out, align 2
  ret void
}

define void @ashr_pair_with_sticky(ptr addrspace(1) %low_out,
                                   ptr addrspace(1) %high_out,
                                   ptr addrspace(1) %sticky_out, i16 %low,
                                   i16 %high) {
; ISEL-LABEL: name: ashr_pair_with_sticky
; ISEL:       SRAPAIRri1
; CHECK-LABEL: ashr_pair_with_sticky:
; CHECK:       shr [[LOW:r[0-9]+]], #1
; CHECK-NEXT:  bmov [[LOW]].15, [[HIGH:r[0-9]+]].0
; CHECK-NEXT:  ashr [[HIGH]], #1
  %sticky = and i16 %low, 1
  %low_shift = lshr i16 %low, 1
  %carry = shl i16 %high, 15
  %new_low = or i16 %low_shift, %carry
  %new_high = ashr i16 %high, 1
  store i16 %new_low, ptr addrspace(1) %low_out, align 2
  store i16 %new_high, ptr addrspace(1) %high_out, align 2
  store i16 %sticky, ptr addrspace(1) %sticky_out, align 2
  ret void
}

define void @lshr_three_words(ptr addrspace(1) %word0_out,
                              ptr addrspace(1) %word1_out,
                              ptr addrspace(1) %word2_out, i16 %word0,
                              i16 %word1, i16 %word2) {
; ISEL-LABEL: name: lshr_three_words
; ISEL-NOT:   SRLPAIRri1
  %new_word0 = call i16 @llvm.fshl.i16(i16 %word1, i16 %word0, i16 15)
  %new_word1 = call i16 @llvm.fshl.i16(i16 %word2, i16 %word1, i16 15)
  %new_word2 = lshr i16 %word2, 1
  store i16 %new_word0, ptr addrspace(1) %word0_out, align 2
  store i16 %new_word1, ptr addrspace(1) %word1_out, align 2
  store i16 %new_word2, ptr addrspace(1) %word2_out, align 2
  ret void
}

declare i16 @llvm.fshl.i16(i16, i16, i16)

define i32 @shl20(i32 %value) {
; CHECK-LABEL: shl20:
; CHECK-NOT:   calls
; CHECK:       shl r12, #4
; CHECK-NEXT:  mov r4, #0
; CHECK-NEXT:  mov r5, r12
; HUGE:        rets
; NEAR:        ret
  %result = shl i32 %value, 20
  ret i32 %result
}

define i32 @lshr20(i32 %value) {
; CHECK-LABEL: lshr20:
; CHECK-NOT:   calls
; CHECK:       shr r13, #4
; CHECK-NEXT:  mov r5, #0
; CHECK-NEXT:  mov r4, r13
; HUGE:        rets
; NEAR:        ret
  %result = lshr i32 %value, 20
  ret i32 %result
}

define i32 @ashr20(i32 %value) {
; CHECK-LABEL: ashr20:
; CHECK-NOT:   calls
; CHECK:       mov r5, r13
; CHECK-NEXT:  ashr r5, #15
; CHECK-NEXT:  ashr r13, #4
; CHECK-NEXT:  mov r4, r13
; HUGE:        rets
; NEAR:        ret
  %result = ashr i32 %value, 20
  ret i32 %result
}

define i16 @lshr23_low(i32 %value) {
; CHECK-LABEL: lshr23_low:
; CHECK-NOT:   calls
; CHECK:       mov r4, r13
; CHECK-NEXT:  shr r4, #7
; HUGE:        rets
; NEAR:        ret
  %shift = lshr i32 %value, 23
  %result = trunc i32 %shift to i16
  ret i16 %result
}

define i16 @ashr23_low(i32 %value) {
; CHECK-LABEL: ashr23_low:
; CHECK-NOT:   calls
; CHECK:       mov r4, r13
; CHECK-NEXT:  ashr r4, #7
; HUGE:        rets
; NEAR:        ret
  %shift = ashr i32 %value, 23
  %result = trunc i32 %shift to i16
  ret i16 %result
}

define i32 @add_lshr16(i32 %left, i32 %right) {
; ISEL-LABEL: name: add_lshr16
; ISEL:       ZEXT16_32
; CHECK-LABEL: add_lshr16:
; CHECK-NOT:   calls
; CHECK:       mov r4, r13
; CHECK-NEXT:  mov r5, #0
; CHECK-NEXT:  add r4, r14
; CHECK-NEXT:  addc r5, r15
; HUGE:        rets
; NEAR:        ret
  %shift = lshr i32 %left, 16
  %result = add i32 %shift, %right
  ret i32 %result
}

define i64 @shl64_1(i64 %value) {
; CHECK-LABEL: shl64_1:
; CHECK-NOT:   calls
; CHECK:       add {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  addc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  addc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  addc {{r[0-9]+}}, {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = shl i64 %value, 1
  ret i64 %result
}

define i64 @lshr64_1(i64 %value) {
; ISEL-LABEL: name: lshr64_1
; ISEL:       SRL64ri1
; CHECK-LABEL: lshr64_1:
; CHECK-NOT:   calls
; CHECK:       shr [[WORD0:r[0-9]+]], #1
; CHECK-NEXT:  bmov [[WORD0]].15, [[WORD1:r[0-9]+]].0
; CHECK-NEXT:  shr [[WORD1]], #1
; CHECK-NEXT:  bmov [[WORD1]].15, [[WORD2:r[0-9]+]].0
; CHECK-NEXT:  shr [[WORD2]], #1
; CHECK-NEXT:  bmov [[WORD2]].15, [[WORD3:r[0-9]+]].0
; CHECK-NEXT:  shr [[WORD3]], #1
; HUGE:        rets
; NEAR:        ret
  %result = lshr i64 %value, 1
  ret i64 %result
}

define i64 @ashr64_1(i64 %value) {
; ISEL-LABEL: name: ashr64_1
; ISEL:       SRA64ri1
; CHECK-LABEL: ashr64_1:
; CHECK-NOT:   calls
; CHECK:       shr [[WORD0:r[0-9]+]], #1
; CHECK-NEXT:  bmov [[WORD0]].15, [[WORD1:r[0-9]+]].0
; CHECK-NEXT:  shr [[WORD1]], #1
; CHECK-NEXT:  bmov [[WORD1]].15, [[WORD2:r[0-9]+]].0
; CHECK-NEXT:  shr [[WORD2]], #1
; CHECK-NEXT:  bmov [[WORD2]].15, [[WORD3:r[0-9]+]].0
; CHECK-NEXT:  ashr [[WORD3]], #1
; HUGE:        rets
; NEAR:        ret
  %result = ashr i64 %value, 1
  ret i64 %result
}

define i32 @lshr64_1_low(i64 %value) {
; ISEL-LABEL: name: lshr64_1_low
; ISEL:       SRL64ri1
; CHECK-LABEL: lshr64_1_low:
; CHECK-NOT:   calls
; CHECK:       shr [[WORD0:r[0-9]+]], #1
; CHECK-NEXT:  bmov [[WORD0]].15, [[WORD1:r[0-9]+]].0
; CHECK-NEXT:  shr [[WORD1]], #1
; CHECK-NEXT:  bmov [[WORD1]].15, {{r[0-9]+}}.0
; CHECK-NOT:   shr
; HUGE:        rets
; NEAR:        ret
  %shift = lshr i64 %value, 1
  %result = trunc i64 %shift to i32
  ret i32 %result
}

define i64 @shl64_2(i64 %value) {
; CHECK-LABEL: shl64_2:
; CHECK-NOT:   calls
; CHECK:       shl [[WORD0:r[0-9]+]], #1
; CHECK-NEXT:  addc [[WORD1:r[0-9]+]], [[WORD1]]
; CHECK-NEXT:  addc [[WORD2:r[0-9]+]], [[WORD2]]
; CHECK-NEXT:  addc [[WORD3:r[0-9]+]], [[WORD3]]
; CHECK-NEXT:  shl [[WORD0]], #1
; CHECK-NEXT:  addc [[WORD1]], [[WORD1]]
; CHECK-NEXT:  addc [[WORD2]], [[WORD2]]
; CHECK-NEXT:  addc [[WORD3]], [[WORD3]]
; HUGE:        rets
; NEAR:        ret
  %result = shl i64 %value, 2
  ret i64 %result
}

define i64 @shl64_3(i64 %value) {
; CHECK-LABEL: shl64_3:
; CHECK-NOT:   calls
; CHECK:       shl [[WORD0:r[0-9]+]], #1
; CHECK-NEXT:  addc [[WORD1:r[0-9]+]], [[WORD1]]
; CHECK-NEXT:  addc [[WORD2:r[0-9]+]], [[WORD2]]
; CHECK-NEXT:  addc [[WORD3:r[0-9]+]], [[WORD3]]
; CHECK-NEXT:  shl [[WORD0]], #1
; CHECK-NEXT:  addc [[WORD1]], [[WORD1]]
; CHECK-NEXT:  addc [[WORD2]], [[WORD2]]
; CHECK-NEXT:  addc [[WORD3]], [[WORD3]]
; CHECK-NEXT:  shl [[WORD0]], #1
; CHECK-NEXT:  addc [[WORD1]], [[WORD1]]
; CHECK-NEXT:  addc [[WORD2]], [[WORD2]]
; CHECK-NEXT:  addc [[WORD3]], [[WORD3]]
; HUGE:        rets
; NEAR:        ret
  %result = shl i64 %value, 3
  ret i64 %result
}

define i64 @lshr64_3(i64 %value) {
; CHECK-LABEL: lshr64_3:
; CHECK-NOT:   calls
; CHECK:       shl {{r[0-9]+}}, #13
; CHECK:       shr {{r[0-9]+}}, #3
; CHECK:       or {{r[0-9]+}}, {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = lshr i64 %value, 3
  ret i64 %result
}

define i64 @ashr64_3(i64 %value) {
; CHECK-LABEL: ashr64_3:
; CHECK-NOT:   calls
; CHECK:       shl {{r[0-9]+}}, #13
; CHECK:       ashr {{r[0-9]+}}, #3
; HUGE:        rets
; NEAR:        ret
  %result = ashr i64 %value, 3
  ret i64 %result
}

define i64 @shl64_17(i64 %value) {
; CHECK-LABEL: shl64_17:
; CHECK-NOT:   calls
; CHECK-NOT:   shl {{r[0-9]+}}
; CHECK-NOT:   shr {{r[0-9]+}}
; CHECK:       add [[LOW:r[0-9]+]], [[LOW]]
; CHECK-NEXT:  addc [[MIDDLE:r[0-9]+]], [[MIDDLE]]
; CHECK-NEXT:  addc [[HIGH:r[0-9]+]], [[HIGH]]
; CHECK-NOT:   shl {{r[0-9]+}}
; CHECK-NOT:   shr {{r[0-9]+}}
; HUGE:        rets
; NEAR:        ret
  %result = shl i64 %value, 17
  ret i64 %result
}

define i64 @lshr64_17(i64 %value) {
; CHECK-LABEL: lshr64_17:
; CHECK-NOT:   calls
; CHECK:       shl {{r[0-9]+}}, #15
; CHECK:       shr {{r[0-9]+}}, #1
; HUGE:        rets
; NEAR:        ret
  %result = lshr i64 %value, 17
  ret i64 %result
}

define i64 @ashr64_63(i64 %value) {
; CHECK-LABEL: ashr64_63:
; CHECK-NOT:   calls
; CHECK:       ashr {{r[0-9]+}}, #15
; HUGE:        rets
; NEAR:        ret
  %result = ashr i64 %value, 63
  ret i64 %result
}
