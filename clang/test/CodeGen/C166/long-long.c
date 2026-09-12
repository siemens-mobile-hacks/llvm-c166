// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=tiny -O1 -mllvm -verify-machineinstrs -c %s -o %t.tiny.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.large.o
// RUN: %clang --target=c166-none-elf -mcmodel=huge -O1 -mllvm -verify-machineinstrs -c %s -o %t.huge.o
// RUN: %clang_cc1 -triple c166-none-elf -emit-llvm -o - %s | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=small -O2 -S -o - %s | FileCheck %s --check-prefix=ASM

typedef signed long long s64;
typedef unsigned long long u64;
typedef unsigned int u16;

struct record {
  u16 tag;
  s64 value;
};

_Static_assert(sizeof(s64) == 8, "64-bit long long");
_Static_assert(_Alignof(s64) == 2, "word-aligned long long");
_Static_assert(sizeof(struct record) == 10, "record size");
_Static_assert(__builtin_offsetof(struct record, value) == 2, "record offset");

s64 add(s64 left, s64 right) { return left + right; }
u64 multiply(u64 left, u64 right) { return left * right; }
u64 divide(u64 left, u64 right) { return left / right; }
s64 packed(u16 prefix, s64 value, u16 tail) {
  return value + prefix + tail;
}
s64 record_sum(struct record value) { return value.value + value.tag; }
struct record record_make(u16 tag, s64 value) {
  struct record result = {tag, value};
  return result;
}
int signed_less(s64 left, s64 right) { return left < right; }
u16 to_word(u64 value) { return (u16)value; }
u64 from_word(u16 value) { return (u64)value; }

// An i64 libcall result is already in integer word order.  It must not use
// the word reversal required for a softened f64 result carried as i64.
// ASM-LABEL: _divide:
// ASM: calls seg(___udivdi3), sof(___udivdi3)
// ASM-NEXT: mov r1, [r4+]
// ASM-NEXT: mov r2, [r4+]
// ASM-NEXT: mov r3, [r4+]
// ASM-NEXT: mov r4, [r4]

// The prefix consumes R12, leaving only three argument registers.  The
// four-word i64 argument therefore starts on the stack as one unit; it must
// not be split between R13:R14 and the stack.
// ASM-LABEL: _packed:
// ASM: mov r13, r0
// ASM-COUNT-4: mov {{r[0-9]+}}, [r13+]
// ASM: mov {{r[0-9]+}}, [r13]

// CHECK-LABEL: define{{.*}} i64 @add(i64 noundef %left, i64 noundef %right)
// CHECK: add{{.*}} i64
// CHECK-LABEL: define{{.*}} i64 @multiply(i64 noundef %left, i64 noundef %right)
// CHECK: mul i64
// CHECK-LABEL: define{{.*}} i64 @divide(i64 noundef %left, i64 noundef %right)
// CHECK: udiv i64
// CHECK-LABEL: define{{.*}} i64 @packed(i16 noundef %prefix, i64 noundef %value, i16 noundef %tail)
// CHECK-LABEL: define{{.*}} i64 @record_sum(ptr addrspace(2) noundef byval(%struct.record) align 2 %value)
// CHECK-LABEL: define{{.*}} void @record_make(ptr addrspace(2){{.*}}sret(%struct.record) align 2 %agg.result, i16 noundef %tag, i64 noundef %value)
// CHECK-LABEL: define{{.*}} i16 @signed_less(i64 noundef %left, i64 noundef %right)
// CHECK: icmp slt i64
// CHECK-LABEL: define{{.*}} i16 @to_word(i64 noundef %value)
// CHECK: trunc i64
// CHECK-LABEL: define{{.*}} i64 @from_word(i16 noundef %value)
// CHECK: zext i16
