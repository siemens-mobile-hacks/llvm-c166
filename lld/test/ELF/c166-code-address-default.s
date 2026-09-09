# REQUIRES: c166
# RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o -Ttext=0x120100 --defsym=target=0x123456 -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s --check-prefix=BYTES
# RUN: not ld.lld %t.o -Ttext=0x120100 --defsym=target=0x133456 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERROR

## A bare symbolic code address has the same semantics as explicit cof().
# RELOC: 0x2 R_C166_COF16 target 0x0
# RELOC: 0x6 R_C166_COF16 target 0x0
# RELOC: 0xA R_C166_COF16 target 0x0
# RELOC: 0xE R_C166_COF16 target 0x0
# RELOC: 0x12 R_C166_COF16 target 0x0
# RELOC: 0x16 R_C166_COF16 target 0x0
# BYTES:      120100 ca005634 ca005634 ea005634 ea005634
# BYTES-NEXT: 120110 e2f45634 e2f45634
# ERROR-COUNT-6: same-segment code relocation to 'target' crosses a 64 KiB code boundary
.globl _start
_start:
calla cc_uc, target
calla cc_uc, cof(target)
jmpa cc_uc, target
jmpa cc_uc, cof(target)
pcall r4, target
pcall r4, cof(target)
