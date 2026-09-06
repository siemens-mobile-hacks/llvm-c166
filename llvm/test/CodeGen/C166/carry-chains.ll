; RUN: llc -mtriple=c166-none-elf -code-model=large -O2 \
; RUN:   -verify-machineinstrs < %s | FileCheck %s

declare { i16, i1 } @llvm.uadd.with.overflow.i16(i16, i16)
declare { i16, i1 } @llvm.usub.with.overflow.i16(i16, i16)
declare { i32, i1 } @llvm.usub.with.overflow.i32(i32, i32)
declare i16 @llvm.fshl.i16(i16, i16, i16)
declare void @borrow_handler()

@borrow_sink0 = external addrspace(2) global i16
@borrow_sink1 = external addrspace(2) global i16
@shift_sink0 = external addrspace(2) global i16
@shift_sink1 = external addrspace(2) global i16

define void @split_left_shift_one(i16 %low, i16 %high) {
; CHECK-LABEL: split_left_shift_one:
; CHECK-NOT:   shl
; CHECK-NOT:   shr
; CHECK:       add [[LOW:r[0-9]+]], [[LOW]]
; CHECK-NOT:   shl
; CHECK-NOT:   shr
; CHECK:       addc [[HIGH:r[0-9]+]], [[HIGH]]
; CHECK-NOT:   shl
; CHECK-NOT:   shr
; CHECK:       rets
  %low.shift = shl i16 %low, 1
  %carry = lshr i16 %low, 15
  %high.shift = shl i16 %high, 1
  %high.result = or i16 %high.shift, %carry
  store volatile i16 %low.shift, ptr addrspace(2) @shift_sink0, align 2
  store volatile i16 %high.result, ptr addrspace(2) @shift_sink1, align 2
  ret void
}

define void @split_funnel_left_one(i16 %low, i16 %high) {
; CHECK-LABEL: split_funnel_left_one:
; CHECK-NOT:   shl
; CHECK-NOT:   shr
; CHECK:       add [[LOW:r[0-9]+]], [[LOW]]
; CHECK-NOT:   shl
; CHECK-NOT:   shr
; CHECK:       addc [[HIGH:r[0-9]+]], [[HIGH]]
; CHECK-NOT:   shl
; CHECK-NOT:   shr
; CHECK:       rets
  %low.shift = shl i16 %low, 1
  %high.result = call i16 @llvm.fshl.i16(i16 %high, i16 %low, i16 1)
  store volatile i16 %low.shift, ptr addrspace(2) @shift_sink0, align 2
  store volatile i16 %high.result, ptr addrspace(2) @shift_sink1, align 2
  ret void
}

define i16 @branch_on_sub_borrow(i16 %value) {
; CHECK-LABEL: branch_on_sub_borrow:
; CHECK-NOT:   mov {{r[0-9]+}}, #1
; CHECK:       sub [[RESULT:r[0-9]+]], #1
; CHECK-NEXT:  jmpr cc_uge,
; CHECK-NOT:   addc
; CHECK-NOT:   cmp
; CHECK:       calls
; CHECK:       rets
  %pair = call { i16, i1 } @llvm.usub.with.overflow.i16(i16 %value, i16 1)
  %result = extractvalue { i16, i1 } %pair, 0
  %borrow = extractvalue { i16, i1 } %pair, 1
  br i1 %borrow, label %handle, label %done

handle:
  call void @borrow_handler()
  br label %done

done:
  ret i16 %result
}

