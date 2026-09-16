// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=small -O2 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS
// RUN: %clang --target=c166-none-elf -mcmodel=small -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o

typedef unsigned int u16;
typedef unsigned long u32;
typedef void __attribute__((c166_far)) far_void;
typedef void __attribute__((c166_huge)) huge_void;
typedef void __attribute__((c166_shuge)) shuge_void;

u16 direct_to_word(const void *pointer) { return (u16)pointer; }
const void *word_to_direct(u16 address) { return (const void *)address; }
u32 direct_to_long(const void *pointer) { return (u32)pointer; }
const void *long_to_direct(u32 address) { return (const void *)address; }

far_void *direct_to_far(void *pointer) { return (far_void *)pointer; }
void *far_to_direct(far_void *pointer) { return (void *)pointer; }
huge_void *direct_to_huge(void *pointer) { return (huge_void *)pointer; }
void *huge_to_direct(huge_void *pointer) { return (void *)pointer; }
shuge_void *direct_to_shuge(void *pointer) { return (shuge_void *)pointer; }
void *shuge_to_direct(shuge_void *pointer) { return (void *)pointer; }

// C166-ABI: small.pointers.direct_casts
// C166-ABI: small.pointers.direct_dpp_selector

// Small integer casts preserve the raw 16-bit direct representation when the
// destination is one word.  Widening uses the pointer's selector bits to read
// the matching DPP0..DPP3 page.
// IR-LABEL: define{{.*}}i16 @direct_to_word(ptr addrspace(3){{.*}}%pointer)
// IR:       ptrtoaddr ptr addrspace(3) %pointer to i16
// IR-LABEL: define{{.*}}ptr addrspace(3) @word_to_direct(i16{{.*}}%address)
// IR:       inttoptr i16 %address to ptr addrspace(3)
// IR-LABEL: define{{.*}}i32 @direct_to_long(ptr addrspace(3){{.*}}%pointer)
// IR-DAG:   call{{.*}} i16 @llvm.c166.read.dpp(i16 0)
// IR-DAG:   call{{.*}} i16 @llvm.c166.read.dpp(i16 1)
// IR-DAG:   call{{.*}} i16 @llvm.c166.read.dpp(i16 2)
// IR-DAG:   call{{.*}} i16 @llvm.c166.read.dpp(i16 3)
// IR-LABEL: define{{.*}}ptr addrspace(3) @long_to_direct(i32{{.*}}%address)
// IR:       trunc i32 %address to i16
// IR:       inttoptr i16 {{.*}} to ptr addrspace(3)

// Direct-to-far first forms the linear address selected by the DPP, then uses
// the target conversion intrinsic.  Keeping that conversion explicit prevents
// the late lowering pass from treating an already packed offset:page value as
// linear and converting it a second time.  Direct-to-huge/shuge produces a
// linear segment:offset value.  Narrowing back to direct retains the
// selector-containing low word.
// IR-LABEL: define{{.*}}ptr addrspace(2) @direct_to_far(ptr addrspace(3){{.*}}%pointer)
// IR-NOT:   inttoptr
// IR-DAG:   call{{.*}} i16 @llvm.c166.read.dpp(i16 0)
// IR-DAG:   call{{.*}} i16 @llvm.c166.read.dpp(i16 3)
// IR:       select i1 {{.*}}, i32 0, i32 {{.*}}
// IR-NEXT:  call{{.*}} ptr addrspace(2) @llvm.c166.linear.to.far.p2(i32 {{.*}})
// IR-NOT:   inttoptr
// IR-LABEL: define{{.*}}ptr addrspace(3) @far_to_direct(ptr addrspace(2){{.*}}%pointer)
// IR:       lshr i32 {{.*}}, 2
// IR:       and i16 {{.*}}, -16384
// IR:       inttoptr i16 {{.*}} to ptr addrspace(3)
// IR-LABEL: define{{.*}}ptr addrspace(5) @direct_to_huge(ptr addrspace(3){{.*}}%pointer)
// IR-DAG:   call{{.*}} i16 @llvm.c166.read.dpp(i16 0)
// IR-DAG:   call{{.*}} i16 @llvm.c166.read.dpp(i16 3)
// IR:       inttoptr i32 {{.*}} to ptr addrspace(5)
// IR-LABEL: define{{.*}}ptr addrspace(3) @huge_to_direct(ptr addrspace(5){{.*}}%pointer)
// IR:       trunc i32 {{.*}} to i16
// IR-LABEL: define{{.*}}ptr addrspace(6) @direct_to_shuge(ptr addrspace(3){{.*}}%pointer)
// IR-DAG:   call{{.*}} i16 @llvm.c166.read.dpp(i16 0)
// IR-DAG:   call{{.*}} i16 @llvm.c166.read.dpp(i16 3)
// IR:       inttoptr i32 {{.*}} to ptr addrspace(6)
// IR-LABEL: define{{.*}}ptr addrspace(3) @shuge_to_direct(ptr addrspace(6){{.*}}%pointer)
// IR:       trunc i32 {{.*}} to i16

// DIS-LABEL: <_direct_to_long>:
// DIS-DAG:   mov {{r[0-9]+}}, dpp0
// DIS-DAG:   mov {{r[0-9]+}}, dpp1
// DIS-DAG:   mov {{r[0-9]+}}, dpp2
// DIS-DAG:   mov {{r[0-9]+}}, dpp3
// DIS:       rets
// DIS-LABEL: <_direct_to_far>:
// DIS-DAG:   mov {{r[0-9]+}}, dpp0
// DIS-DAG:   mov {{r[0-9]+}}, dpp1
// DIS-DAG:   mov {{r[0-9]+}}, dpp2
// DIS-DAG:   mov {{r[0-9]+}}, dpp3
// DIS:       rets
// DIS-LABEL: <_direct_to_huge>:
// DIS-DAG:   mov {{r[0-9]+}}, dpp0
// DIS-DAG:   mov {{r[0-9]+}}, dpp1
// DIS-DAG:   mov {{r[0-9]+}}, dpp2
// DIS-DAG:   mov {{r[0-9]+}}, dpp3
// DIS:       rets
// DIS-LABEL: <_direct_to_shuge>:
// DIS-DAG:   mov {{r[0-9]+}}, dpp0
// DIS-DAG:   mov {{r[0-9]+}}, dpp1
// DIS-DAG:   mov {{r[0-9]+}}, dpp2
// DIS-DAG:   mov {{r[0-9]+}}, dpp3
// DIS:       rets
