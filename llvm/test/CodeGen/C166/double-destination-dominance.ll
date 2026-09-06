; RUN: llc -mtriple=c166-none-elf -code-model=large -O0 -verify-each -verify-machineinstrs -o /dev/null %s
; RUN: llc -mtriple=c166-none-elf -code-model=medium -O0 -verify-each -verify-machineinstrs -o /dev/null %s
; RUN: llc -mtriple=c166-none-elf -code-model=small -O0 -verify-each -verify-machineinstrs -o /dev/null %s

; The store address is defined after the arithmetic operation. Using it as
; the helper destination at the producer would violate SSA dominance.
define void @late_address(double %x, double %y) {
entry:
  %p = alloca [2 x double], align 2, addrspace(2)
  %sum = fadd double %x, %y
  %q = getelementptr [2 x double], ptr addrspace(2) %p, i16 0, i16 1
  store double %sum, ptr addrspace(2) %q, align 2
  ret void
}

; Nor may the write be moved to a predecessor of a conditional store.
define void @conditional_destination(double %x, double %y, i1 %write) {
entry:
  %p = alloca [2 x double], align 2, addrspace(2)
  %sum = fadd double %x, %y
  br i1 %write, label %store, label %exit
store:
  %q = getelementptr [2 x double], ptr addrspace(2) %p, i16 0, i16 1
  store double %sum, ptr addrspace(2) %q, align 2
  br label %exit
exit:
  ret void
}
