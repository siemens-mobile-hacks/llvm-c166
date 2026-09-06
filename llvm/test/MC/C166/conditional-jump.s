# RUN: llvm-mc -triple=c166-none-elf -show-encoding %s \
# RUN:   | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s -o - \
# RUN:   | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

jmpa cc_eq, 0x1234
# ENC: jmpa cc_eq, 4660{{.*}}encoding: [0xea,0x20,0x34,0x12]
# DIS: ea 20 34 12{{.*}}jmpa cc_eq, 4660
jmpa cc_ne, 0x1234
# ENC: jmpa cc_ne, 4660{{.*}}encoding: [0xea,0x30,0x34,0x12]
# DIS: ea 30 34 12{{.*}}jmpa cc_ne, 4660
jmpa cc_n, 0x1234
# ENC: jmpa cc_n, 4660{{.*}}encoding: [0xea,0x60,0x34,0x12]
# DIS: ea 60 34 12{{.*}}jmpa cc_n, 4660
jmpa cc_nn, 0x1234
# ENC: jmpa cc_nn, 4660{{.*}}encoding: [0xea,0x70,0x34,0x12]
# DIS: ea 70 34 12{{.*}}jmpa cc_nn, 4660
jmpa cc_ult, 0x1234
# ENC: jmpa cc_ult, 4660{{.*}}encoding: [0xea,0x80,0x34,0x12]
# DIS: ea 80 34 12{{.*}}jmpa cc_ult, 4660
jmpa cc_uge, 0x1234
# ENC: jmpa cc_uge, 4660{{.*}}encoding: [0xea,0x90,0x34,0x12]
# DIS: ea 90 34 12{{.*}}jmpa cc_uge, 4660
jmpa cc_sgt, 0x1234
# ENC: jmpa cc_sgt, 4660{{.*}}encoding: [0xea,0xa0,0x34,0x12]
# DIS: ea a0 34 12{{.*}}jmpa cc_sgt, 4660
jmpa cc_sle, 0x1234
# ENC: jmpa cc_sle, 4660{{.*}}encoding: [0xea,0xb0,0x34,0x12]
# DIS: ea b0 34 12{{.*}}jmpa cc_sle, 4660
jmpa cc_slt, 0x1234
# ENC: jmpa cc_slt, 4660{{.*}}encoding: [0xea,0xc0,0x34,0x12]
# DIS: ea c0 34 12{{.*}}jmpa cc_slt, 4660
jmpa cc_sge, 0x1234
# ENC: jmpa cc_sge, 4660{{.*}}encoding: [0xea,0xd0,0x34,0x12]
# DIS: ea d0 34 12{{.*}}jmpa cc_sge, 4660
jmpa cc_ugt, 0x1234
# ENC: jmpa cc_ugt, 4660{{.*}}encoding: [0xea,0xe0,0x34,0x12]
# DIS: ea e0 34 12{{.*}}jmpa cc_ugt, 4660
jmpa cc_ule, 0x1234
# ENC: jmpa cc_ule, 4660{{.*}}encoding: [0xea,0xf0,0x34,0x12]
# DIS: ea f0 34 12{{.*}}jmpa cc_ule, 4660
