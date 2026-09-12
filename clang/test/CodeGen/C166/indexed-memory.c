// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t.large-o0.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t.large.o
// RUN: llvm-objdump -d %t.large.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O2 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O2 -mllvm -verify-machineinstrs -c %s -o %t.small.o

typedef unsigned int u16;
typedef unsigned char u8;
typedef unsigned long u32;
typedef u8 __attribute__((c166_near)) near_u8;
typedef u16 __attribute__((c166_near)) near_u16;
typedef u32 __attribute__((c166_near)) near_u32;
typedef u8 __attribute__((c166_far)) far_u8;
typedef u16 __attribute__((c166_far)) far_u16;
typedef u32 __attribute__((c166_far)) far_u32;
typedef u16 __attribute__((c166_huge)) huge_u16;

u16 load_near_byte_displaced(const near_u8 *source) { return source[3]; }

void store_near_byte_displaced(near_u8 *destination, u8 value) {
  destination[3] = value;
}

u16 load_near_word_displaced(const near_u16 *source) { return source[3]; }

void store_near_word_displaced(near_u16 *destination, u16 value) {
  destination[3] = value;
}

u32 load_near_long_displaced(const near_u32 *source) { return source[1]; }

void store_near_long_displaced(near_u32 *destination, u32 value) {
  destination[1] = value;
}

u16 load_far_byte_displaced(const far_u8 *source) { return source[3]; }

void store_far_byte_displaced(far_u8 *destination, u8 value) {
  destination[3] = value;
}

u16 load_far_word_displaced(const far_u16 *source) { return source[3]; }

void store_far_word_displaced(far_u16 *destination, u16 value) {
  destination[3] = value;
}

u32 load_far_long_displaced(const far_u32 *source) { return source[1]; }

void store_far_long_displaced(far_u32 *destination, u32 value) {
  destination[1] = value;
}

u16 swap_far_words(far_u16 *value) {
  u16 high = value[3];
  value[3] = value[0];
  return high;
}

u16 sum_near_words(const near_u16 *source, u16 count) {
  u16 sum = 0;
  while (count != 0) {
    sum += *source++;
    --count;
  }
  return sum;
}

u16 sum_far_words(const far_u16 *source, u16 count) {
  u16 sum = 0;
  while (count != 0) {
    sum += *source++;
    --count;
  }
  return sum;
}

u16 sum_huge_words(const huge_u16 *source, u16 count) {
  u16 sum = 0;
  while (count != 0) {
    sum += *source++;
    --count;
  }
  return sum;
}

u16 sum_volatile_far_words(const volatile far_u16 *source, u16 count) {
  u16 sum = 0;
  while (count != 0) {
    sum += *source++;
    --count;
  }
  return sum;
}

u16 sum_volatile_near_words(const volatile near_u16 *source, u16 count) {
  u16 sum = 0;
  while (count != 0) {
    sum += *source++;
    --count;
  }
  return sum;
}