define i16 @add_chain(i16 %a0, i16 %b0, i16 %a1, i16 %b1,
                      i16 %a2, i16 %b2) {
; CHECK-LABEL: add_chain:
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       add {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       addc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       addc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       rets
  %first = call { i16, i1 } @llvm.uadd.with.overflow.i16(i16 %a0, i16 %b0)
  %carry0 = extractvalue { i16, i1 } %first, 1
  %carry0.word = zext i1 %carry0 to i16

  %second.base =
      call { i16, i1 } @llvm.uadd.with.overflow.i16(i16 %a1, i16 %b1)
  %second.word = extractvalue { i16, i1 } %second.base, 0
  %second.carry = call { i16, i1 } @llvm.uadd.with.overflow.i16(
      i16 %second.word, i16 %carry0.word)
  %carry1.base = extractvalue { i16, i1 } %second.base, 1
  %carry1.more = extractvalue { i16, i1 } %second.carry, 1
  %carry1 = or i1 %carry1.base, %carry1.more
  %carry1.word = zext i1 %carry1 to i16

  %third.base = add i16 %a2, %b2
  %third = add i16 %third.base, %carry1.word
  ret i16 %third
}

define i16 @sub_chain(i16 %a0, i16 %b0, i16 %a1, i16 %b1,
                      i16 %a2, i16 %b2) {
; CHECK-LABEL: sub_chain:
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       sub {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       subc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       subc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       rets
  %first = call { i16, i1 } @llvm.usub.with.overflow.i16(i16 %a0, i16 %b0)
  %borrow0 = extractvalue { i16, i1 } %first, 1
  %borrow0.word = zext i1 %borrow0 to i16

  %second.base =
      call { i16, i1 } @llvm.usub.with.overflow.i16(i16 %a1, i16 %b1)
  %second.word = extractvalue { i16, i1 } %second.base, 0
  %second.borrow = call { i16, i1 } @llvm.usub.with.overflow.i16(
      i16 %second.word, i16 %borrow0.word)
  %borrow1.base = extractvalue { i16, i1 } %second.base, 1
  %borrow1.more = extractvalue { i16, i1 } %second.borrow, 1
  %borrow1 = or i1 %borrow1.base, %borrow1.more
  %borrow1.word = zext i1 %borrow1 to i16

  %third.base = sub i16 %a2, %b2
  %third = sub i16 %third.base, %borrow1.word
  ret i16 %third
}

define i32 @shared_sub_overflow_i32(i32 %a, i32 %b) {
; CHECK-LABEL: shared_sub_overflow_i32:
; CHECK:       sub {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK:       mov [[BORROW:r[0-9]+]], #0
; CHECK-COUNT-1: addc [[BORROW]], #0
; CHECK:       mov {{.*}}, [[BORROW]]
; CHECK:       mov {{.*}}, [[BORROW]]
; CHECK:       rets
  %pair = call { i32, i1 } @llvm.usub.with.overflow.i32(i32 %a, i32 %b)
  %difference = extractvalue { i32, i1 } %pair, 0
  %borrow = extractvalue { i32, i1 } %pair, 1
  %borrow.word = zext i1 %borrow to i16
  store volatile i16 %borrow.word, ptr addrspace(2) @borrow_sink0, align 2
  store volatile i16 %borrow.word, ptr addrspace(2) @borrow_sink1, align 2
  ret i32 %difference
}

define void @add_i64(ptr addrspace(2) %left, ptr addrspace(2) %right) {
; CHECK-LABEL: add_i64:
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       add {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  addc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK:       mov {{r[0-9]+}}, [{{r[0-9]+}} + #4]
; CHECK:       mov {{r[0-9]+}}, [{{r[0-9]+}} + #6]
; CHECK:       addc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  addc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       rets
  %rhs = load i64, ptr addrspace(2) %right, align 2
  %lhs = load i64, ptr addrspace(2) %left, align 2
  %sum = add i64 %lhs, %rhs
  store i64 %sum, ptr addrspace(2) %left, align 2
  ret void
}

define void @sub_i64(ptr addrspace(2) %left, ptr addrspace(2) %right) {
; CHECK-LABEL: sub_i64:
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       sub {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK:       mov {{r[0-9]+}}, [{{r[0-9]+}} + #4]
; CHECK:       mov {{r[0-9]+}}, [{{r[0-9]+}} + #6]
; CHECK:       subc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; CHECK-NOT:   sub r0
; CHECK-NOT:   cmp
; CHECK:       rets
  %rhs = load i64, ptr addrspace(2) %right, align 2
  %lhs = load i64, ptr addrspace(2) %left, align 2
  %difference = sub i64 %lhs, %rhs
  store i64 %difference, ptr addrspace(2) %left, align 2
  ret void
}
