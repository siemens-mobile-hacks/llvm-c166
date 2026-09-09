; RUN: llc -mtriple=c166-none-elf -code-model=small -O0 -verify-machineinstrs -print-after=c166-float-memory-lowering -o /dev/null %s 2>&1 | FileCheck %s --check-prefix=SMALL
; RUN: llc -mtriple=c166-none-elf -code-model=small -O2 -verify-machineinstrs -print-after=c166-float-memory-lowering -o /dev/null %s 2>&1 | FileCheck %s --check-prefix=SMALL
; RUN: llc -mtriple=c166-none-elf -code-model=medium -O0 -verify-machineinstrs -print-after=c166-float-memory-lowering -o /dev/null %s 2>&1 | FileCheck %s --check-prefix=FAR
; RUN: llc -mtriple=c166-none-elf -code-model=medium -O2 -verify-machineinstrs -print-after=c166-float-memory-lowering -o /dev/null %s 2>&1 | FileCheck %s --check-prefix=FAR
; RUN: llc -mtriple=c166-none-elf -code-model=large -O0 -verify-machineinstrs -print-after=c166-float-memory-lowering -o /dev/null %s 2>&1 | FileCheck %s --check-prefix=FAR
; RUN: llc -mtriple=c166-none-elf -code-model=large -O2 -verify-machineinstrs -print-after=c166-float-memory-lowering -o /dev/null %s 2>&1 | FileCheck %s --check-prefix=FAR

; An externally supplied cursor bypasses local va_list promotion. Its stored
; pointer still has the model's data-pointer type, not necessarily AS0.
define void @read_double(ptr addrspace(2) %cursor, ptr addrspace(2) %out) {
; SMALL-LABEL: define void @read_double(
; SMALL: %value.address = load ptr addrspace(3), ptr addrspace(2) %cursor, align 2
; SMALL: %value.next = getelementptr i8, ptr addrspace(3) %value.address, i32 8
; SMALL: store ptr addrspace(3) %value.next, ptr addrspace(2) %cursor, align 2
; SMALL: load i64, ptr addrspace(3) %value.address, align 2
; FAR-LABEL: define void @read_double(
; FAR: %value.address = load ptr addrspace(2), ptr addrspace(2) %cursor, align 2
; FAR: %value.next = getelementptr i8, ptr addrspace(2) %value.address, i32 8
; FAR: store ptr addrspace(2) %value.next, ptr addrspace(2) %cursor, align 2
; FAR: load i64, ptr addrspace(2) %value.address, align 2
  %value = va_arg ptr addrspace(2) %cursor, double
  store double %value, ptr addrspace(2) %out, align 2
  ret void
}
