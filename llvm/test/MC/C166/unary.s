# RUN: llvm-mc -triple c166 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple c166 -filetype=obj %s -o - | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

neg r4
cpl r13

# ENC: neg r4{{.*}}encoding: [0x81,0x40]
# ENC: cpl r13{{.*}}encoding: [0x91,0xd0]

# DIS: neg r4
# DIS: cpl r13

# Register-field boundaries; PRIOR also permits source/destination aliasing.
neg r0
# ENC: neg r0{{.*}}encoding: [0x81,0x00]
# DIS: neg r0
neg r15
# ENC: neg r15{{.*}}encoding: [0x81,0xf0]
# DIS: neg r15
cpl r0
# ENC: cpl r0{{.*}}encoding: [0x91,0x00]
# DIS: cpl r0
cpl r15
# ENC: cpl r15{{.*}}encoding: [0x91,0xf0]
# DIS: cpl r15
negb rl0
# ENC: negb rl0{{.*}}encoding: [0xa1,0x00]
# DIS: negb rl0
negb rh7
# ENC: negb rh7{{.*}}encoding: [0xa1,0xf0]
# DIS: negb rh7
cplb rl0
# ENC: cplb rl0{{.*}}encoding: [0xb1,0x00]
# DIS: cplb rl0
cplb rh7
# ENC: cplb rh7{{.*}}encoding: [0xb1,0xf0]
# DIS: cplb rh7
prior r0, r15
# ENC: prior r0, r15{{.*}}encoding: [0x2b,0x0f]
# DIS: prior r0, r15
prior r15, r0
# ENC: prior r15, r0{{.*}}encoding: [0x2b,0xf0]
# DIS: prior r15, r0
prior r15, r15
# ENC: prior r15, r15{{.*}}encoding: [0x2b,0xff]
# DIS: prior r15, r15
