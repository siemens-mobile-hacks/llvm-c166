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
// CHECK: call{{.*}} @llvm.c166.float.store.f32.p2(float %value, ptr addrspace(2) [[FVIEW:%.*]], i32 2)
// CHECK: [[FRESULT:%.*]] = load i16, ptr addrspace(2) [[FVIEW]]
// CHECK: ret i16 [[FRESULT]]
u16 float_msw(float value) {
  float_words view;
  view.value = value;
  return view.words[0];
}

// CHECK-LABEL: define{{.*}} i16 @float_lsw(
// CHECK: call{{.*}} @llvm.c166.float.store.f32.p2(float %value, ptr addrspace(2) [[FVIEW2:%.*]], i32 2)
// CHECK: [[FLSW:%.*]] = getelementptr{{.*}} ptr addrspace(2) [[FVIEW2]], i32 2
// CHECK: [[FRESULT2:%.*]] = load i16, ptr addrspace(2) [[FLSW]]
// CHECK: ret i16 [[FRESULT2]]
u16 float_lsw(float value) {
  float_words view;
  view.value = value;
  return view.words[1];
}

// CHECK-LABEL: define{{.*}} i16 @double_msw(
// CHECK: [[DVALUE:%.*]] = load double, ptr addrspace(2) %{{.*}}
// CHECK: call{{.*}} @llvm.c166.float.store.f64.p2(double [[DVALUE]], ptr addrspace(2) [[DVIEW:%.*]], i32 2)
// CHECK: [[DRESULT:%.*]] = load i16, ptr addrspace(2) [[DVIEW]]
// CHECK: ret i16 [[DRESULT]]
u16 double_msw(double value) {
  double_words view;
  view.value = value;
  return view.words[0];
}

// CHECK-LABEL: define{{.*}} i16 @double_lsw(
// CHECK: [[DVALUE2:%.*]] = load double, ptr addrspace(2) %{{.*}}
// CHECK: call{{.*}} @llvm.c166.float.store.f64.p2(double [[DVALUE2]], ptr addrspace(2) [[DVIEW2:%.*]], i32 2)
// CHECK: [[DLSW:%.*]] = getelementptr{{.*}} ptr addrspace(2) [[DVIEW2]], i32 6
// CHECK: [[DRESULT2:%.*]] = load i16, ptr addrspace(2) [[DLSW]]
// CHECK: ret i16 [[DRESULT2]]
u16 double_lsw(double value) {
  double_words view;
  view.value = value;
  return view.words[3];
}

// CHECK-LABEL: define{{.*}} float @float_from_words(
// CHECK: store i16 %msw, ptr addrspace(2) [[WVIEW:%[-a-zA-Z$._0-9]+]]
// CHECK: [[WLSW:%.*]] = getelementptr{{.*}} ptr addrspace(2) [[WVIEW]], i32 2
// CHECK: store i16 %lsw, ptr addrspace(2) [[WLSW]]
// CHECK: [[FVALUE:%.*]] = call{{.*}} float @llvm.c166.float.load.f32.p2(ptr addrspace(2) [[WVIEW]], i32 2)
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
