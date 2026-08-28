// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -s -d -r %t.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o

volatile float global_float;
volatile float initialized_float = 1.0f;
volatile struct {
  unsigned int head;
  float value;
  unsigned long tail;
} initialized_record = {0x1357U, -2.5f, 0x89abcdefUL};
volatile float initialized_array[3] = {0.5f, -0.0f, 0x1p-149f};

float load_global_float(void) { return global_float; }
void store_global_float(float value) { global_float = value; }
float load_float_pointer(volatile float *value) { return *value; }
void store_float_pointer(volatile float *where, float value) { *where = value; }
float load_float_array(unsigned int index) { return initialized_array[index]; }
void store_float_array(unsigned int index, float value) {
  initialized_array[index] = value;
}

// C166 binary32 storage is word-mixed: each 16-bit word is little-endian,
// but the IEEE most-significant word occupies the lower address.  Integer
// long storage remains low-word first.  The bytes below therefore encode
// 1.0f, then {0x1357, -2.5f, 0x89abcdef}, then the float array
// {0.5f, -0.0f, minimum-subnormal}.
// CHECK:      Contents of section .data:
// CHECK-NEXT:  0000 803f0000 571320c0 0000efcd ab89003f
// CHECK-NEXT:  0010 00000080 00000000 0100

// A public float return is R4=MSW, R5=LSW, so a direct load from C166
// storage keeps the lower-address word in R4.
// CHECK-LABEL: <_load_global_float>:
// CHECK:       mov r4, 0
// CHECK-NEXT:  {{.*}}R_C166_POF14 _global_float
// CHECK:       mov r5, 0
// CHECK-NEXT:  {{.*}}R_C166_POF14 _global_float+0x2
// CHECK:       rets

// The incoming public float is stack-only, MSW first.  Storing it preserves
// that physical word order rather than the low-word-first integer order.
// CHECK-LABEL: <_store_global_float>:
// CHECK:       mov r1, [r0]
// CHECK:       mov r3, [r0 + #2]
// CHECK:       mov 0, r2
// CHECK-NEXT:  {{.*}}R_C166_POF14 _global_float
// CHECK:       mov 0, r3
// CHECK-NEXT:  {{.*}}R_C166_POF14 _global_float+0x2
// CHECK:       rets

// CHECK-LABEL: <_load_float_pointer>:
// CHECK:       mov r4, [r12]
// CHECK:       mov r5, [r12 + #2]
// CHECK:       rets

// CHECK-LABEL: <_store_float_pointer>:
// CHECK:       mov [r12 + #2], r3
// CHECK:       mov [r12], r2
// CHECK:       rets
