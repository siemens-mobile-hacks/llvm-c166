// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: llvm-objdump -d %t-o0.o | FileCheck %s --check-prefix=OFFSET
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t-o1.o
// RUN: llvm-objdump -d %t-o1.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t-o2.o
// C166-ABI: stack.fixed_frame

extern unsigned int callee_word(unsigned int);
extern unsigned long callee_long(unsigned long);
extern unsigned int callee_five(unsigned int, unsigned int, unsigned int,
                                unsigned int, unsigned int);

unsigned int local_word(unsigned int value) {
  volatile unsigned int local = value;
  return local;
}

unsigned int local_array(unsigned int value) {
  volatile unsigned int words[3];
  words[0] = value;
  words[1] = value + 1;
  words[2] = value + 2;
  return words[0] + words[1] + words[2];
}

unsigned int live_across_call(unsigned int a, unsigned int b,
                              unsigned int c, unsigned int d) {
  return callee_word(a) + a + b + c + d + 10;
}

unsigned long long_across_call(unsigned long value) {
  return value + callee_long(value);
}

unsigned int stack_arg_with_local(unsigned int a, unsigned int b,
                                  unsigned int c, unsigned int d,
                                  unsigned int e) {
  volatile unsigned int local = e;
  return local;
}

unsigned int nested_stack_and_locals(unsigned int a0, unsigned int a1,
                                     unsigned int a2, unsigned int a3,
                                     unsigned int a4, unsigned int a5,
                                     unsigned int a6, unsigned int a7) {
  volatile unsigned int local0 = a6;
  volatile unsigned int local1 = a7;
  unsigned int left = callee_five(a0, a1, a2, a3, local0);
  unsigned int right = callee_five(a4, a5, local1, left, a0);

  if (callee_word(right) == 0)
    return left + local0;
  if (right & 1)
    return right + local1;
  return left + right + local0 + local1;
}

struct frame_offsets {
  unsigned int at_0;
  unsigned int at_2;
  unsigned char pad_4[2];
  unsigned int at_6;
  unsigned int at_8;
  unsigned char pad_10[4];
  unsigned int at_14;
  unsigned char pad_16[240];
  unsigned int at_256;
};

unsigned int boundary_offsets(unsigned int value) {
  volatile struct frame_offsets frame;
  frame.at_0 = value;
  frame.at_2 = value + 2;
  frame.at_6 = value + 6;
  frame.at_8 = value + 8;
  frame.at_14 = value + 14;
  frame.at_256 = value + 256;
  return frame.at_0 + frame.at_2 + frame.at_6 + frame.at_8 + frame.at_14 +
         frame.at_256;
}

// CHECK-LABEL: <_local_word>:
// CHECK:       sub r0, #2
// CHECK-NEXT:  mov [r0], r12
// CHECK-NEXT:  mov r4, [r0]
// CHECK-NEXT:  add r0, #2
// CHECK-NEXT:  rets

// CHECK-LABEL: <_local_array>:
// CHECK:       sub r0, #6
// CHECK:       mov [r0 + #{{[024]}}], r{{[0-9]+}}
// CHECK:       mov r{{[0-9]+}}, [r0 + #{{[024]}}]
// CHECK:       add r0, #6
// CHECK-NEXT:  rets

// CHECK-LABEL: <_live_across_call>:
// CHECK:       sub r0, #2
// CHECK-NEXT:  mov [r0], r6
// CHECK:       calls
// CHECK:       mov r6, [r0]
// CHECK-NEXT:  add r0, #2
// CHECK-NEXT:  rets

// CHECK-LABEL: <_long_across_call>:
// CHECK:       sub r0, #4
// CHECK-NEXT:  mov [r0 + #2], r6
// CHECK-NEXT:  mov [r0], r7
// CHECK:       calls
// CHECK:       add r4, r6
// CHECK-NEXT:  addc r5, r7
// CHECK-NEXT:  mov r7, [r0]
// CHECK-NEXT:  mov r6, [r0 + #2]
// CHECK-NEXT:  add r0, #4
// CHECK-NEXT:  rets

// CHECK-LABEL: <_stack_arg_with_local>:
// CHECK:       sub r0, #2
// CHECK-NEXT:  mov [[ARG:r[0-9]+]], [r0 + #2]
// CHECK-NEXT:  mov [r0], [[ARG]]
// CHECK-NEXT:  mov r4, [r0]
// CHECK-NEXT:  add r0, #2
// CHECK-NEXT:  rets

// Three calls cover nested outgoing stack arguments while incoming stack
// arguments, volatile locals and callee-saved values remain live.
// CHECK-LABEL: <_nested_stack_and_locals>:
// CHECK-DAG:   mov [r0 + #{{[0-9]+}}], r6
// CHECK-DAG:   mov [r0 + #{{[0-9]+}}], r7
// CHECK:       calls
// CHECK:       add r0, #2
// CHECK:       calls
// CHECK:       add r0, #2
// CHECK:       calls
// CHECK-DAG:   mov r7, [r0 + #{{[0-9]+}}]
// CHECK-DAG:   mov r6, [r0 + #{{[0-9]+}}]
// CHECK:       rets

// At O0 the volatile aggregate remains one 258-byte frame object, so these
// checks exercise both compact and full 16-bit user-stack displacements.
// OFFSET-LABEL: <_boundary_offsets>:
// OFFSET:       mov r1, #260
// OFFSET-NEXT:  sub r0, r1
// OFFSET:       mov [r0], r{{[0-9]+}}
// OFFSET:       mov [r0 + #2], r{{[0-9]+}}
// OFFSET:       mov [r0 + #6], r{{[0-9]+}}
// OFFSET:       mov [r0 + #8], r{{[0-9]+}}
// OFFSET:       mov [r0 + #14], r{{[0-9]+}}
// OFFSET:       mov [r0 + #256], r{{[0-9]+}}
// OFFSET:       mov r{{[0-9]+}}, [r0]
// OFFSET:       mov r{{[0-9]+}}, [r0 + #2]
// OFFSET:       mov r{{[0-9]+}}, [r0 + #6]
// OFFSET:       mov r{{[0-9]+}}, [r0 + #8]
// OFFSET:       mov r{{[0-9]+}}, [r0 + #14]
// OFFSET:       mov r{{[0-9]+}}, [r0 + #256]
// OFFSET:       mov r1, #260
// OFFSET-NEXT:  add r0, r1
// OFFSET-NEXT:  rets