// A single-use constant address update is represented directly by the
// register-displacement memory forms.  Besides removing an address temporary,
// this avoids a copy when the original pointer remains live.
// CHECK-LABEL: <_load_near_byte_displaced>:
// CHECK-NOT:   add
// CHECK:       movb {{r[lh][0-9]+}}, [r{{[0-9]+}} + #3]
// CHECK:       rets
// CHECK-LABEL: <_store_near_byte_displaced>:
// CHECK-NOT:   add
// CHECK:       movb [r{{[0-9]+}} + #3], {{r[lh][0-9]+}}
// CHECK:       rets
// CHECK-LABEL: <_load_near_word_displaced>:
// CHECK-NOT:   add
// CHECK:       mov r4, [r{{[0-9]+}} + #6]
// CHECK:       rets
// CHECK-LABEL: <_store_near_word_displaced>:
// CHECK-NOT:   add
// CHECK:       mov [r{{[0-9]+}} + #6], r{{[0-9]+}}
// CHECK:       rets
// CHECK-LABEL: <_load_near_long_displaced>:
// CHECK-NOT:   add
// CHECK:       mov r4, [r{{[0-9]+}} + #4]
// CHECK:       mov r5, [r{{[0-9]+}} + #6]
// CHECK:       rets
// CHECK-LABEL: <_store_near_long_displaced>:
// CHECK-NOT:   add
// CHECK:       mov [r{{[0-9]+}} + #4], r{{[0-9]+}}
// CHECK:       mov [r{{[0-9]+}} + #6], r{{[0-9]+}}
// CHECK:       rets
// CHECK-LABEL: <_load_far_byte_displaced>:
// CHECK-NOT:   add
// CHECK:       extp
// CHECK-NEXT:  movb {{r[lh][0-9]+}}, [r{{[0-9]+}} + #3]
// CHECK:       rets
// CHECK-LABEL: <_store_far_byte_displaced>:
// CHECK-NOT:   add
// CHECK:       extp
// CHECK-NEXT:  movb [r{{[0-9]+}} + #3], {{r[lh][0-9]+}}
// CHECK:       rets
// CHECK-LABEL: <_load_far_word_displaced>:
// CHECK-NOT:   add
// CHECK:       extp
// CHECK-NEXT:  mov r4, [r{{[0-9]+}} + #6]
// CHECK:       rets
// CHECK-LABEL: <_store_far_word_displaced>:
// CHECK-NOT:   add
// CHECK:       extp
// CHECK-NEXT:  mov [r{{[0-9]+}} + #6], r{{[0-9]+}}
// CHECK:       rets
// CHECK-LABEL: <_load_far_long_displaced>:
// CHECK-NOT:   add
// CHECK:       extp
// CHECK-NEXT:  mov r4, [r{{[0-9]+}} + #4]
// CHECK-NEXT:  mov r5, [r{{[0-9]+}} + #6]
// CHECK:       rets
// CHECK-LABEL: <_store_far_long_displaced>:
// CHECK-NOT:   add
// CHECK:       extp
// CHECK-NEXT:  mov [r{{[0-9]+}} + #6], r{{[0-9]+}}
// CHECK-NEXT:  mov [r{{[0-9]+}} + #4], r{{[0-9]+}}
// CHECK:       rets
// CHECK-LABEL: <_swap_far_words>:
// CHECK-NOT:   add
// CHECK:       mov r4, [r{{[0-9]+}} + #6]
// CHECK:       mov [r{{[0-9]+}} + #6], r{{[0-9]+}}
// CHECK:       rets

// CHECK-LABEL: <_sum_near_words>:
// CHECK:       add r{{[0-9]+}}, [r{{[0-3]}}+]
// CHECK-NOT:   mov r{{[0-9]+}}, [r{{[0-9]+}}+]
// CHECK-NOT:   add r{{[0-9]+}}, #2
// CHECK:       rets

// CHECK-LABEL: <_sum_far_words>:
// CHECK:       extp
// CHECK-NEXT:  mov r{{[0-9]+}}, [r{{[0-9]+}}+]
// CHECK-NOT:   add r{{[0-9]+}}, #2
// CHECK:       rets

// A huge pointer may cross a segment boundary and therefore needs the
// carry-aware pointer update instead of the page-local post-increment form.
// CHECK-LABEL: <_sum_huge_words>:
// CHECK-NOT:   [r{{[0-9]+}}+]
// CHECK:       add r{{[0-9]+}}, #2
// CHECK-NEXT:  addc
// CHECK:       rets

// Post-increment remains part of the volatile load and preserves its ordering.
// CHECK-LABEL: <_sum_volatile_far_words>:
// CHECK:       extp
// CHECK-NEXT:  mov r{{[0-9]+}}, [r{{[0-9]+}}+]
// CHECK-NOT:   add r{{[0-9]+}}, #2
// CHECK:       rets

// CHECK-LABEL: <_sum_volatile_near_words>:
// CHECK:       mov r{{[0-9]+}}, [r{{[0-9]+}}+]
// CHECK-NOT:   add r{{[0-9]+}}, #2
// CHECK:       rets
