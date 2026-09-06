# RUN: llvm-mc -triple c166 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple c166 -filetype=obj %s -o - | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

neg r4
cpl r13

# ENC: neg r4{{.*}}encoding: [0x81,0x40]
# ENC: cpl r13{{.*}}encoding: [0x91,0xd0]

# DIS: neg r4
# DIS: cpl r13
