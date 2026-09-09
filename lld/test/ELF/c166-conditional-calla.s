# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x120100 --defsym=target=0x123456 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o -Ttext=0x120100 --defsym=target=0x133456 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR
# RELOC: 0x2 R_C166_COF16 target 0x0
# RELOC: 0x6 R_C166_COF16 target 0x2
# RELOC: 0xA R_C166_COF16 .text 0xC
# BYTES: 120100 ca105634 ca405834 ca500c01 cb00
# ERROR: same-segment code relocation to 'target' crosses a 64 KiB code boundary
.globl _start
_start:
calla cc_net, target
calla cc_v, target+2
calla cc_nv, .Lcallee
.Lcallee:
ret
