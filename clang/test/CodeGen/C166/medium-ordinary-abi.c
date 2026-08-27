// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -dr %t.o | FileCheck %s --check-prefixes=CHECK,NEAR
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o0.o

#include <stdarg.h>

typedef unsigned int u16;
typedef unsigned long u32;

// C166-ABI: medium.ordinary.scalar_slots
u16 scalar_w0(u16 value) { return value; }
u16 scalar_w1(u16 p0, u16 value) { return value; }
u16 scalar_w2(u16 p0, u16 p1, u16 value) { return value; }
u16 scalar_w3(u16 p0, u16 p1, u16 p2, u16 value) { return value; }
u16 scalar_w4(u16 p0, u16 p1, u16 p2, u16 p3, u16 value) { return value; }

u32 scalar_d0(u32 value) { return value; }
u32 scalar_d1(u16 p0, u32 value) { return value; }
u32 scalar_d2(u16 p0, u16 p1, u32 value) { return value; }
u32 scalar_d3(u16 p0, u16 p1, u16 p2, u32 value) { return value; }
u32 scalar_d4(u16 p0, u16 p1, u16 p2, u16 p3, u32 value) { return value; }

u16 scalar_d3_tail(u16 p0, u16 p1, u16 p2, u32 value, u16 tail) {
  return tail;
}

// C166-ABI: medium.ordinary.scalar_arities
u16 arity0(void) { return 0x1660U; }
u16 arity1(u16 a0) { return a0; }
u16 arity2(u16 a0, u16 a1) { return a1; }
u16 arity3(u16 a0, u16 a1, u16 a2) { return a2; }
u16 arity4(u16 a0, u16 a1, u16 a2, u16 a3) { return a3; }
u16 arity5(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4) { return a4; }
u16 arity6(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5) {
  return a5;
}
u16 arity7(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6) {
  return a6;
}
u16 arity8(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6,
           u16 a7) {
  return a7;
}
u16 arity9(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6,
           u16 a7, u16 a8) {
  return a8;
}
u16 arity10(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6,
            u16 a7, u16 a8, u16 a9) {
  return a9;
}

struct two_words {
  u16 low;
  u16 high;
};

struct four_words {
  u16 first;
  u16 second;
  u16 third;
  u16 fourth;
};

// C166-ABI: medium.ordinary.aggregates
__attribute__((noinline)) u16 take_two(struct two_words value) {
  return value.low + value.high;
}

u16 mixed_aggregate(u16 head, struct two_words value, u16 tail) {
  return head + value.low + value.high + tail;
}

__attribute__((noinline)) struct two_words return_two(u16 low, u16 high) {
  struct two_words value = {low, high};
  return value;
}

u16 consume_return_two(u16 low, u16 high) {
  struct two_words value = return_two(low, high);
  return value.low + value.high;
}

__attribute__((noinline)) struct four_words return_four_with_stack_tail(
    u16 first, u16 second, u16 third, u16 fourth, u16 fifth, u16 sixth,
    u16 seventh, u16 eighth) {
  struct four_words value;
  value.first = first + fifth;
  value.second = second + sixth;
  value.third = third + seventh;
  value.fourth = fourth + eighth;
  return value;
}

u16 consume_return_four_with_stack_tail(
    u16 first, u16 second, u16 third, u16 fourth, u16 fifth, u16 sixth,
    u16 seventh, u16 eighth) {
  struct four_words value = return_four_with_stack_tail(
      first, second, third, fourth, fifth, sixth, seventh, eighth);
  return value.first ^ value.second ^ value.third ^ value.fourth;
}

// C166-ABI: medium.ordinary.varargs
__attribute__((noinline)) u16 sum_words(u16 count, ...) {
  va_list args;
  u16 result = 0;
  va_start(args, count);
  while (count--)
    result += va_arg(args, unsigned int);
  va_end(args);
  return result;
}

__attribute__((noinline)) u32 take_long(u16 tag, ...) {
  va_list args;
  va_start(args, tag);
  u32 value = va_arg(args, unsigned long);
  va_end(args);
  return value;
}

__attribute__((noinline)) u16 *take_pointer(u16 tag, ...) {
  va_list args;
  va_start(args, tag);
  u16 *value = va_arg(args, u16 *);
  va_end(args);
  return value;
}

u16 call_sum_words(void) { return sum_words(3U, 10U, 20U, 30U); }
u32 call_take_long(u32 value) { return take_long(1U, value); }
u16 *call_take_pointer(u16 *value) { return take_pointer(1U, value); }

