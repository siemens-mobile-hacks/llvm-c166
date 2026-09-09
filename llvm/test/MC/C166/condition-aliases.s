; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s --check-prefix=ASM
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

; Aliases assemble to the canonical condition, including mixed case input.
JMPR CC_Z, 1
; ASM: jmpr cc_eq, 1{{.*}}encoding: [0x2d,0x01]
; DIS: 2d 01{{.*}}jmpr cc_eq, 1

JMPA CC_Z, 0x1234
; ASM: jmpa cc_eq, 4660{{.*}}encoding: [0xea,0x20,0x34,0x12]
; DIS: ea 20 34 12{{.*}}jmpa cc_eq, 4660

JMPI CC_Z, [r15]
; ASM: jmpi cc_eq, [r15]{{.*}}encoding: [0x9c,0x2f]
; DIS: 9c 2f{{.*}}jmpi cc_eq, [r15]

CALLA CC_Z, 0x1234
; ASM: calla cc_eq, 4660{{.*}}encoding: [0xca,0x20,0x34,0x12]
; DIS: ca 20 34 12{{.*}}calla cc_eq, 4660

CALLI CC_Z, [r0]
; ASM: calli cc_eq, [r0]{{.*}}encoding: [0xab,0x20]
; DIS: ab 20{{.*}}calli cc_eq, [r0]

JMPR CC_NZ, 1
; ASM: jmpr cc_ne, 1{{.*}}encoding: [0x3d,0x01]
; DIS: 3d 01{{.*}}jmpr cc_ne, 1

JMPA CC_NZ, 0x1234
; ASM: jmpa cc_ne, 4660{{.*}}encoding: [0xea,0x30,0x34,0x12]
; DIS: ea 30 34 12{{.*}}jmpa cc_ne, 4660

JMPI CC_NZ, [r15]
; ASM: jmpi cc_ne, [r15]{{.*}}encoding: [0x9c,0x3f]
; DIS: 9c 3f{{.*}}jmpi cc_ne, [r15]

CALLA CC_NZ, 0x1234
; ASM: calla cc_ne, 4660{{.*}}encoding: [0xca,0x30,0x34,0x12]
; DIS: ca 30 34 12{{.*}}calla cc_ne, 4660

CALLI CC_NZ, [r0]
; ASM: calli cc_ne, [r0]{{.*}}encoding: [0xab,0x30]
; DIS: ab 30{{.*}}calli cc_ne, [r0]

JMPR CC_C, 1
; ASM: jmpr cc_ult, 1{{.*}}encoding: [0x8d,0x01]
; DIS: 8d 01{{.*}}jmpr cc_ult, 1

JMPA CC_C, 0x1234
; ASM: jmpa cc_ult, 4660{{.*}}encoding: [0xea,0x80,0x34,0x12]
; DIS: ea 80 34 12{{.*}}jmpa cc_ult, 4660

JMPI CC_C, [r15]
; ASM: jmpi cc_ult, [r15]{{.*}}encoding: [0x9c,0x8f]
; DIS: 9c 8f{{.*}}jmpi cc_ult, [r15]

CALLA CC_C, 0x1234
; ASM: calla cc_ult, 4660{{.*}}encoding: [0xca,0x80,0x34,0x12]
; DIS: ca 80 34 12{{.*}}calla cc_ult, 4660

CALLI CC_C, [r0]
; ASM: calli cc_ult, [r0]{{.*}}encoding: [0xab,0x80]
; DIS: ab 80{{.*}}calli cc_ult, [r0]

JMPR CC_NC, 1
; ASM: jmpr cc_uge, 1{{.*}}encoding: [0x9d,0x01]
; DIS: 9d 01{{.*}}jmpr cc_uge, 1

JMPA CC_NC, 0x1234
; ASM: jmpa cc_uge, 4660{{.*}}encoding: [0xea,0x90,0x34,0x12]
; DIS: ea 90 34 12{{.*}}jmpa cc_uge, 4660

JMPI CC_NC, [r15]
; ASM: jmpi cc_uge, [r15]{{.*}}encoding: [0x9c,0x9f]
; DIS: 9c 9f{{.*}}jmpi cc_uge, [r15]

CALLA CC_NC, 0x1234
; ASM: calla cc_uge, 4660{{.*}}encoding: [0xca,0x90,0x34,0x12]
; DIS: ca 90 34 12{{.*}}calla cc_uge, 4660

CALLI CC_NC, [r0]
; ASM: calli cc_uge, [r0]{{.*}}encoding: [0xab,0x90]
; DIS: ab 90{{.*}}calli cc_uge, [r0]
