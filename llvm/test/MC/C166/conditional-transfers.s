; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

calla cc_net, 0x1234
; ASM: calla cc_net, 4660{{.*}}encoding: [0xca,0x10,0x34,0x12]
; DIS: ca 10 34 12{{.*}}calla cc_net, 4660
calli cc_net, [r0]
; ASM: calli cc_net, [r0]{{.*}}encoding: [0xab,0x10]
; DIS: ab 10{{.*}}calli cc_net, [r0]
jmpi cc_net, [r15]
; ASM: jmpi cc_net, [r15]{{.*}}encoding: [0x9c,0x1f]
; DIS: 9c 1f{{.*}}jmpi cc_net, [r15]

calla cc_eq, 0x1234
; ASM: calla cc_eq, 4660{{.*}}encoding: [0xca,0x20,0x34,0x12]
; DIS: ca 20 34 12{{.*}}calla cc_eq, 4660
calli cc_eq, [r0]
; ASM: calli cc_eq, [r0]{{.*}}encoding: [0xab,0x20]
; DIS: ab 20{{.*}}calli cc_eq, [r0]
jmpi cc_eq, [r15]
; ASM: jmpi cc_eq, [r15]{{.*}}encoding: [0x9c,0x2f]
; DIS: 9c 2f{{.*}}jmpi cc_eq, [r15]

calla cc_ne, 0x1234
; ASM: calla cc_ne, 4660{{.*}}encoding: [0xca,0x30,0x34,0x12]
; DIS: ca 30 34 12{{.*}}calla cc_ne, 4660
calli cc_ne, [r0]
; ASM: calli cc_ne, [r0]{{.*}}encoding: [0xab,0x30]
; DIS: ab 30{{.*}}calli cc_ne, [r0]
jmpi cc_ne, [r15]
; ASM: jmpi cc_ne, [r15]{{.*}}encoding: [0x9c,0x3f]
; DIS: 9c 3f{{.*}}jmpi cc_ne, [r15]

calla cc_v, 0x1234
; ASM: calla cc_v, 4660{{.*}}encoding: [0xca,0x40,0x34,0x12]
; DIS: ca 40 34 12{{.*}}calla cc_v, 4660
calli cc_v, [r0]
; ASM: calli cc_v, [r0]{{.*}}encoding: [0xab,0x40]
; DIS: ab 40{{.*}}calli cc_v, [r0]
jmpi cc_v, [r15]
; ASM: jmpi cc_v, [r15]{{.*}}encoding: [0x9c,0x4f]
; DIS: 9c 4f{{.*}}jmpi cc_v, [r15]

calla cc_nv, 0x1234
; ASM: calla cc_nv, 4660{{.*}}encoding: [0xca,0x50,0x34,0x12]
; DIS: ca 50 34 12{{.*}}calla cc_nv, 4660
calli cc_nv, [r0]
; ASM: calli cc_nv, [r0]{{.*}}encoding: [0xab,0x50]
; DIS: ab 50{{.*}}calli cc_nv, [r0]
jmpi cc_nv, [r15]
; ASM: jmpi cc_nv, [r15]{{.*}}encoding: [0x9c,0x5f]
; DIS: 9c 5f{{.*}}jmpi cc_nv, [r15]

calla cc_n, 0x1234
; ASM: calla cc_n, 4660{{.*}}encoding: [0xca,0x60,0x34,0x12]
; DIS: ca 60 34 12{{.*}}calla cc_n, 4660
calli cc_n, [r0]
; ASM: calli cc_n, [r0]{{.*}}encoding: [0xab,0x60]
; DIS: ab 60{{.*}}calli cc_n, [r0]
jmpi cc_n, [r15]
; ASM: jmpi cc_n, [r15]{{.*}}encoding: [0x9c,0x6f]
; DIS: 9c 6f{{.*}}jmpi cc_n, [r15]

calla cc_nn, 0x1234
; ASM: calla cc_nn, 4660{{.*}}encoding: [0xca,0x70,0x34,0x12]
; DIS: ca 70 34 12{{.*}}calla cc_nn, 4660
calli cc_nn, [r0]
; ASM: calli cc_nn, [r0]{{.*}}encoding: [0xab,0x70]
; DIS: ab 70{{.*}}calli cc_nn, [r0]
jmpi cc_nn, [r15]
; ASM: jmpi cc_nn, [r15]{{.*}}encoding: [0x9c,0x7f]
; DIS: 9c 7f{{.*}}jmpi cc_nn, [r15]

calla cc_ult, 0x1234
; ASM: calla cc_ult, 4660{{.*}}encoding: [0xca,0x80,0x34,0x12]
; DIS: ca 80 34 12{{.*}}calla cc_ult, 4660
calli cc_ult, [r0]
; ASM: calli cc_ult, [r0]{{.*}}encoding: [0xab,0x80]
; DIS: ab 80{{.*}}calli cc_ult, [r0]
jmpi cc_ult, [r15]
; ASM: jmpi cc_ult, [r15]{{.*}}encoding: [0x9c,0x8f]
; DIS: 9c 8f{{.*}}jmpi cc_ult, [r15]

