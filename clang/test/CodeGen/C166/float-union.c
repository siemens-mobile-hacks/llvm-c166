// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=large -O1 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=large -O3 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O1 -emit-obj -o %t.medium.o %s
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=small -O1 -emit-obj -o %t.small.o %s

// C166-ABI: floating.union_word_view

typedef unsigned int u16;

typedef union {
  float value;
  u16 words[2];
} float_words;

typedef union {
  double value;
  u16 words[4];
} double_words;

static const float_words global_float = {.value = 1.0f};
static const double_words global_double = {.value = 1.0};

// The ABI stores the most-significant 16-bit floating word at the
// lowest address.  These non-volatile union views used to be folded by SROA
// before the target could expose that physical representation.

// CHECK-LABEL: define{{.*}} i16 @float_msw(
// CHECK: [[FBITS:%.*]] = bitcast float %{{.*}} to i32
// CHECK: [[FMSW:%.*]] = lshr i32 [[FBITS]], 16
// CHECK: [[FRESULT:%.*]] = trunc{{.*}} i32 [[FMSW]] to i16
// CHECK: ret i16 [[FRESULT]]
u16 float_msw(float value) {
  float_words view;
  view.value = value;
  return view.words[0];
}

// CHECK-LABEL: define{{.*}} i16 @float_lsw(
// CHECK: [[FBITS2:%.*]] = bitcast float %{{.*}} to i32
// CHECK: [[FRESULT2:%.*]] = trunc i32 [[FBITS2]] to i16
// CHECK: ret i16 [[FRESULT2]]
u16 float_lsw(float value) {
  float_words view;
  view.value = value;
  return view.words[1];
}

// CHECK-LABEL: define{{.*}} i16 @double_msw(
// CHECK: [[DPHYSICAL:%.*]] = load i64
// CHECK: [[DRESULT:%.*]] = trunc i64 [[DPHYSICAL]] to i16
// CHECK: ret i16 [[DRESULT]]
u16 double_msw(double value) {
  double_words view;
  view.value = value;
  return view.words[0];
}

// CHECK-LABEL: define{{.*}} i16 @double_lsw(
// CHECK: [[DPHYSICAL2:%.*]] = load i64
// CHECK: [[DLSW:%.*]] = lshr i64 [[DPHYSICAL2]], 48
// CHECK: [[DRESULT2:%.*]] = trunc{{.*}} i64 [[DLSW]] to i16
// CHECK: ret i16 [[DRESULT2]]
u16 double_lsw(double value) {
  double_words view;
  view.value = value;
  return view.words[3];
}

// CHECK-LABEL: define{{.*}} float @float_from_words(
// CHECK: [[FLSW32:%.*]] = zext i16 %{{.*}} to i32
// CHECK: [[FMSW16:%.*]] = zext i16 %{{.*}} to i32
// CHECK: [[FMSW32:%.*]] = shl nuw i32 [[FMSW16]], 16
// CHECK: [[FCOMBINED:%.*]] = or disjoint i32 [[FMSW32]], [[FLSW32]]
// CHECK: [[FVALUE:%.*]] = bitcast i32 [[FCOMBINED]] to float
// CHECK: ret float [[FVALUE]]
float float_from_words(u16 msw, u16 lsw) {
  float_words view;
  view.words[0] = msw;
  view.words[1] = lsw;
  return view.value;
}

// CHECK-LABEL: define{{.*}} i16 @global_float_msw(
// CHECK: ret i16 16256
u16 global_float_msw(void) { return global_float.words[0]; }

// CHECK-LABEL: define{{.*}} i16 @global_double_msw(
// CHECK: ret i16 16368
u16 global_double_msw(void) { return global_double.words[0]; }
