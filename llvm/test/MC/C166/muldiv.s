; RUN: llvm-mc -triple=c166 -show-encoding %s | FileCheck %s --check-prefix=ENC
; RUN: llvm-mc -triple=c166 -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
; RUN: llvm-mc -triple=c166 %s | llvm-mc -triple=c166 -show-encoding | FileCheck %s --check-prefix=ENC

.irp reg, r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15
  div \reg
  divu \reg
  divl \reg
  divlu \reg
.endr
; ENC: div r0{{.*}}encoding: [0x4b,0x00]
; DIS: div r0{{[[:space:]]*$}}
; ENC: divu r0{{.*}}encoding: [0x5b,0x00]
; DIS: divu r0{{[[:space:]]*$}}
; ENC: divl r0{{.*}}encoding: [0x6b,0x00]
; DIS: divl r0{{[[:space:]]*$}}
; ENC: divlu r0{{.*}}encoding: [0x7b,0x00]
; DIS: divlu r0{{[[:space:]]*$}}
; ENC: div r1{{.*}}encoding: [0x4b,0x11]
; DIS: div r1{{[[:space:]]*$}}
; ENC: divu r1{{.*}}encoding: [0x5b,0x11]
; DIS: divu r1{{[[:space:]]*$}}
; ENC: divl r1{{.*}}encoding: [0x6b,0x11]
; DIS: divl r1{{[[:space:]]*$}}
; ENC: divlu r1{{.*}}encoding: [0x7b,0x11]
; DIS: divlu r1{{[[:space:]]*$}}
; ENC: div r2{{.*}}encoding: [0x4b,0x22]
; DIS: div r2{{[[:space:]]*$}}
; ENC: divu r2{{.*}}encoding: [0x5b,0x22]
; DIS: divu r2{{[[:space:]]*$}}
; ENC: divl r2{{.*}}encoding: [0x6b,0x22]
; DIS: divl r2{{[[:space:]]*$}}
; ENC: divlu r2{{.*}}encoding: [0x7b,0x22]
; DIS: divlu r2{{[[:space:]]*$}}
; ENC: div r3{{.*}}encoding: [0x4b,0x33]
; DIS: div r3{{[[:space:]]*$}}
; ENC: divu r3{{.*}}encoding: [0x5b,0x33]
; DIS: divu r3{{[[:space:]]*$}}
; ENC: divl r3{{.*}}encoding: [0x6b,0x33]
; DIS: divl r3{{[[:space:]]*$}}
; ENC: divlu r3{{.*}}encoding: [0x7b,0x33]
; DIS: divlu r3{{[[:space:]]*$}}
; ENC: div r4{{.*}}encoding: [0x4b,0x44]
; DIS: div r4{{[[:space:]]*$}}
; ENC: divu r4{{.*}}encoding: [0x5b,0x44]
; DIS: divu r4{{[[:space:]]*$}}
; ENC: divl r4{{.*}}encoding: [0x6b,0x44]
; DIS: divl r4{{[[:space:]]*$}}
; ENC: divlu r4{{.*}}encoding: [0x7b,0x44]
; DIS: divlu r4{{[[:space:]]*$}}
; ENC: div r5{{.*}}encoding: [0x4b,0x55]
; DIS: div r5{{[[:space:]]*$}}
; ENC: divu r5{{.*}}encoding: [0x5b,0x55]
; DIS: divu r5{{[[:space:]]*$}}
; ENC: divl r5{{.*}}encoding: [0x6b,0x55]
; DIS: divl r5{{[[:space:]]*$}}
; ENC: divlu r5{{.*}}encoding: [0x7b,0x55]
; DIS: divlu r5{{[[:space:]]*$}}
; ENC: div r6{{.*}}encoding: [0x4b,0x66]
; DIS: div r6{{[[:space:]]*$}}
; ENC: divu r6{{.*}}encoding: [0x5b,0x66]
; DIS: divu r6{{[[:space:]]*$}}
; ENC: divl r6{{.*}}encoding: [0x6b,0x66]
; DIS: divl r6{{[[:space:]]*$}}
; ENC: divlu r6{{.*}}encoding: [0x7b,0x66]
; DIS: divlu r6{{[[:space:]]*$}}
; ENC: div r7{{.*}}encoding: [0x4b,0x77]
; DIS: div r7{{[[:space:]]*$}}
; ENC: divu r7{{.*}}encoding: [0x5b,0x77]
; DIS: divu r7{{[[:space:]]*$}}
; ENC: divl r7{{.*}}encoding: [0x6b,0x77]
; DIS: divl r7{{[[:space:]]*$}}
; ENC: divlu r7{{.*}}encoding: [0x7b,0x77]
; DIS: divlu r7{{[[:space:]]*$}}
; ENC: div r8{{.*}}encoding: [0x4b,0x88]
; DIS: div r8{{[[:space:]]*$}}
; ENC: divu r8{{.*}}encoding: [0x5b,0x88]
; DIS: divu r8{{[[:space:]]*$}}
; ENC: divl r8{{.*}}encoding: [0x6b,0x88]
; DIS: divl r8{{[[:space:]]*$}}
; ENC: divlu r8{{.*}}encoding: [0x7b,0x88]
; DIS: divlu r8{{[[:space:]]*$}}
; ENC: div r9{{.*}}encoding: [0x4b,0x99]
; DIS: div r9{{[[:space:]]*$}}
; ENC: divu r9{{.*}}encoding: [0x5b,0x99]
; DIS: divu r9{{[[:space:]]*$}}
; ENC: divl r9{{.*}}encoding: [0x6b,0x99]
; DIS: divl r9{{[[:space:]]*$}}
; ENC: divlu r9{{.*}}encoding: [0x7b,0x99]
; DIS: divlu r9{{[[:space:]]*$}}
; ENC: div r10{{.*}}encoding: [0x4b,0xaa]
; DIS: div r10{{[[:space:]]*$}}
; ENC: divu r10{{.*}}encoding: [0x5b,0xaa]
; DIS: divu r10{{[[:space:]]*$}}
; ENC: divl r10{{.*}}encoding: [0x6b,0xaa]
; DIS: divl r10{{[[:space:]]*$}}
; ENC: divlu r10{{.*}}encoding: [0x7b,0xaa]
; DIS: divlu r10{{[[:space:]]*$}}
; ENC: div r11{{.*}}encoding: [0x4b,0xbb]
; DIS: div r11{{[[:space:]]*$}}
; ENC: divu r11{{.*}}encoding: [0x5b,0xbb]
; DIS: divu r11{{[[:space:]]*$}}
; ENC: divl r11{{.*}}encoding: [0x6b,0xbb]
; DIS: divl r11{{[[:space:]]*$}}
; ENC: divlu r11{{.*}}encoding: [0x7b,0xbb]
; DIS: divlu r11{{[[:space:]]*$}}
; ENC: div r12{{.*}}encoding: [0x4b,0xcc]
; DIS: div r12{{[[:space:]]*$}}
; ENC: divu r12{{.*}}encoding: [0x5b,0xcc]
; DIS: divu r12{{[[:space:]]*$}}
; ENC: divl r12{{.*}}encoding: [0x6b,0xcc]
; DIS: divl r12{{[[:space:]]*$}}
; ENC: divlu r12{{.*}}encoding: [0x7b,0xcc]
; DIS: divlu r12{{[[:space:]]*$}}
; ENC: div r13{{.*}}encoding: [0x4b,0xdd]
; DIS: div r13{{[[:space:]]*$}}
; ENC: divu r13{{.*}}encoding: [0x5b,0xdd]
; DIS: divu r13{{[[:space:]]*$}}
; ENC: divl r13{{.*}}encoding: [0x6b,0xdd]
; DIS: divl r13{{[[:space:]]*$}}
; ENC: divlu r13{{.*}}encoding: [0x7b,0xdd]
; DIS: divlu r13{{[[:space:]]*$}}
; ENC: div r14{{.*}}encoding: [0x4b,0xee]
; DIS: div r14{{[[:space:]]*$}}
; ENC: divu r14{{.*}}encoding: [0x5b,0xee]
; DIS: divu r14{{[[:space:]]*$}}
; ENC: divl r14{{.*}}encoding: [0x6b,0xee]
; DIS: divl r14{{[[:space:]]*$}}
; ENC: divlu r14{{.*}}encoding: [0x7b,0xee]
; DIS: divlu r14{{[[:space:]]*$}}
; ENC: div r15{{.*}}encoding: [0x4b,0xff]
; DIS: div r15{{[[:space:]]*$}}
; ENC: divu r15{{.*}}encoding: [0x5b,0xff]
; DIS: divu r15{{[[:space:]]*$}}
; ENC: divl r15{{.*}}encoding: [0x6b,0xff]
; DIS: divl r15{{[[:space:]]*$}}
; ENC: divlu r15{{.*}}encoding: [0x7b,0xff]
; DIS: divlu r15{{[[:space:]]*$}}

mul r0, r15
; ENC: mul r0, r15{{.*}}encoding: [0x0b,0x0f]
; DIS: mul r0, r15

mul r15, r0
; ENC: mul r15, r0{{.*}}encoding: [0x0b,0xf0]
; DIS: mul r15, r0

mul r15, r15
; ENC: mul r15, r15{{.*}}encoding: [0x0b,0xff]
; DIS: mul r15, r15

mulu r0, r15
; ENC: mulu r0, r15{{.*}}encoding: [0x1b,0x0f]
; DIS: mulu r0, r15

mulu r15, r0
; ENC: mulu r15, r0{{.*}}encoding: [0x1b,0xf0]
; DIS: mulu r15, r0

mulu r15, r15
; ENC: mulu r15, r15{{.*}}encoding: [0x1b,0xff]
; DIS: mulu r15, r15
