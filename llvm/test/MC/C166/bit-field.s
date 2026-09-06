# RUN: llvm-mc -triple c166 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple c166 -filetype=obj %s -o - | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

bfldl r4, #240, #16
bfldh r13, #240, #16
bfldl 128, #52, #18
bfldh psw, #18, #52

# ENC: bfldl r4, #240, #16{{.*}}encoding: [0x0a,0xf4,0xf0,0x10]
# ENC: bfldh r13, #240, #16{{.*}}encoding: [0x1a,0xfd,0x10,0xf0]
# ENC: bfldl 128, #52, #18{{.*}}encoding: [0x0a,0x80,0x34,0x12]
# ENC: bfldh psw, #18, #52{{.*}}encoding: [0x1a,0x88,0x34,0x12]

# DIS: bfldl r4, #240, #16
# DIS: bfldh r13, #240, #16
# DIS: bfldl 128, #52, #18
# DIS: bfldh psw, #18, #52
