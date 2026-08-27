// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// C166-ABI: aggregates.argument_stack
// C166-ABI: aggregates.argument_stop_rule
// C166-ABI: aggregates.caller_reserved_result
// C166-ABI: aggregates.result_with_stack_tail

struct two_words {
  unsigned int low;
  unsigned int high;
};

struct one_word {
  unsigned int value;
};

struct four_words {
  unsigned int first;
  unsigned int second;
  unsigned int third;
  unsigned int fourth;
};

__attribute__((noinline))
unsigned int take_two(struct two_words value) {
  return value.low + value.high;
}

unsigned int mixed_aggregate(unsigned int head, struct two_words value,
                             unsigned int tail) {
  return head + value.low + value.high + tail;
}

unsigned int call_take_two(unsigned int low, unsigned int high) {
  struct two_words value;
  value.low = low;
  value.high = high;
  return take_two(value);
}

__attribute__((noinline))
struct two_words return_two(unsigned int low, unsigned int high) {
  struct two_words value;
  value.low = low;
  value.high = high;
  return value;
}

unsigned int consume_return_two(unsigned int low, unsigned int high) {
  struct two_words value = return_two(low, high);
  return value.low + value.high;
}

__attribute__((noinline))
struct one_word return_one_with_stack(unsigned int first,
                                      unsigned int second,
                                      unsigned int third,
                                      unsigned int fourth,
                                      unsigned int fifth) {
  struct one_word value;
  value.value = first + second + third + fourth + fifth;
  return value;
}

unsigned int consume_return_one_with_stack(unsigned int first,
                                           unsigned int second,
                                           unsigned int third,
                                           unsigned int fourth,
                                           unsigned int fifth) {
  struct one_word value =
      return_one_with_stack(first, second, third, fourth, fifth);
  return value.value;
}

__attribute__((noinline))
struct four_words return_four_with_stack_tail(
    unsigned int first, unsigned int second, unsigned int third,
    unsigned int fourth, unsigned int fifth, unsigned int sixth,
    unsigned int seventh, unsigned int eighth) {
  struct four_words value;
  value.first = first + fifth;
  value.second = second + sixth;
  value.third = third + seventh;
  value.fourth = fourth + eighth;
  return value;
}

unsigned int consume_return_four_with_stack_tail(
    unsigned int first, unsigned int second, unsigned int third,
    unsigned int fourth, unsigned int fifth, unsigned int sixth,
    unsigned int seventh, unsigned int eighth) {
  struct four_words value = return_four_with_stack_tail(
      first, second, third, fourth, fifth, sixth, seventh, eighth);
  return value.first ^ value.second ^ value.third ^ value.fourth;
}

// C166 passes every aggregate by value on the user stack. Encountering
// one also activates the normal stop rule for every following argument.
// CHECK-LABEL: <_take_two>:
// CHECK:       mov {{r[0-9]+}}, [r0]
// CHECK:       mov r4, [r0 + #2]
// CHECK:       rets
// CHECK-LABEL: <_mixed_aggregate>:
// CHECK:       mov r4, [r0 + #4]
// CHECK:       add r4, r12
// CHECK:       mov {{r[0-9]+}}, [r0]
// CHECK:       mov {{r[0-9]+}}, [r0 + #2]
// CHECK:       rets
// CHECK-LABEL: <_call_take_two>:
// CHECK:       sub r0, #4
// CHECK:       mov [r0], {{r[0-9]+}}
// CHECK:       mov [r0 + #2], {{r[0-9]+}}
// CHECK:       calls
// CHECK:       add r0, #4
// CHECK:       rets
// CHECK-LABEL: <_return_two>:
// CHECK:       mov r4, r0
// CHECK:       mov [r0], r12
// CHECK:       mov [r0 + #2], r13
// CHECK:       rets
// CHECK-LABEL: <_consume_return_two>:
// CHECK:       calls
// CHECK:       mov {{r[0-9]+}}, [r4]
// CHECK:       mov {{r[0-9]+}}, [r4 + #2]
// CHECK:       add r0, #4
// CHECK:       rets
// CHECK-LABEL: <_return_one_with_stack>:
// CHECK:       mov {{r[0-9]+}}, [r0]
// CHECK:       mov r4, r0
// CHECK:       add r4, #2
// CHECK:       mov [r0 + #2], {{r[0-9]+}}
// CHECK:       rets
// CHECK-LABEL: <_consume_return_one_with_stack>:
// CHECK:       sub r0, #4
// CHECK:       mov [r0], {{r[0-9]+}}
// CHECK:       calls
// CHECK:       mov {{r[0-9]+}}, [r4]
// CHECK:       add r0, #4
// CHECK:       rets

// The final user-stack layout for a call returning eight aggregate bytes and
// passing four stack words is [arguments:8][result:8].  LLVM allocates that
// complete sixteen-byte outgoing frame before the fixed-offset stores.  This
// is externally equivalent to result reservation followed by four
// reverse predecrement pushes.
// CHECK-LABEL: <_return_four_with_stack_tail>:
// CHECK-DAG:   mov {{r[0-9]+}}, [r0]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #2]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #4]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #6]
// CHECK-DAG:   mov [r0 + #8], {{r[0-9]+}}
// CHECK-DAG:   mov [r0 + #10], {{r[0-9]+}}
// CHECK-DAG:   mov [r0 + #12], {{r[0-9]+}}
// CHECK-DAG:   mov [r0 + #14], {{r[0-9]+}}
// CHECK:       rets
// CHECK-LABEL: <_consume_return_four_with_stack_tail>:
// CHECK:       mov {{r[0-9]+}}, [r0 + #8]
// CHECK-NEXT:  sub r0, #6
// CHECK-NEXT:  sub r0, #6
// CHECK-NEXT:  sub r0, #4
// CHECK:       mov [r0], {{r[0-9]+}}
// CHECK:       mov [r0 + #2], {{r[0-9]+}}
// CHECK:       mov [r0 + #4], {{r[0-9]+}}
// CHECK:       mov [r0 + #6], {{r[0-9]+}}
// CHECK:       calls
// CHECK:       mov {{r[0-9]+}}, [r4]
// CHECK:       mov {{r[0-9]+}}, [r4 + #2]
// CHECK:       mov {{r[0-9]+}}, [r4 + #4]
// CHECK:       mov r4, [r4 + #6]
// CHECK:       add r0, #6
// CHECK-NEXT:  add r0, #6
// CHECK-NEXT:  add r0, #4
// CHECK:       rets
