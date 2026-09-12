// RUN: %clang_cc1 -triple c166-none-elf -emit-llvm -o - %s | FileCheck %s

// CHECK: target datalayout = "e-m:u-P1-G2-A2-p:32:16-p1:32:16-p2:32:16:16:32-p3:16:16-p4:16:16-p5:32:16:16:32-p6:32:16:16:32-p7:16:16-p8:16:16-i32:16-i64:16-f32:16-f64:16-a:0:16-n8:16-S16-ni:2"
// CHECK: target triple = "c166-unknown-none-elf"

struct c166_layout {
  unsigned char byte;
  unsigned long dword;
};

_Static_assert(sizeof(struct c166_layout) == 6, "C166 struct size");
_Static_assert(_Alignof(struct c166_layout) == 2, "C166 struct alignment");
_Static_assert(__builtin_offsetof(struct c166_layout, dword) == 2,
               "C166 long offset");
_Static_assert(sizeof(long long) == 8, "C166 long long size");
_Static_assert(_Alignof(long long) == 2,
               "C166 long long alignment");

typedef unsigned short __sfr sfr_u16;
typedef unsigned short __esfr esfr_u16;
typedef unsigned short __near near_u16;
typedef unsigned short __xnear xnear_u16;
typedef unsigned short __far far_u16;
typedef unsigned short __huge huge_u16;
typedef unsigned short __shuge shuge_u16;
_Static_assert(sizeof(sfr_u16 *) == 2, "C166 SFR pointer size");
_Static_assert(sizeof(esfr_u16 *) == 2, "C166 ESFR pointer size");
_Static_assert(sizeof(near_u16 *) == 2, "C166 near pointer size");
_Static_assert(sizeof(xnear_u16 *) == 2, "C166 xnear pointer size");
_Static_assert(sizeof(far_u16 *) == 4, "C166 far pointer size");
_Static_assert(sizeof(huge_u16 *) == 4, "C166 huge pointer size");
_Static_assert(sizeof(shuge_u16 *) == 4, "C166 shuge pointer size");

// A 32-bit long starts at the next word and retains word alignment.
// CHECK: %struct.c166_layout = type { i8, i32 }
// CHECK: @layout_probe = addrspace(2) global %struct.c166_layout zeroinitializer, align 2
struct c166_layout layout_probe;

struct c166_bytes3 {
  unsigned char bytes[3];
};

struct __attribute__((packed)) c166_packed_bytes3 {
  unsigned char bytes[3];
};

// C166 rounds ordinary structure and union sizes to a 16-bit
// boundary even when every member is byte-aligned. Packed records retain the
// 8-bit size boundary.
_Static_assert(sizeof(struct c166_bytes3) == 4, "C166 record size boundary");
_Static_assert(_Alignof(struct c166_bytes3) == 2,
               "C166 minimum record alignment");
_Static_assert(sizeof(struct c166_packed_bytes3) == 3,
               "C166 packed record size boundary");
_Static_assert(_Alignof(struct c166_packed_bytes3) == 1,
               "C166 packed record alignment");

// C166 passes and returns char values in word carriers with an unspecified
// upper byte.  The IR signature must therefore not request sign/zero extension.
// CHECK-LABEL: define{{.*}} i8 @identity_byte(i8 noundef %value)
// CHECK-NOT: signext
// CHECK-NOT: zeroext
unsigned char identity_byte(unsigned char value) { return value; }

// CHECK-LABEL: define{{.*}} i16 @add_words(i16 noundef %lhs, i16 noundef %rhs)
unsigned int add_words(unsigned int lhs, unsigned int rhs) { return lhs + rhs; }

// CHECK-LABEL: define{{.*}} i32 @identity_dword(i32 noundef %value)
unsigned long identity_dword(unsigned long value) { return value; }

// A 64-bit integer is passed directly as four words. The backend returns it
// through a caller-owned slot because it does not fit the scalar return pair.
// This is a C166 LLVM ABI extension; existing integer types are unchanged.
// CHECK-LABEL: define{{.*}} i64 @identity_long_long(i64 noundef %value)
long long identity_long_long(long long value) { return value; }

// Large-model data pointers occupy 32 bits with 16-bit ABI alignment.
// CHECK-LABEL: define{{.*}} ptr addrspace(2) @identity_pointer(ptr addrspace(2) noundef %value)
void *identity_pointer(void *value) { return value; }

// Code pointers use the program address space but retain the same 32-bit
// storage and 16-bit ABI alignment.
// CHECK-LABEL: define{{.*}} ptr addrspace(1) @identity_function_pointer(ptr addrspace(1) noundef %value)
typedef void (*callback)(void);
callback identity_function_pointer(callback value) { return value; }

// CHECK-LABEL: define{{.*}} ptr addrspace(7) @identity_sfr_pointer(ptr addrspace(7) noundef %value)
sfr_u16 *identity_sfr_pointer(sfr_u16 *value) { return value; }

// CHECK-LABEL: define{{.*}} ptr addrspace(8) @identity_esfr_pointer(ptr addrspace(8) noundef %value)
esfr_u16 *identity_esfr_pointer(esfr_u16 *value) { return value; }
