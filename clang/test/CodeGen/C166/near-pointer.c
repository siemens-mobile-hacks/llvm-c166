// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// C166-ABI: pointers.near_qualifier
// C166-ABI: pointers.xnear_qualifier

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u16;
typedef signed int s16;
typedef unsigned long u32;

typedef void __attribute__((c166_far)) far_void;
typedef u16 __attribute__((c166_far)) far_u16;
typedef void __attribute__((c166_near)) near_void;
typedef u8 __attribute__((c166_near)) near_u8;
typedef s8 __attribute__((c166_near)) near_s8;
typedef u16 __attribute__((c166_near)) near_u16;
typedef u32 __attribute__((c166_near)) near_u32;
typedef void __attribute__((c166_xnear)) xnear_void;
typedef u8 __attribute__((c166_xnear)) xnear_u8;
typedef u16 __attribute__((c166_xnear)) xnear_u16;

_Static_assert(sizeof(near_void *) == 2, "C166 near pointer width");
_Static_assert(sizeof(xnear_void *) == 2, "C166 xnear pointer width");
_Static_assert(sizeof(far_void *) == 4, "C166 far pointer width");

u16 load_near_u8(const volatile near_u8 *p) { return *p; }
s16 load_near_s8(const volatile near_s8 *p) { return *p; }
u16 load_near_u16(const volatile near_u16 *p) { return *p; }
u32 load_near_u32(const volatile near_u32 *p) { return *p; }

void store_near_u8(volatile near_u8 *p, u8 value) { *p = value; }
void store_near_u16(volatile near_u16 *p, u16 value) { *p = value; }
void store_near_u32(volatile near_u32 *p, u32 value) { *p = value; }

u16 load_xnear_u16(const volatile xnear_u16 *p) { return *p; }
void store_xnear_u16(volatile xnear_u16 *p, u16 value) { *p = value; }

near_u8 *near_add(near_u8 *p, u16 count) { return p + count; }
s16 near_difference(const near_u16 *lhs, const near_u16 *rhs) {
  return lhs - rhs;
}
u16 near_less(const near_u8 *lhs, const near_u8 *rhs) {
  return lhs < rhs;
}
u16 near_is_null(const near_u8 *p) { return p == 0; }

xnear_u8 *xnear_add(xnear_u8 *p, u16 count) { return p + count; }
s16 xnear_difference(const xnear_u16 *lhs, const xnear_u16 *rhs) {
  return lhs - rhs;
}

// A 16-bit pointer consumes one argument word: p is in R13,
// between the scalar words in R12 and R14.
u16 near_between_words(u16 first, const volatile near_u8 *p, u16 last) {
  return first + *p + last;
}

u16 load_explicit_far(const volatile far_u16 *p) { return *p; }

u16 near_to_word(const near_void *p) { return (u16)p; }
u16 xnear_to_word(const xnear_void *p) { return (u16)p; }
near_void *word_to_near(u16 address) { return (near_void *)address; }
xnear_void *word_to_xnear(u16 address) { return (xnear_void *)address; }

u32 near_to_long(const near_void *p) { return (u32)p; }
u32 xnear_to_long(const xnear_void *p) { return (u32)p; }
near_void *long_to_near(u32 address) { return (near_void *)address; }
xnear_void *long_to_xnear(u32 address) { return (xnear_void *)address; }

far_void *near_to_far(near_void *p) { return (far_void *)p; }
far_void *xnear_to_far(xnear_void *p) { return (far_void *)p; }
near_void *far_to_near(far_void *p) { return (near_void *)p; }
xnear_void *far_to_xnear(far_void *p) { return (xnear_void *)p; }
xnear_void *near_to_xnear(near_void *p) { return (xnear_void *)p; }
near_void *xnear_to_near(xnear_void *p) { return (near_void *)p; }

