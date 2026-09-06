; RUN: llc -mtriple=c166-none-elf -O2 < %s | FileCheck %s

define void @copy_near_byte(ptr addrspace(3) %destination,
                            ptr addrspace(3) %source) {
; CHECK-LABEL: _copy_near_byte:
; CHECK:       movb [{{r[0-9]+}}], [{{r[0-9]+}}]
; CHECK-NEXT:  rets
  %value = load i8, ptr addrspace(3) %source, align 1
  store i8 %value, ptr addrspace(3) %destination, align 1
  ret void
}

define void @copy_far_byte(ptr addrspace(2) %destination,
                           ptr addrspace(2) %source) {
; CHECK-LABEL: _copy_far_byte:
; CHECK:       extp
; CHECK-NEXT:  movb [[VALUE:r[lh][0-7]]], [{{r[0-9]+}}]
; CHECK-NEXT:  extp
; CHECK-NEXT:  movb [{{r[0-9]+}}], [[VALUE]]
; CHECK-NEXT:  rets
; CHECK-NOT:   movb [{{r[0-9]+}}], [{{r[0-9]+}}]
  %value = load i8, ptr addrspace(2) %source, align 1
  store i8 %value, ptr addrspace(2) %destination, align 1
  ret void
}
