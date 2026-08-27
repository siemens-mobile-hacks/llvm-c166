// RUN: %clang_cc1 -triple c166-none-elf -std=c++17 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple c166-none-elf -std=c++17 -ast-print %s | FileCheck %s --check-prefix=PRINT
// RUN: %clang_cc1 -triple c166-none-elf -std=c++17 -emit-llvm -o - %s | FileCheck %s --check-prefix=MANGLE

using u16 = unsigned int;
using far_data = u16 [[clang::c166_far]];
using near_data = u16 [[clang::c166_near]];
using xnear_data = u16 [[clang::c166_xnear]];
using huge_data = u16 [[clang::c166_huge]];
using shuge_data = u16 [[clang::c166_shuge]];
using near_fn = u16 (u16) [[clang::c166_near]];
using huge_fn = u16 (u16) [[clang::c166_huge]];

static_assert(sizeof(far_data *) == 4);
static_assert(sizeof(near_data *) == 2);
static_assert(sizeof(xnear_data *) == 2);
static_assert(sizeof(huge_data *) == 4);
static_assert(sizeof(shuge_data *) == 4);

u16 near_decl(u16 value) [[clang::c166_near]] { return value; }
u16 huge_decl(u16 value) [[clang::c166_huge]] { return value; }

int select(near_fn *) { return 1; }
int select(huge_fn *) { return 2; }
int select(near_data *) { return 3; }
int select(xnear_data *) { return 4; }

[[c166_near]] u16 unqualified_near(u16); // expected-warning {{unknown attribute 'c166_near' ignored}}
[[c166_huge]] u16 unqualified_huge(u16); // expected-warning {{unknown attribute 'c166_huge' ignored}}

// PRINT-DAG: using far_data = u16 __attribute__((c166_far));
// PRINT-DAG: using near_data = u16 __attribute__((c166_near));
// PRINT-DAG: using xnear_data = u16 __attribute__((c166_xnear));
// PRINT-DAG: using huge_data = u16 __attribute__((c166_huge));
// PRINT-DAG: using shuge_data = u16 __attribute__((c166_shuge));
// PRINT: using near_fn = u16 (u16) __attribute__((c166_near));
// PRINT: using huge_fn = u16 (u16) __attribute__((c166_huge));
// MANGLE-DAG: define{{.*}} i16 @_Z9near_declj(i16
// MANGLE-DAG: define{{.*}} i16 @_Z9huge_declj(i16
// MANGLE-DAG: define{{.*}} i16 @_Z6selectPU3AS3FjjE(ptr addrspace(3)
// MANGLE-DAG: define{{.*}} i16 @_Z6selectPU3AS1FjjE(ptr addrspace(1)
// MANGLE-DAG: define{{.*}} i16 @_Z6selectPU3AS3j(ptr addrspace(3)
// MANGLE-DAG: define{{.*}} i16 @_Z6selectPU3AS4j(ptr addrspace(4)
