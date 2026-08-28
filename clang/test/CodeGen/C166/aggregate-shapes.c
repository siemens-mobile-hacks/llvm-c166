// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: llvm-objdump -d %t-o0.o | FileCheck %s --check-prefix=O0
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o2.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -fno-inline -mllvm -stop-after=finalize-isel -c %s -o - | FileCheck %s --check-prefix=MIR
// C166-ABI: aggregates.shape_matrix

typedef unsigned char u8;
typedef unsigned int u16;

struct bytes3 {
  u8 bytes[3];
};

struct bytes5 {
  u8 bytes[5];
};

struct bytes8 {
  u8 bytes[8];
};

struct two_words {
  u16 low;
  u16 high;
};

struct nested6 {
  struct two_words pair;
  u16 tail;
};

extern u16 external_bytes3(struct bytes3 value, u16 tail);
extern u16 external_select3(u16 first, u16 second, u16 third,
                            struct bytes3 value, u16 tail);

u16 take_bytes3(struct bytes3 value, u16 tail) {
  return value.bytes[0] + value.bytes[1] + value.bytes[2] + tail;
}

u16 take_bytes5(struct bytes5 value, u16 tail) {
  return value.bytes[0] + value.bytes[4] + tail;
}

u16 take_nested6(struct nested6 value, u16 tail) {
  return value.pair.low + value.pair.high + value.tail + tail;
}

struct bytes3 return_bytes3(u8 first, u8 second, u8 third) {
  struct bytes3 value;
  value.bytes[0] = first;
  value.bytes[1] = second;
  value.bytes[2] = third;
  return value;
}

struct bytes5 return_bytes5(u8 first, u8 fifth) {
  struct bytes5 value;
  value.bytes[0] = first;
  value.bytes[1] = 0;
  value.bytes[2] = 0;
  value.bytes[3] = 0;
  value.bytes[4] = fifth;
  return value;
}

struct bytes8 return_bytes8(u16 seed) {
  struct bytes8 value;
  value.bytes[0] = seed;
  value.bytes[1] = seed + 1;
  value.bytes[2] = seed + 2;
  value.bytes[3] = seed + 3;
  value.bytes[4] = seed + 4;
  value.bytes[5] = seed + 5;
  value.bytes[6] = seed + 6;
  value.bytes[7] = seed + 7;
  return value;
}

struct nested6 return_nested6(u16 low, u16 high, u16 tail) {
  struct nested6 value;
  value.pair.low = low;
  value.pair.high = high;
  value.tail = tail;
  return value;
}

u16 consume_bytes3(u8 first, u8 second, u8 third) {
  struct bytes3 value = return_bytes3(first, second, third);
  return value.bytes[0] + value.bytes[1] + value.bytes[2];
}

u16 consume_bytes5(u8 first, u8 fifth) {
  struct bytes5 value = return_bytes5(first, fifth);
  return value.bytes[0] + value.bytes[4];
}

u16 consume_nested6(u16 low, u16 high, u16 tail) {
  struct nested6 value = return_nested6(low, high, tail);
  return value.pair.low + value.pair.high + value.tail;
}

u16 consume_eight_bytes8(u16 seed) {
  struct bytes8 value0 = return_bytes8(seed + 0);
  struct bytes8 value1 = return_bytes8(seed + 1);
  struct bytes8 value2 = return_bytes8(seed + 2);
  struct bytes8 value3 = return_bytes8(seed + 3);
  struct bytes8 value4 = return_bytes8(seed + 4);
  struct bytes8 value5 = return_bytes8(seed + 5);
  struct bytes8 value6 = return_bytes8(seed + 6);
  struct bytes8 value7 = return_bytes8(seed + 7);
  return value0.bytes[0] + value1.bytes[1] + value2.bytes[2] +
         value3.bytes[3] + value4.bytes[4] + value5.bytes[5] +
         value6.bytes[6] + value7.bytes[7];
}

u16 call_bytes3(u16 seed) {
  struct bytes3 value;
  value.bytes[0] = seed + 1;
  value.bytes[1] = seed + 2;
  value.bytes[2] = seed + 3;
  return external_bytes3(value, seed ^ 0x55aaU);
}

// Keep a select live while preparing a later stack call.  SelectionDAG may
// expand it into new machine basic blocks inside the call sequence; those
// blocks must inherit the complete outgoing-frame displacement.
u16 call_bytes3_after_select(u16 seed, u16 selection) {
  struct bytes3 first;
  struct bytes3 second;
  first.bytes[0] = seed + 1;
  first.bytes[1] = seed + 2;
  first.bytes[2] = seed + 3;
  second.bytes[0] = seed + 4;
  second.bytes[1] = seed + 5;
  second.bytes[2] = seed + 6;
  u16 first_result = external_bytes3(first, seed ^ 0x1111U);
  u16 selected = selection == 1 ? first_result : seed;
  u16 second_result = external_select3(seed + 0x100U, seed + 0x200U,
                                       seed + 0x300U, second,
                                       seed ^ 0x2222U);
  return selection == 2 ? second_result : selected;
}