struct va_pair {
  u16 first;
  u16 second;
};

// C166-ABI: medium.ordinary.varargs_aggregates
__attribute__((noinline)) u16 take_va_pair(u16 tag, ...) {
  va_list args;
  va_start(args, tag);
  struct va_pair value = va_arg(args, struct va_pair);
  u16 tail = va_arg(args, unsigned int);
  va_end(args);
  return tag + value.first + value.second + tail;
}

u16 call_va_pair(u16 tag, struct va_pair value, u16 tail) {
  return take_va_pair(tag, value, tail);
}

// C166-ABI: medium.ordinary.recursion
u32 medium_recursive(u16 depth, u16 head, u32 value, u16 tail) {
  volatile u16 canary[3];
  canary[0] = head ^ 0x1357U;
  canary[1] = tail ^ 0x2468U;
  canary[2] = depth ^ 0x55aaU;
  if (depth != 0U)
    value = medium_recursive(depth - 1U, head + 0x101U,
                             value ^ 0x1660a55aUL, tail + 0x202U);
  return value + canary[0] + canary[1] + canary[2];
}

// NEAR: file format elf32-c166
// NEAR-NOT: rets
// NEAR-NOT: R_C166_SEG8

// CHECK-LABEL: <_scalar_w0>:
// CHECK:       mov r4, r12
// CHECK-NEXT:  ret
// CHECK-LABEL: <_scalar_w1>:
// CHECK:       mov r4, r13
// CHECK-NEXT:  ret
// CHECK-LABEL: <_scalar_w2>:
// CHECK:       mov r4, r14
// CHECK-NEXT:  ret
// CHECK-LABEL: <_scalar_w3>:
// CHECK:       mov r4, r15
// CHECK-NEXT:  ret
// CHECK-LABEL: <_scalar_w4>:
// CHECK:       mov r4, [r0]
// CHECK-NEXT:  ret
// CHECK-LABEL: <_scalar_d0>:
// CHECK:       mov r4, r12
// CHECK-NEXT:  mov r5, r13
// CHECK-NEXT:  ret
// CHECK-LABEL: <_scalar_d1>:
// CHECK:       mov r4, r13
// CHECK-NEXT:  mov r5, r14
// CHECK-NEXT:  ret
// CHECK-LABEL: <_scalar_d2>:
// CHECK:       mov r4, r14
// CHECK-NEXT:  mov r5, r15
// CHECK-NEXT:  ret
// CHECK-LABEL: <_scalar_d3>:
// CHECK:       mov r4, [r0]
// CHECK-NEXT:  mov r5, [r0 + #2]
// CHECK-NEXT:  ret
// CHECK-LABEL: <_scalar_d4>:
// CHECK:       mov r4, [r0]
// CHECK-NEXT:  mov r5, [r0 + #2]
// CHECK-NEXT:  ret
// CHECK-LABEL: <_scalar_d3_tail>:
// CHECK:       mov r4, [r0 + #4]
// CHECK-NEXT:  ret

// CHECK-LABEL: <_arity0>:
// CHECK:       mov r4, #5728
// CHECK-NEXT:  ret
// CHECK-LABEL: <_arity1>:
// CHECK:       mov r4, r12
// CHECK-NEXT:  ret
// CHECK-LABEL: <_arity2>:
// CHECK:       mov r4, r13
// CHECK-NEXT:  ret
// CHECK-LABEL: <_arity3>:
// CHECK:       mov r4, r14
// CHECK-NEXT:  ret
// CHECK-LABEL: <_arity4>:
// CHECK:       mov r4, r15
// CHECK-NEXT:  ret
// CHECK-LABEL: <_arity5>:
// CHECK:       mov r4, [r0]
// CHECK-NEXT:  ret
// CHECK-LABEL: <_arity6>:
// CHECK:       mov r4, [r0 + #2]
// CHECK-NEXT:  ret
// CHECK-LABEL: <_arity7>:
// CHECK:       mov r4, [r0 + #4]
// CHECK-NEXT:  ret
// CHECK-LABEL: <_arity8>:
// CHECK:       mov r4, [r0 + #6]
// CHECK-NEXT:  ret
// CHECK-LABEL: <_arity9>:
// CHECK:       mov r4, [r0 + #8]
// CHECK-NEXT:  ret
// CHECK-LABEL: <_arity10>:
// CHECK:       mov r4, [r0 + #10]
// CHECK-NEXT:  ret