calla cc_uge, 0x1234
; ASM: calla cc_uge, 4660{{.*}}encoding: [0xca,0x90,0x34,0x12]
; DIS: ca 90 34 12{{.*}}calla cc_uge, 4660
calli cc_uge, [r0]
; ASM: calli cc_uge, [r0]{{.*}}encoding: [0xab,0x90]
; DIS: ab 90{{.*}}calli cc_uge, [r0]
jmpi cc_uge, [r15]
; ASM: jmpi cc_uge, [r15]{{.*}}encoding: [0x9c,0x9f]
; DIS: 9c 9f{{.*}}jmpi cc_uge, [r15]

calla cc_sgt, 0x1234
; ASM: calla cc_sgt, 4660{{.*}}encoding: [0xca,0xa0,0x34,0x12]
; DIS: ca a0 34 12{{.*}}calla cc_sgt, 4660
calli cc_sgt, [r0]
; ASM: calli cc_sgt, [r0]{{.*}}encoding: [0xab,0xa0]
; DIS: ab a0{{.*}}calli cc_sgt, [r0]
jmpi cc_sgt, [r15]
; ASM: jmpi cc_sgt, [r15]{{.*}}encoding: [0x9c,0xaf]
; DIS: 9c af{{.*}}jmpi cc_sgt, [r15]

calla cc_sle, 0x1234
; ASM: calla cc_sle, 4660{{.*}}encoding: [0xca,0xb0,0x34,0x12]
; DIS: ca b0 34 12{{.*}}calla cc_sle, 4660
calli cc_sle, [r0]
; ASM: calli cc_sle, [r0]{{.*}}encoding: [0xab,0xb0]
; DIS: ab b0{{.*}}calli cc_sle, [r0]
jmpi cc_sle, [r15]
; ASM: jmpi cc_sle, [r15]{{.*}}encoding: [0x9c,0xbf]
; DIS: 9c bf{{.*}}jmpi cc_sle, [r15]

calla cc_slt, 0x1234
; ASM: calla cc_slt, 4660{{.*}}encoding: [0xca,0xc0,0x34,0x12]
; DIS: ca c0 34 12{{.*}}calla cc_slt, 4660
calli cc_slt, [r0]
; ASM: calli cc_slt, [r0]{{.*}}encoding: [0xab,0xc0]
; DIS: ab c0{{.*}}calli cc_slt, [r0]
jmpi cc_slt, [r15]
; ASM: jmpi cc_slt, [r15]{{.*}}encoding: [0x9c,0xcf]
; DIS: 9c cf{{.*}}jmpi cc_slt, [r15]

calla cc_sge, 0x1234
; ASM: calla cc_sge, 4660{{.*}}encoding: [0xca,0xd0,0x34,0x12]
; DIS: ca d0 34 12{{.*}}calla cc_sge, 4660
calli cc_sge, [r0]
; ASM: calli cc_sge, [r0]{{.*}}encoding: [0xab,0xd0]
; DIS: ab d0{{.*}}calli cc_sge, [r0]
jmpi cc_sge, [r15]
; ASM: jmpi cc_sge, [r15]{{.*}}encoding: [0x9c,0xdf]
; DIS: 9c df{{.*}}jmpi cc_sge, [r15]

calla cc_ugt, 0x1234
; ASM: calla cc_ugt, 4660{{.*}}encoding: [0xca,0xe0,0x34,0x12]
; DIS: ca e0 34 12{{.*}}calla cc_ugt, 4660
calli cc_ugt, [r0]
; ASM: calli cc_ugt, [r0]{{.*}}encoding: [0xab,0xe0]
; DIS: ab e0{{.*}}calli cc_ugt, [r0]
jmpi cc_ugt, [r15]
; ASM: jmpi cc_ugt, [r15]{{.*}}encoding: [0x9c,0xef]
; DIS: 9c ef{{.*}}jmpi cc_ugt, [r15]

calla cc_ule, 0x1234
; ASM: calla cc_ule, 4660{{.*}}encoding: [0xca,0xf0,0x34,0x12]
; DIS: ca f0 34 12{{.*}}calla cc_ule, 4660
calli cc_ule, [r0]
; ASM: calli cc_ule, [r0]{{.*}}encoding: [0xab,0xf0]
; DIS: ab f0{{.*}}calli cc_ule, [r0]
jmpi cc_ule, [r15]
; ASM: jmpi cc_ule, [r15]{{.*}}encoding: [0x9c,0xff]
; DIS: 9c ff{{.*}}jmpi cc_ule, [r15]
