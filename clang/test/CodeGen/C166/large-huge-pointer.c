// RUN: %clang_cc1 -triple c166-unknown-none -O2 -emit-llvm -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1 -triple c166-unknown-none -O2 -S -o - %s | FileCheck %s --check-prefix=ASM
// RUN: %clang_cc1 -triple c166-unknown-none -O0 -S -o /dev/null -mllvm -verify-machineinstrs %s
// RUN: %clang_cc1 -triple c166-unknown-none -O2 -S -o /dev/null -mllvm -verify-machineinstrs %s
// expected-no-diagnostics

// C166-ABI: pointers.huge_qualifier
// C166-ABI: pointers.shuge_qualifier

typedef signed long s32;
typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

typedef u8 __attribute__((c166_far)) far_u8;
typedef u8 __attribute__((c166_near)) near_u8;
typedef u8 __attribute__((c166_huge)) huge_u8;
typedef u16 __attribute__((c166_huge)) huge_u16;
typedef u32 __attribute__((c166_huge)) huge_u32;
typedef u8 __attribute__((c166_shuge)) shuge_u8;
typedef u16 __attribute__((c166_shuge)) shuge_u16;
typedef u32 __attribute__((c166_shuge)) shuge_u32;

u16 size_huge(void) { return sizeof(huge_u8 *); }
u16 size_shuge(void) { return sizeof(shuge_u8 *); }

u8 load_huge(volatile huge_u8 *p) { return *p; }
u16 load_huge_word(volatile huge_u16 *p) { return *p; }
u32 load_huge_long(volatile huge_u32 *p) { return *p; }
void store_huge(volatile huge_u32 *p, u32 value) { *p = value; }

u8 load_shuge(volatile shuge_u8 *p) { return *p; }
u16 load_shuge_word(volatile shuge_u16 *p) { return *p; }
u32 load_shuge_long(volatile shuge_u32 *p) { return *p; }
void store_shuge(volatile shuge_u32 *p, u32 value) { *p = value; }

volatile huge_u8 *add_huge(volatile huge_u8 *p, u16 n) { return p + n; }
volatile shuge_u8 *add_shuge(volatile shuge_u8 *p, u16 n) { return p + n; }
s32 wrap_shuge_delta(u32 raw) {
  volatile shuge_u8 *edge =
      (volatile shuge_u8 *)((raw & 0xffff0000UL) | 0xfff0UL);
  volatile shuge_u8 *wrapped = edge + 0x20U;
  return wrapped - edge;
}

s32 diff_huge(volatile huge_u8 *a, volatile huge_u8 *b) { return a - b; }
s32 diff_shuge(volatile shuge_u8 *a, volatile shuge_u8 *b) { return a - b; }
u16 size_diff_huge(volatile huge_u8 *a, volatile huge_u8 *b) {
  return sizeof(a - b);
}
u16 size_diff_shuge(volatile shuge_u8 *a, volatile shuge_u8 *b) {
  return sizeof(a - b);
}

u16 eq_huge(volatile huge_u8 *a, volatile huge_u8 *b) { return a == b; }
u16 lt_huge(volatile huge_u8 *a, volatile huge_u8 *b) { return a < b; }
u16 null_huge(volatile huge_u8 *p) { return p == 0; }
u16 eq_shuge(volatile shuge_u8 *a, volatile shuge_u8 *b) { return a == b; }
u16 lt_shuge(volatile shuge_u8 *a, volatile shuge_u8 *b) { return a < b; }
u16 null_shuge(volatile shuge_u8 *p) { return p == 0; }

u32 huge_to_long(volatile huge_u8 *p) { return (u32)p; }
volatile huge_u8 *long_to_huge(u32 p) { return (volatile huge_u8 *)p; }
u32 shuge_to_long(volatile shuge_u8 *p) { return (u32)p; }
volatile shuge_u8 *long_to_shuge(u32 p) { return (volatile shuge_u8 *)p; }

volatile shuge_u8 *huge_to_shuge(volatile huge_u8 *p) {
  return (volatile shuge_u8 *)p;
}
volatile huge_u8 *shuge_to_huge(volatile shuge_u8 *p) {
  return (volatile huge_u8 *)p;
}
volatile far_u8 *huge_to_far(volatile huge_u8 *p) {
  return (volatile far_u8 *)p;
}
volatile huge_u8 *far_to_huge(volatile far_u8 *p) {
  return (volatile huge_u8 *)p;
}
volatile huge_u8 *near_to_huge(volatile near_u8 *p) {
  return (volatile huge_u8 *)p;
}
volatile near_u8 *huge_to_near(volatile huge_u8 *p) {
  return (volatile near_u8 *)p;
}

// IR: target datalayout = "{{.*}}p5:32:16:16:32-p6:32:16:16:32{{.*}}"
// IR-LABEL: define{{.*}} i16 @size_huge()
// IR: ret i16 4
// IR-LABEL: define{{.*}} i16 @size_shuge()
// IR: ret i16 4
// IR-LABEL: define{{.*}} ptr addrspace(5) @add_huge(ptr addrspace(5){{.*}}, i16{{.*}})
// IR-LABEL: define{{.*}} ptr addrspace(6) @add_shuge(ptr addrspace(6){{.*}}, i16{{.*}})
// IR-LABEL: define{{.*}} i32 @wrap_shuge_delta(i32{{.*}})
// IR: call{{.*}} i32 @llvm.c166.far.add(i32 {{.*}}, i16 32)
// IR: sub i32
// IR-LABEL: define{{.*}} i32 @diff_huge(ptr addrspace(5){{.*}}, ptr addrspace(5){{.*}})
// IR: ptrtoaddr ptr addrspace(5) {{.*}} to i32
// IR: sub i32
// IR-LABEL: define{{.*}} i32 @diff_shuge(ptr addrspace(6){{.*}}, ptr addrspace(6){{.*}})
// IR: ptrtoaddr ptr addrspace(6) {{.*}} to i32
// IR: sub i32
// IR-LABEL: define{{.*}} i16 @size_diff_huge
// IR: ret i16 4
// IR-LABEL: define{{.*}} i16 @size_diff_shuge
// IR: ret i16 4

// ASM-LABEL: load_huge:
// ASM: exts
// ASM-LABEL: load_shuge:
// ASM: exts
// ASM-LABEL: load_shuge_long:
// ASM: exts
// ASM-LABEL: add_shuge:
// ASM: add
// ASM-NOT: addc
// ASM: rets
// ASM-LABEL: wrap_shuge_delta:
// ASM: add
// ASM-NOT: addc
// ASM: sub
// ASM: subc
