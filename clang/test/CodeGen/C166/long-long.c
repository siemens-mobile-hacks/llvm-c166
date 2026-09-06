// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s
// RUN: %clang_cc1 -triple c166-none-elf -emit-llvm -o - %s | FileCheck %s --check-prefix=IR
// C166-ABI: types.long_long_alias

typedef signed long long sll;
typedef unsigned long long ull;
typedef unsigned int u16;

struct long_long_record {
  u16 tag;
  sll value;
};

_Static_assert(sizeof(sll) == sizeof(long),
               "C166 long long aliases long width");
_Static_assert(sizeof(sll) == 4, "C166 long long is 32 bit");
_Static_assert(_Alignof(sll) == 2, "C166 long long is word aligned");
_Static_assert(sizeof(struct long_long_record) == 6,
               "C166 long long record size");
_Static_assert(__builtin_offsetof(struct long_long_record, value) == 2,
               "C166 long long record offset");

sll long_long_add(sll left, sll right) {
  return left + right;
}

ull long_long_add_unsigned(ull left, ull right) {
  return left + right;
}

sll long_long_packed(u16 prefix, sll value, u16 tail) {
  return value + prefix + tail;
}

sll long_long_record_sum(struct long_long_record record) {
  return record.value + record.tag;
}

struct long_long_record long_long_record_make(u16 tag, sll value) {
  struct long_long_record result = {tag, value};
  return result;
}

int long_long_signed_less(sll left, sll right) {
  return left < right;
}

u16 long_long_to_word(ull value) {
  return (u16)value;
}

ull word_to_long_long(u16 value) {
  return (ull)value;
}

// Two 32-bit values use R12:R13 and R14:R15.  The result is returned in
// R4:R5, like long.
// CHECK-LABEL: <_long_long_add>:
// CHECK-DAG:   mov r4, r{{(12|14)}}
// CHECK-DAG:   mov r5, r{{(13|15)}}
// CHECK:       add r4, r{{(12|14)}}
// CHECK-NEXT:  addc r5, r{{(13|15)}}
// CHECK-NEXT:  rets

// CHECK-LABEL: <_long_long_add_unsigned>:
// CHECK:       add r4, r{{(12|14)}}
// CHECK-NEXT:  addc r5, r{{(13|15)}}
// CHECK-NEXT:  rets

// A long-long value may start at R13; it is not even-pair aligned, and the
// following word remains in R15.
// CHECK-LABEL: <_long_long_packed>:
// CHECK:       mov r1, r12
// CHECK-NEXT:  mov r2, #0
// CHECK-NEXT:  add r1, r13
// CHECK-NEXT:  addc r2, r14
// CHECK-NEXT:  mov r4, r15
// CHECK-NEXT:  mov r5, #0
// CHECK-NEXT:  add r4, r1
// CHECK-NEXT:  addc r5, r2
// CHECK-NEXT:  rets

// IR-LABEL: define{{.*}} i32 @long_long_add(i32 noundef %left, i32 noundef %right)
// IR: add{{.*}} i32
// IR-LABEL: define{{.*}} i32 @long_long_add_unsigned(i32 noundef %left, i32 noundef %right)
// IR: add{{.*}} i32
// IR-LABEL: define{{.*}} i32 @long_long_packed(i16 noundef %prefix, i32 noundef %value, i16 noundef %tail)
// IR-LABEL: define{{.*}} i32 @long_long_record_sum(ptr addrspace(2) noundef byval(%struct.long_long_record) align 2 %record)
// IR-LABEL: define{{.*}} void @long_long_record_make(ptr addrspace(2){{.*}}sret(%struct.long_long_record) align 2 %agg.result, i16 noundef %tag, i32 noundef %value)
// IR-LABEL: define{{.*}} i16 @long_long_signed_less(i32 noundef %left, i32 noundef %right)
// IR: icmp slt i32
// IR-LABEL: define{{.*}} i16 @long_long_to_word(i32 noundef %value)
// IR: trunc i32
// IR-LABEL: define{{.*}} i32 @word_to_long_long(i16 noundef %value)
// IR: zext i16