// IR-LABEL: define{{.*}}i16 @load_near_u16(ptr addrspace(3){{.*}}%p)
// IR:       load volatile i16, ptr addrspace(3) %p
// IR-LABEL: define{{.*}}i16 @load_xnear_u16(ptr addrspace(4){{.*}}%p)
// IR:       load volatile i16, ptr addrspace(4) %p
// IR-LABEL: define{{.*}}ptr addrspace(3) @near_add(ptr addrspace(3){{.*}}%p, i16{{.*}}%count)
// IR:       getelementptr inbounds{{.*}} i8, ptr addrspace(3) %p, i16 %count
// IR-LABEL: define{{.*}}ptr addrspace(4) @xnear_add(ptr addrspace(4){{.*}}%p, i16{{.*}}%count)
// IR:       getelementptr inbounds{{.*}} i8, ptr addrspace(4) %p, i16 %count
// IR-LABEL: define{{.*}}i16 @load_explicit_far(ptr addrspace(2){{.*}}%p)
// IR-LABEL: define{{.*}}i16 @near_to_word(ptr addrspace(3){{.*}}%p)
// IR:       call{{.*}}i32 @llvm.c166.near.to.linear.p3(ptr addrspace(3) %p)
// IR-LABEL: define{{.*}}i16 @xnear_to_word(ptr addrspace(4){{.*}}%p)
// IR:       call{{.*}}i32 @llvm.c166.near.to.linear.p4(ptr addrspace(4) %p)
// IR-LABEL: define{{.*}}ptr addrspace(3) @word_to_near(i16{{.*}}%address)
// IR:       call{{.*}}ptr addrspace(3) @llvm.c166.linear.to.near.p3(i32
// IR-LABEL: define{{.*}}ptr addrspace(4) @word_to_xnear(i16{{.*}}%address)
// IR:       call{{.*}}ptr addrspace(4) @llvm.c166.linear.to.near.p4(i32
// IR-LABEL: define{{.*}}i32 @near_to_long(ptr addrspace(3){{.*}}%p)
// IR:       call{{.*}}i32 @llvm.c166.near.to.linear.p3(ptr addrspace(3) %p)
// IR-LABEL: define{{.*}}i32 @xnear_to_long(ptr addrspace(4){{.*}}%p)
// IR:       call{{.*}}i32 @llvm.c166.near.to.linear.p4(ptr addrspace(4) %p)
// IR-LABEL: define{{.*}}ptr addrspace(3) @long_to_near(i32{{.*}}%address)
// IR:       call{{.*}}ptr addrspace(3) @llvm.c166.linear.to.near.p3(i32 %{{.*}})
// IR-LABEL: define{{.*}}ptr addrspace(4) @long_to_xnear(i32{{.*}}%address)
// IR:       call{{.*}}ptr addrspace(4) @llvm.c166.linear.to.near.p4(i32 %{{.*}})
// IR-LABEL: define{{.*}}ptr addrspace(2) @near_to_far(ptr addrspace(3){{.*}}%p)
// IR:       call{{.*}}ptr addrspace(2) @llvm.c166.near.to.far.p2(i16{{.*}}, i16 2)
// IR-LABEL: define{{.*}}ptr addrspace(2) @xnear_to_far(ptr addrspace(4){{.*}}%p)
// IR:       call{{.*}}ptr addrspace(2) @llvm.c166.near.to.far.p2(i16{{.*}}, i16 1)
// IR-NOT:   addrspacecast

// Near and xnear dereferences are ordinary indirect accesses.  The pointer's
// two selector bits choose DPP2 or DPP1; no EXTP prefix is part of the ABI.
// DIS-LABEL: <_load_near_u8>:
// DIS:       movb rl4, [r12]
// DIS-NEXT:  movbz r4, rl4
// DIS-NOT:   extp
// DIS:       rets
// DIS-LABEL: <_load_near_s8>:
// DIS:       movb rl4, [r12]
// DIS-NEXT:  movbs r4, rl4
// DIS-NOT:   extp
// DIS:       rets
// DIS-LABEL: <_load_near_u16>:
// DIS:       mov r4, [r12]
// DIS-NOT:   extp
// DIS:       rets
// DIS-LABEL: <_load_near_u32>:
// DIS:       mov r4, [r12]
// DIS-NEXT:  mov r5, [r12 + #2]
// DIS-NOT:   extp
// DIS:       rets
// DIS-LABEL: <_store_near_u8>:
// DIS:       movb [r12],
// DIS-NOT:   extp
// DIS:       rets
// DIS-LABEL: <_store_near_u16>:
// DIS:       mov [r12], r13
// DIS-NOT:   extp
// DIS:       rets
// DIS-LABEL: <_store_near_u32>:
// DIS:       mov [r12], r13
// DIS-NEXT:  mov [r12 + #2], r14
// DIS-NOT:   extp
// DIS:       rets
// DIS-LABEL: <_load_xnear_u16>:
// DIS:       mov r4, [r12]
// DIS-NOT:   extp
// DIS:       rets
// DIS-LABEL: <_store_xnear_u16>:
// DIS:       mov [r12], r13
// DIS-NOT:   extp
// DIS:       rets

