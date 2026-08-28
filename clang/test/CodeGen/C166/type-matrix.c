// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// C166-ABI: args.unsigned_char_slots
// C166-ABI: args.signed_char_slots
// C166-ABI: args.two_word_with_one_slot_left
// C166-ABI: args.enum_slots
// C166-ABI: aggregates.union_layout_and_stop
// C166-ABI: aggregates.padded_layout_and_stop
// C166-ABI: aggregates.bitfield_layout_and_stop
// C166-ABI: varargs.signed_char_promotion
// C166-ABI: varargs.unsigned_char_promotion
// C166-ABI: varargs.enum_promotion

typedef signed char s8;
typedef unsigned char u8;
typedef signed int s16;
typedef unsigned int u16;
typedef unsigned long u32;

enum matrix_enum {
  MATRIX_ZERO = 0,
  MATRIX_THREE_HUNDRED = 300
};

union matrix_union {
  u32 whole;
  u16 words[2];
};

struct padded_bytes {
  u8 first;
  u16 second;
};

struct bitfield_word {
  unsigned int low : 3;
  unsigned int high : 5;
  unsigned int tail : 8;
};

u16 matrix_five_u8(u8 a, u8 b, u8 c, u8 d, u8 e) {
  return (u16)a + b + c + d + e;
}

s16 matrix_five_s8(s8 a, s8 b, s8 c, s8 d, s8 e) {
  return (s16)a + b + c + d + e;
}

u16 matrix_packed_u8_u32(u8 first, u32 pair, u8 last) {
  return (u16)first + (u16)pair + last;
}

u16 matrix_pair_with_one_slot(u16 a, u16 b, u16 c, u32 pair, u16 tail) {
  return a + b + c + (u16)pair + tail;
}

u16 matrix_five_enum(enum matrix_enum a, enum matrix_enum b,
                     enum matrix_enum c, enum matrix_enum d,
                     enum matrix_enum e) {
  return (u16)a + (u16)b + (u16)c + (u16)d + (u16)e;
}

u16 matrix_union_then_word(union matrix_union value, u16 tail) {
  return value.words[0] + value.words[1] + tail;
}

u16 matrix_padded_then_word(struct padded_bytes value, u16 tail) {
  return value.first + value.second + tail;
}

u16 matrix_bitfield_then_word(struct bitfield_word value, u16 tail) {
  return value.low + value.high + value.tail + tail;
}

extern u16 matrix_variadic_sink(u16 tag, ...);

u16 matrix_promote_s8(s8 value) {
  return matrix_variadic_sink(1, value);
}

u16 matrix_promote_u8(u8 value) {
  return matrix_variadic_sink(1, value);
}

u16 matrix_promote_enum(enum matrix_enum value) {
  return matrix_variadic_sink(1, value);
}

// Character arguments consume one word-sized register or stack slot.  The
// fifth byte is at [R0], while the first four arrive through R12-R15.
// CHECK-LABEL: <_matrix_five_u8>:
// CHECK:       movb r{{[lh][0-7]}}, [r0]
// CHECK-NEXT:  movbz {{r[0-9]+}}, r{{[lh][0-7]}}
// CHECK:       rets
// CHECK-LABEL: <_matrix_five_s8>:
// CHECK:       movb r{{[lh][0-7]}}, [r0]
// CHECK-NEXT:  movbs {{r[0-9]+}}, r{{[lh][0-7]}}
// CHECK:       rets

// A two-word scalar is packed into R13:R14 after a byte in R12.  It does not
// require an even register pair, so the final byte still occupies R15.
// CHECK-LABEL: <_matrix_packed_u8_u32>:
// CHECK:       mov r4, r13
// CHECK-NEXT:  mov r5, r14
// CHECK:       and r15, {{r[0-9]+}}
// CHECK-NOT:   [r0]
// CHECK:       rets

// With only R15 free, a two-word argument starts the stack-stop rule: its low
// word is at offset 0 and the following word argument is at offset 4.
// CHECK-LABEL: <_matrix_pair_with_one_slot>:
// CHECK:       mov {{r[0-9]+}}, [r0]
// CHECK:       mov {{r[0-9]+}}, [r0 + #4]
// CHECK:       rets

// This enum uses one int-sized slot.
// CHECK-LABEL: <_matrix_five_enum>:
// CHECK-DAG:   {{(mov|add)}} r4, r13
// CHECK-DAG:   add r4, r14
// CHECK-DAG:   add r4, r15
// CHECK:       mov {{r[0-9]+}}, [r0]
// CHECK:       rets

// Every aggregate is stack-passed, and the following scalar remains on the
// stack.  The observed sizes are union=4, padded struct=4, bit-field struct=2.
// CHECK-LABEL: <_matrix_union_then_word>:
// CHECK-DAG:   mov r4, [r0]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #2]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #4]
// CHECK:       rets
// CHECK-LABEL: <_matrix_padded_then_word>:
// CHECK-DAG:   movb {{r[lh][0-7]}}, [r0]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #2]
// CHECK-DAG:   mov {{r[0-9]+}}, [r0 + #4]
// CHECK:       rets
// CHECK-LABEL: <_matrix_bitfield_then_word>:
// CHECK:       mov {{r[0-9]+}}, [r0]
// CHECK:       mov {{r[0-9]+}}, [r0 + #2]
// CHECK:       rets

// Default argument promotions produce a single 16-bit stack word.  Signed
// and unsigned bytes are extended appropriately before the fixed-offset
// store; enum is already int-sized.
// CHECK-LABEL: <_matrix_promote_s8>:
// CHECK:       sub r0, #2
// CHECK:       movbs {{r[0-9]+}}, r{{[lh][0-7]}}
// CHECK:       mov [r0], {{r[0-9]+}}
// CHECK:       calls
// CHECK:       add r0, #2
// CHECK:       rets
// CHECK-LABEL: <_matrix_promote_u8>:
// CHECK:       and r12, {{r[0-9]+}}
// CHECK:       sub r0, #2
// CHECK:       mov [r0], {{r[0-9]+}}
// CHECK:       calls
// CHECK:       add r0, #2
// CHECK:       rets
// CHECK-LABEL: <_matrix_promote_enum>:
// CHECK:       sub r0, #2
// CHECK:       mov [r0], r12
// CHECK:       calls
// CHECK:       add r0, #2
// CHECK:       rets
