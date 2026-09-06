; RUN: llc -mtriple=c166-none-elf -code-model=large -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; RUN: llc -mtriple=c166-none-elf -code-model=medium -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,NEAR
; RUN: llc -mtriple=c166-none-elf -code-model=small -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,HUGE
; C166-ABI: floating.double_stack_and_return

@left_sink = addrspace(2) global double 0.0, align 2
@right_sink = addrspace(2) global double 0.0, align 2

; Legalization splits each double into two i32 inputs. Both pieces must be
; reconstructed from the public MSW-first stack representation; none may be
; assigned to the ordinary R12-R15 argument area.
define internal fastcc void @double_callee(double %left, double %right) noinline {
; CHECK-LABEL: _double_callee:
; CHECK-DAG:   mov {{r[0-9]+}}, [r0]
; CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #2]
; CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #4]
; CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #6]
; CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #8]
; CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #10]
; CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #12]
; CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #14]
; HUGE:        rets
; NEAR:        ret
  store volatile double %left, ptr addrspace(2) @left_sink, align 2
  store volatile double %right, ptr addrspace(2) @right_sink, align 2
  ret void
}

define void @call_double_callee(double %left, double %right) {
; CHECK-LABEL: _call_double_callee:
; HUGE:        calls seg(_double_callee), sof(_double_callee)
; NEAR:        calla cc_uc, cof(_double_callee)
; CHECK-NEXT:  add r0, #16
  call fastcc void @double_callee(double %left, double %right)
  ret void
}
