; RUN: llc -mtriple=c166-none-elf -code-model=large -O1 \
; RUN:   -verify-machineinstrs < %s | FileCheck %s

@name = private addrspace(2) constant [1 x i8] zeroinitializer

declare i16 @callee(i16, i16, i16, i16, i16, i16, i16, i16, i16) addrspace(1)
declare void @sink(i32, i32, ptr addrspace(2)) addrspace(1)

define void @stack_argument_cfg(i16 %a, i16 %b, i16 %c, i16 %d,
                                i16 %e, i16 %f, i16 %g, i16 %h) addrspace(1) {
entry:
  %condition = icmp ult i16 %a, %e
  %selected = select i1 %condition, i16 4951, i16 9320
  %actual = call i16 @callee(i16 %a, i16 %b, i16 %c, i16 %d,
                            i16 %e, i16 %f, i16 %g, i16 %h, i16 12)
  %expected = xor i16 %selected, %a
  %actual.wide = zext i16 %actual to i32
  %expected.wide = zext i16 %expected to i32
  call void @sink(i32 %actual.wide, i32 %expected.wide,
                  ptr addrspace(2) @name)
  ret void
}

; The select custom inserter splits a block while the callee's outgoing stack
; frame is partially allocated. Verify that the generated CFG retains a
; stable base for fixed arguments and spill slots on every edge.
;
; CHECK-LABEL: _stack_argument_cfg:
; CHECK:       mov [-r0], r6
; CHECK:       mov r6, r0
; CHECK:       mov r1, [r6 + #12]
; CHECK:       calls seg(_callee), sof(_callee)
; CHECK:       mov r0, r6
