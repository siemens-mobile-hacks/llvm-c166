# RUN: llvm-mc -triple=c166-none-elf -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=c166-none-elf -filetype=obj %s -o - \
# RUN:   | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

bmov r4.15, r5.0
# CHECK: bmov r4.15, r5.0
# CHECK-SAME: encoding: [0x4a,0xf5,0xf4,0x0f]
# DIS: bmov r4.15, r5.0

bmov r15.0, r0.15
# CHECK: bmov r15.0, r0.15
# CHECK-SAME: encoding: [0x4a,0xf0,0xff,0xf0]
# DIS: bmov r15.0, r0.15

bmov psw.11, r1.11
# CHECK: bmov psw.11, r1.11
# CHECK-SAME: encoding: [0x4a,0xf1,0x88,0xbb]
# DIS: bmov psw.11, r1.11

bmovn r4.15, r5.0
# CHECK: bmovn r4.15, r5.0
# CHECK-SAME: encoding: [0x3a,0xf5,0xf4,0x0f]
# DIS: bmovn r4.15, r5.0

band r4.15, r5.0
# CHECK: band r4.15, r5.0
# CHECK-SAME: encoding: [0x6a,0xf5,0xf4,0x0f]
# DIS: band r4.15, r5.0

bor r4.15, r5.0
# CHECK: bor r4.15, r5.0
# CHECK-SAME: encoding: [0x5a,0xf5,0xf4,0x0f]
# DIS: bor r4.15, r5.0

bxor r4.15, r5.0
# CHECK: bxor r4.15, r5.0
# CHECK-SAME: encoding: [0x7a,0xf5,0xf4,0x0f]
# DIS: bxor r4.15, r5.0

bcmp r3.1, r3.0
# CHECK: bcmp r3.1, r3.0
# CHECK-SAME: encoding: [0x2a,0xf3,0xf3,0x01]
# DIS: bcmp r3.1, r3.0

jmpr cc_n, 0
# CHECK: jmpr cc_n, 0
# CHECK-SAME: encoding: [0x6d,0x00]
# DIS: jmpr cc_n, 0

jmpr cc_nn, 0
# CHECK: jmpr cc_nn, 0
# CHECK-SAME: encoding: [0x7d,0x00]
# DIS: jmpr cc_nn, 0
