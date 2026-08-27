// REQUIRES: c166-registered-target
// RUN: %clang_cc1 -triple c166-none-elf -fdump-record-layouts -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=LAYOUT
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t-o2.o

typedef unsigned int u16;

struct exact_3_5_8 {
  unsigned int a : 3;
  unsigned int b : 5;
  unsigned int c : 8;
};

struct cross_7_10 {
  unsigned int a : 7;
  unsigned int b : 10;
};

struct zero_width {
  unsigned int a : 3;
  unsigned int : 0;
  unsigned int b : 5;
};

struct __attribute__((packed)) packed_cross_7_10 {
  unsigned int a : 7;
  unsigned int b : 10;
};

struct __attribute__((packed)) packed_cross_9_8 {
  unsigned int a : 9;
  unsigned int b : 8;
};

struct __attribute__((packed)) packed_cross_15_2 {
  unsigned int a : 15;
  unsigned int b : 2;
};

struct __attribute__((packed)) packed_cross_1_16 {
  unsigned int a : 1;
  unsigned int b : 16;
};

struct __attribute__((packed)) packed_zero_width {
  unsigned int a : 3;
  unsigned int : 0;
  unsigned int b : 5;
};

struct __attribute__((packed)) packed_inner {
  unsigned char byte;
  unsigned int word;
};

struct nested_packed {
  unsigned char prefix;
  struct packed_inner inner;
  unsigned char suffix;
};

struct __attribute__((packed)) packed_nested {
  unsigned char prefix;
  struct packed_inner inner;
  unsigned char suffix;
};

struct packed_array_outer {
  unsigned char prefix;
  struct packed_inner values[2];
  unsigned char suffix;
};

typedef char assert_exact_size[sizeof(struct exact_3_5_8) == 2 ? 1 : -1];
typedef char assert_cross_size[sizeof(struct cross_7_10) == 4 ? 1 : -1];
typedef char assert_zero_size[sizeof(struct zero_width) == 4 ? 1 : -1];
typedef char assert_packed_cross_size[
    sizeof(struct packed_cross_7_10) == 3 ? 1 : -1];
typedef char assert_packed_cross_9_8_size[
    sizeof(struct packed_cross_9_8) == 3 ? 1 : -1];
typedef char assert_packed_cross_15_2_size[
    sizeof(struct packed_cross_15_2) == 3 ? 1 : -1];
typedef char assert_packed_cross_1_16_size[
    sizeof(struct packed_cross_1_16) == 3 ? 1 : -1];
typedef char assert_packed_zero_size[
    sizeof(struct packed_zero_width) == 3 ? 1 : -1];
typedef char assert_packed_inner_size[
    sizeof(struct packed_inner) == 3 ? 1 : -1];
typedef char assert_nested_packed_size[
    sizeof(struct nested_packed) == 6 ? 1 : -1];
typedef char assert_packed_nested_size[
    sizeof(struct packed_nested) == 5 ? 1 : -1];
typedef char assert_packed_array_outer_size[
    sizeof(struct packed_array_outer) == 10 ? 1 : -1];

u16 get_packed_cross_7_10_b(struct packed_cross_7_10 *value) {
  return value->b;
}

u16 get_packed_cross_9_8_b(struct packed_cross_9_8 *value) {
  return value->b;
}

u16 get_packed_cross_15_2_b(struct packed_cross_15_2 *value) {
  return value->b;
}

u16 get_packed_cross_1_16_b(struct packed_cross_1_16 *value) {
  return value->b;
}

// LAYOUT:              0 | struct packed_cross_7_10
// LAYOUT-NEXT:     0:0-6 |   unsigned int a
// LAYOUT-NEXT: 1:0-9 |   unsigned int b
// LAYOUT-NEXT:         | [sizeof=3, align=1]
// LAYOUT:              0 | struct packed_cross_9_8
// LAYOUT-NEXT:     0:0-8 |   unsigned int a
// LAYOUT-NEXT: 1:1-8 |   unsigned int b
// LAYOUT-NEXT:         | [sizeof=3, align=1]
// LAYOUT:              0 | struct packed_cross_15_2
// LAYOUT-NEXT:    0:0-14 |   unsigned int a
// LAYOUT-NEXT: 1:7-8 |   unsigned int b
// LAYOUT-NEXT:          | [sizeof=3, align=1]
// LAYOUT:              0 | struct packed_cross_1_16
// LAYOUT-NEXT:     0:0-0 |   unsigned int a
// LAYOUT-NEXT: 1:0-15 |   unsigned int b
// LAYOUT-NEXT:          | [sizeof=3, align=1]
// LAYOUT:              0 | struct nested_packed
// LAYOUT-NEXT:         0 |   unsigned char prefix
// LAYOUT-NEXT:         2 |   struct packed_inner inner
// LAYOUT-NEXT:         2 |     unsigned char byte
// LAYOUT-NEXT:         3 |     unsigned int word
// LAYOUT-NEXT:         5 |   unsigned char suffix
// LAYOUT-NEXT:           | [sizeof=6, align=2]
// LAYOUT:              0 | struct packed_nested
// LAYOUT-NEXT:         0 |   unsigned char prefix
// LAYOUT-NEXT:         1 |   struct packed_inner inner
// LAYOUT-NEXT:         1 |     unsigned char byte
// LAYOUT-NEXT:         2 |     unsigned int word
// LAYOUT-NEXT:         4 |   unsigned char suffix
// LAYOUT-NEXT:           | [sizeof=5, align=1]
// LAYOUT:              0 | struct packed_array_outer
// LAYOUT-NEXT:         0 |   unsigned char prefix
// LAYOUT-NEXT:         2 |   struct packed_inner[2] values
// LAYOUT-NEXT:         8 |   unsigned char suffix
// LAYOUT-NEXT:           | [sizeof=10, align=2]
