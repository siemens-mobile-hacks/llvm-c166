// RUN: %clang_cc1 -triple c166-none-elf -dM -E %s | FileCheck %s

// CHECK-DAG: #define __esfr __attribute__((c166_esfr))
// CHECK-DAG: #define __far __attribute__((c166_far))
// CHECK-DAG: #define __huge __attribute__((c166_huge))
// CHECK-DAG: #define __near __attribute__((c166_near))
// CHECK-DAG: #define __sfr __attribute__((c166_sfr))
// CHECK-DAG: #define __shuge __attribute__((c166_shuge))
// CHECK-DAG: #define __xnear __attribute__((c166_xnear))

#if !__has_attribute(c166_sfr)
#error c166_sfr must be discoverable
#endif

#if !__has_attribute(c166_esfr)
#error c166_esfr must be discoverable
#endif
