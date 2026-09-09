// RUN: %clang --target=c166 -c %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s
// RUN: %clang --target=c166 -E %s -o %t.s
// RUN: %clang --target=c166 -c %t.s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s

#define END_INIT einit
.text
.globl startup
startup:
  diswdt
  mov r4, #-1
  END_INIT
  idle

// CHECK: a5 5a a5 a5{{.*}}diswdt
// CHECK: e6 f4 ff ff{{.*}}mov r4, #65535
// CHECK: b5 4a b5 b5{{.*}}einit
// CHECK: 87 78 87 87{{.*}}idle