// Arithmetic, difference, comparison and null testing are single-word
// operations required by the ABI.
// DIS-LABEL: <_near_add>:
// DIS:       add r4, r13
// DIS-NOT:   addc
// DIS:       rets
// DIS-LABEL: <_near_difference>:
// DIS:       sub r4, r13
// DIS-NEXT:  ashr r4, #1
// DIS:       rets
// DIS-LABEL: <_near_less>:
// DIS:       cmp r12, r13
// DIS:       rets
// DIS-LABEL: <_near_is_null>:
// DIS:       cmp r12,
// DIS:       rets
// DIS-LABEL: <_xnear_add>:
// DIS:       add r4, r13
// DIS-NOT:   addc
// DIS:       rets
// DIS-LABEL: <_xnear_difference>:
// DIS:       sub r4, r13
// DIS-NEXT:  ashr r4, #1
// DIS:       rets
// DIS-LABEL: <_near_between_words>:
// DIS:       add r4, r12
// DIS:       movb {{r[lh][0-9]+}}, [r13]
// DIS:       rets

// Explicit _far remains the existing paged 32-bit data pointer.
// DIS-LABEL: <_load_explicit_far>:
// DIS:       extp r13, #1
// DIS-NEXT:  mov r4, [r12]
// DIS:       rets

// Integer widening reads the ABI-selected page and strips the two selector
// bits.  Narrowing forces exactly selector 10b for near or 01b for xnear.
// DIS-LABEL: <_near_to_long>:
// DIS:       mov {{r[0-9]+}}, dpp2
// DIS:       shl {{r[0-9]+}}, #14
// DIS:       and {{r[0-9]+}}, #16383
// DIS:       rets
// DIS-LABEL: <_xnear_to_long>:
// DIS:       mov {{r[0-9]+}}, dpp1
// DIS:       shl {{r[0-9]+}}, #14
// DIS:       and {{r[0-9]+}}, #16383
// DIS:       rets
// DIS-LABEL: <_long_to_near>:
// DIS:       or {{r[0-9]+}}, #32768
// DIS:       and {{r[0-9]+}}, #49151
// DIS:       rets
// DIS-LABEL: <_long_to_xnear>:
// DIS:       or {{r[0-9]+}}, #16384
// DIS:       and {{r[0-9]+}}, #32767
// DIS:       rets

// Widening to far preserves raw null and otherwise forms offset:DPP.
// DIS-LABEL: <_near_to_far>:
// DIS:       cmp
// DIS-NEXT:  jmpr cc_eq,
// DIS:       mov {{r[0-9]+}}, dpp2
// DIS:       and {{r[0-9]+}}, #16383
// DIS:       rets
// DIS-LABEL: <_xnear_to_far>:
// DIS:       cmp
// DIS-NEXT:  jmpr cc_eq,
// DIS:       mov {{r[0-9]+}}, dpp1
// DIS:       and {{r[0-9]+}}, #16383
// DIS:       rets

// Cross-qualifier casts rewrite only the selector bits.
// DIS-LABEL: <_near_to_xnear>:
// DIS:       or {{r[0-9]+}}, #16384
// DIS:       and {{r[0-9]+}}, #32767
// DIS:       rets
// DIS-LABEL: <_xnear_to_near>:
// DIS:       or {{r[0-9]+}}, #32768
// DIS:       and {{r[0-9]+}}, #49151
// DIS:       rets
