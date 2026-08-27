# RUN: llvm-mc -triple c166-none-elf -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple c166-none-elf -filetype=obj %s -o - | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

jmpi cc_uc, [r4]

# ENC: jmpi cc_uc, [r4]{{.*}}encoding: [0x9c,0x04]
# DIS: 9c 04{{.*}}jmpi cc_uc, [r4]