// The call result is now glued through stack cleanup before either select is
// expanded.  Thus no custom-inserter block inherits a live outgoing frame,
// and both six-byte frames are released immediately after their calls.
// MIR-LABEL: name: call_bytes3_after_select
// MIR:       ADJCALLSTACKDOWN 6,
// MIR:       CALLS @external_bytes3
// MIR-NEXT:  ADJSP 6,
// MIR-NEXT:  ADJCALLSTACKUP 6,
// MIR:       bb.1.entry:
// MIR:       bb.2.entry:
// MIR:       ADJCALLSTACKDOWN 6,
// MIR:       CALLS @external_select3
// MIR-NEXT:  ADJSP 6,
// MIR-NEXT:  ADJCALLSTACKUP 6,
// MIR:       bb.3.entry:
// MIR:       bb.4.entry:

// Stack-passed aggregates are rounded to a word boundary before the following
// scalar: sizes 3, 5 and 6 place it at offsets 4, 6 and 6 respectively.
// CHECK-LABEL: <_take_bytes3>:
// CHECK-DAG:   movb {{r[lh][0-7]}}, [r0]
// CHECK-DAG:   movb {{r[lh][0-7]}}, [r0 + #1]
// CHECK-DAG:   movb {{r[lh][0-7]}}, [r0 + #2]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #4]
// CHECK:       rets
// CHECK-LABEL: <_take_bytes5>:
// CHECK-DAG:   movb {{r[lh][0-7]}}, [r0]
// CHECK-DAG:   movb {{r[lh][0-7]}}, [r0 + #4]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #6]
// CHECK:       rets
// CHECK-LABEL: <_take_nested6>:
// CHECK-DAG:   mov {{r[0-9]+}}, [r0]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #2]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #4]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #6]
// CHECK:       rets

// The optimized callee can write directly into the caller-reserved block at
// R0. The raw user-stack near address is returned in R4; it must
// not be converted to the low word of LLVM's internal DPP-based far pointer.
// CHECK-LABEL: <_return_bytes3>:
// CHECK:       mov r4, r0
// CHECK-NOT:   and r4
// CHECK-NOT:   mov r5, dpp1
// CHECK:       movb [r0], {{r[lh][0-7]}}
// CHECK:       rets
// CHECK-LABEL: <_return_bytes5>:
// CHECK:       mov r4, r0
// CHECK-NOT:   and r4
// CHECK-NOT:   mov r5, dpp1
// CHECK:       movb [r0], {{r[lh][0-7]}}
// CHECK:       rets
// CHECK-LABEL: <_return_nested6>:
// CHECK:       mov r4, r0
// CHECK-NOT:   and r4
// CHECK-NOT:   mov r5, dpp1
// CHECK:       mov [r0], r12
// CHECK:       rets

// Caller-side cleanup proves the rounded caller-reserved sizes: 3 -> 4,
// 5 -> 6, and nested 6 -> 6 bytes.
// CHECK-LABEL: <_consume_bytes3>:
// CHECK:       sub r0, #4
// CHECK:       calls
// CHECK:       add r0, #4
// CHECK:       rets
// CHECK-LABEL: <_consume_bytes5>:
// CHECK:       sub r0, #6
// CHECK:       calls
// CHECK:       add r0, #6
// CHECK:       rets
// CHECK-LABEL: <_consume_nested6>:
// CHECK:       sub r0, #6
// CHECK:       calls
// CHECK:       add r0, #6
// CHECK:       rets

// Outgoing stack arguments are laid out exactly as reverse predecrement
// pushes, but R0 is adjusted for the complete six-byte area first.  This keeps
// LLVM's call-frame displacement exact even if scheduling introduces a block
// boundary while arguments are being prepared.
// CHECK-LABEL: <_call_bytes3>:
// CHECK:       sub r0, #6
// CHECK:       mov [r0], {{r[0-9]+}}
// CHECK:       mov [r0 + #2], {{r[0-9]+}}
// CHECK:       mov [r0 + #4], {{r[0-9]+}}
// CHECK:       calls
// CHECK:       add r0, #6
// CHECK:       rets

// Under O0 register pressure, every caller-reserved eight-byte block must be
// allocated by changing physical R0 directly.  A virtual stack-pointer value
// may otherwise be spilled into that block and overwritten by the callee.
// O0-LABEL: <_consume_eight_bytes8>:
// O0-COUNT-6: calls
// O0:         sub r0, #6
// O0-NEXT:    sub r0, #2
// O0-NEXT:    calls
// O0:         sub r0, #6
// O0-NEXT:    sub r0, #2
// O0-NEXT:    calls
// O0:         rets
