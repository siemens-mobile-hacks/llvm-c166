// RUN: %clang_cc1 -E -dM -ffreestanding -triple c166-none-elf < /dev/null | FileCheck %s --implicit-check-not=__CHAR_UNSIGNED__
// RUN: %clang_cc1 -E -dM -ffreestanding -triple c166-none-elf < /dev/null | FileCheck %s --check-prefix=NO-QUALIFIER-MACROS
// RUN: not %clang_cc1 -fsyntax-only -std=c2x -triple c166-none-elf -DC166_TEST_BITINT %s 2>&1 | FileCheck %s --check-prefix=NO-BITINT
// RUN: %clang_cc1 -fsyntax-only -std=c2x -triple c166-none-elf -DC166_TEST_BITINT -fexperimental-max-bitint-width=64 %s

// CHECK: #define __BIGGEST_ALIGNMENT__ 2
// CHECK: #define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
// CHECK: #define __C166_MEMORY_MODEL__ 1
// CHECK: #define __C166__ 1
// CHECK: #define __CHAR_BIT__ 8
// CHECK: #define __INT64_TYPE__ long long int
// CHECK: #define __INTMAX_MAX__ 9223372036854775807LL
// CHECK: #define __INTMAX_TYPE__ long long int
// CHECK: #define __INTMAX_WIDTH__ 64
// CHECK: #define __INTPTR_TYPE__ long int
// CHECK: #define __LLONG_WIDTH__ 64
// CHECK: #define __LONG_LONG_MAX__ 9223372036854775807LL
// CHECK: #define __PTRDIFF_TYPE__ int
// CHECK: #define __SIZEOF_DOUBLE__ 8
// CHECK: #define __SIZEOF_INT__ 2
// CHECK: #define __SIZEOF_LONG_LONG__ 8
// CHECK: #define __SIZEOF_LONG__ 4
// CHECK: #define __SIZEOF_POINTER__ 4
// CHECK: #define __SIZE_TYPE__ unsigned int
// CHECK: #define __SOFTFP__ 1
// CHECK: #define __USER_LABEL_PREFIX__ _
// CHECK: #define __WCHAR_TYPE__ int
// CHECK: #define __c166__ 1

// NO-QUALIFIER-MACROS: #define __c166__ 1
// NO-QUALIFIER-MACROS-NOT: #define _far
// NO-QUALIFIER-MACROS-NOT: #define _huge
// NO-QUALIFIER-MACROS-NOT: #define _near
// NO-QUALIFIER-MACROS-NOT: #define _shuge
// NO-QUALIFIER-MACROS-NOT: #define _stackparm
// NO-QUALIFIER-MACROS-NOT: #define _xnear

#ifdef C166_TEST_BITINT
// NO-BITINT: error: _BitInt is not supported on this target
typedef unsigned _BitInt(64) c166_runtime_u64;
_Static_assert(sizeof(c166_runtime_u64) == 8, "private runtime integer");
_Static_assert(sizeof(unsigned long long) == 8, "public long long");
#endif
