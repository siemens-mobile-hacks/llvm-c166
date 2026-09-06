; RUN: llc -mtriple=c166-none-elf -O0 -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=O0
; RUN: llc -mtriple=c166-none-elf -O2 -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=O2

declare void @sink()
declare void @sink_word(i16)

define void @reduce_words_after_subtract(ptr addrspace(2) %destination,
                                         i64 %lhs, i64 %rhs) {
; O2-LABEL: reduce_words_after_subtract:
; O2-COUNT-3: {{^ *}}or
; O2-NEXT:  jmpr cc_eq,
  %difference = sub i64 %lhs, %rhs
  store i64 %difference, ptr addrspace(2) %destination, align 2
  %shift16 = lshr i64 %difference, 16
  %shift32 = lshr i64 %difference, 32
  %low_pair = or i64 %shift16, %shift32
  %shift48 = lshr i64 %difference, 48
  %high_pair = or i64 %low_pair, %shift48
  %all_words = or i64 %high_pair, %difference
  %word = trunc i64 %all_words to i16
  %zero = icmp eq i16 %word, 0
  br i1 %zero, label %done, label %nonzero

nonzero:
  %top64 = lshr i64 %difference, 48
  %top = trunc i64 %top64 to i16
  tail call void @sink_word(i16 %top)
  br label %done

done:
  ret void
}

define i16 @materialize_ult(i64 %lhs, i64 %rhs) {
; O0-LABEL: materialize_ult:
; O0:       mov [[BORROW:r[0-9]+]], #0
; O0-NEXT:  sub {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  addc [[BORROW]], #0
; O0-NEXT:  rets
  %condition = icmp ult i64 %lhs, %rhs
  %result = zext i1 %condition to i16
  ret i16 %result
}

define i16 @materialize_uge(i64 %lhs, i64 %rhs) {
; O0-LABEL: materialize_uge:
; O0:       mov [[BORROW:r[0-9]+]], #0
; O0-NEXT:  sub {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  addc [[BORROW]], #0
; O0-NEXT:  xor [[BORROW]], #1
; O0-NEXT:  rets
  %condition = icmp uge i64 %lhs, %rhs
  %result = zext i1 %condition to i16
  ret i16 %result
}

define i16 @materialize_slt(i64 %lhs, i64 %rhs) {
; O0-LABEL: materialize_slt:
; O0-COUNT-2: xor {{r[0-9]+}}, #32768
; O0:       mov [[BORROW:r[0-9]+]], #0
; O0-NEXT:  sub {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  addc [[BORROW]], #0
; O0-NEXT:  rets
  %condition = icmp slt i64 %lhs, %rhs
  %result = zext i1 %condition to i16
  ret i16 %result
}

define i16 @materialize_sge(i64 %lhs, i64 %rhs) {
; O0-LABEL: materialize_sge:
; O0-COUNT-2: xor {{r[0-9]+}}, #32768
; O0:       mov [[BORROW:r[0-9]+]], #0
; O0-NEXT:  sub {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O0-NEXT:  addc [[BORROW]], #0
; O0-NEXT:  xor [[BORROW]], #1
; O0-NEXT:  rets
  %condition = icmp sge i64 %lhs, %rhs
  %result = zext i1 %condition to i16
  ret i16 %result
}

define void @branch_ult(i64 %lhs, i64 %rhs) {
; O2-LABEL: branch_ult:
; O2:       sub {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  jmpr cc_uge,
; O2-NOT:   addc
; O2-NOT:   cmp
; O2:       calls
  %condition = icmp ult i64 %lhs, %rhs
  br i1 %condition, label %taken, label %done

taken:
  tail call void @sink()
  br label %done

done:
  ret void
}

define void @branch_uge(i64 %lhs, i64 %rhs) {
; O2-LABEL: branch_uge:
; O2:       sub {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  jmpr cc_ult,
; O2-NOT:   addc
; O2-NOT:   cmp
; O2:       calls
  %condition = icmp uge i64 %lhs, %rhs
  br i1 %condition, label %taken, label %done

taken:
  tail call void @sink()
  br label %done

done:
  ret void
}

define void @branch_slt(i64 %lhs, i64 %rhs) {
; O2-LABEL: branch_slt:
; O2-COUNT-2: xor {{r[0-9]+}}, #32768
; O2:       sub {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  jmpr cc_uge,
; O2-NOT:   addc
; O2-NOT:   cmp
; O2:       calls
  %condition = icmp slt i64 %lhs, %rhs
  br i1 %condition, label %taken, label %done

taken:
  tail call void @sink()
  br label %done

done:
  ret void
}

define void @branch_sge(i64 %lhs, i64 %rhs) {
; O2-LABEL: branch_sge:
; O2-COUNT-2: xor {{r[0-9]+}}, #32768
; O2:       sub {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  subc {{r[0-9]+}}, {{r[0-9]+}}
; O2-NEXT:  jmpr cc_ult,
; O2-NOT:   addc
; O2-NOT:   cmp
; O2:       calls
  %condition = icmp sge i64 %lhs, %rhs
  br i1 %condition, label %taken, label %done

taken:
  tail call void @sink()
  br label %done

done:
  ret void
}