// CHECK-LABEL: <_take_two>:
// CHECK:       mov {{r[0-9]+}}, [r0]
// CHECK:       mov r4, [r0 + #2]
// CHECK:       ret
// CHECK-LABEL: <_mixed_aggregate>:
// CHECK:       mov r4, [r0 + #4]
// CHECK:       mov {{r[0-9]+}}, [r0]
// CHECK:       mov {{r[0-9]+}}, [r0 + #2]
// CHECK:       ret
// CHECK-LABEL: <_return_two>:
// CHECK:       mov r4, r0
// CHECK:       mov [r0], r12
// CHECK:       mov [r0 + #2], r13
// CHECK:       ret
// CHECK-LABEL: <_consume_return_two>:
// CHECK:       calla
// CHECK-NEXT:  {{.*}}R_C166_COF16{{[[:space:]]+}}_return_two
// CHECK:       mov {{r[0-9]+}}, [r4]
// CHECK:       mov {{r[0-9]+}}, [r4 + #2]
// CHECK:       add r0, #4
// CHECK:       ret
// CHECK-LABEL: <_return_four_with_stack_tail>:
// CHECK-DAG:   mov [r0 + #8], {{r[0-9]+}}
// CHECK-DAG:   mov [r0 + #10], {{r[0-9]+}}
// CHECK-DAG:   mov [r0 + #12], {{r[0-9]+}}
// CHECK-DAG:   mov [r0 + #14], {{r[0-9]+}}
// CHECK:       ret
// CHECK-LABEL: <_consume_return_four_with_stack_tail>:
// CHECK:       calla
// CHECK-NEXT:  {{.*}}R_C166_COF16{{[[:space:]]+}}_return_four_with_stack_tail
// CHECK:       mov {{r[0-9]+}}, [r4]
// CHECK:       mov {{r[0-9]+}}, [r4 + #2]
// CHECK:       mov {{r[0-9]+}}, [r4 + #4]
// CHECK:       mov r4, [r4 + #6]
// CHECK:       ret

// CHECK-LABEL: <_sum_words>:
// CHECK:       mov {{r[0-9]+}}, dpp1
// CHECK:       extp {{r[0-9]+}}, #1
// CHECK:       ret
// CHECK-LABEL: <_take_long>:
// CHECK:       mov {{r[0-9]+}}, dpp1
// CHECK:       extp {{r[0-9]+}}, #2
// CHECK:       mov r4, [{{r[0-9]+}}]
// CHECK:       mov r5, [{{r[0-9]+}} + #2]
// CHECK:       ret
// CHECK-LABEL: <_take_pointer>:
// CHECK:       mov {{r[0-9]+}}, dpp1
// CHECK:       extp {{r[0-9]+}}, #2
// CHECK:       mov r4, [{{r[0-9]+}}]
// CHECK:       mov r5, [{{r[0-9]+}} + #2]
// CHECK:       ret
// CHECK-LABEL: <_call_sum_words>:
// CHECK:       sub r0, #6
// CHECK:       mov [r0], {{r[0-9]+}}
// CHECK:       mov [r0 + #2], {{r[0-9]+}}
// CHECK:       mov [r0 + #4], {{r[0-9]+}}
// CHECK:       calla
// CHECK-NEXT:  {{.*}}R_C166_COF16{{[[:space:]]+}}_sum_words
// CHECK:       add r0, #6
// CHECK:       ret
// CHECK-LABEL: <_take_va_pair>:
// CHECK:       mov {{r[0-9]+}}, dpp1
// CHECK:       extp {{r[0-9]+}}, #1
// CHECK:       ret
// CHECK-LABEL: <_call_va_pair>:
// CHECK:       sub r0, #6
// CHECK:       mov [r0], {{r[0-9]+}}
// CHECK:       mov [r0 + #2], {{r[0-9]+}}
// CHECK:       mov [r0 + #4], {{r[0-9]+}}
// CHECK:       calla
// CHECK-NEXT:  {{.*}}R_C166_COF16{{[[:space:]]+}}_take_va_pair
// CHECK:       add r0, #6
// CHECK:       ret

// CHECK-LABEL: <_medium_recursive>:
// CHECK:       sub r0, #6
// CHECK:       mov {{r[0-9]+}}, [r0 + #6]
// CHECK:       sub r0, #2
// CHECK:       mov [r0], {{r[0-9]+}}
// CHECK:       calla
// CHECK-NEXT:  {{.*}}R_C166_COF16{{[[:space:]]+}}_medium_recursive
// CHECK:       add r0, #2
// CHECK:       add r0, #6
// CHECK:       ret
