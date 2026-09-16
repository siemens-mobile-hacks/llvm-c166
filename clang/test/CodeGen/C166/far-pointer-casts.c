// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t.o
// C166-ABI: pointers.data_pointer_to_long_default
// C166-ABI: pointers.long_to_data_pointer_default
// C166-ABI: pointers.function_pointer_long_default

typedef unsigned int u16;
typedef unsigned long u32;
typedef void (*callback)(void);

u32 data_pointer_to_long(volatile u16 *pointer) { return (u32)pointer; }

volatile u16 *long_to_data_pointer(u32 address) {
  return (volatile u16 *)address;
}

u32 function_pointer_to_long(callback function) { return (u32)function; }

callback long_to_function_pointer(u32 address) { return (callback)address; }

u32 data_pointer_roundtrip(u32 address) {
  return (u32)(volatile u16 *)address;
}

// C functions and function-pointer values use the program address space;
// ordinary data pointers use the non-integral far-data address space.
// IR-LABEL: define{{.*}} i32 @data_pointer_to_long(ptr addrspace(2) {{.*}}%pointer){{.*}}addrspace(1)
// IR:       call{{.*}} i32 @llvm.c166.far.to.linear.p2(ptr addrspace(2) %pointer)
// IR-LABEL: define{{.*}} ptr addrspace(2) @long_to_data_pointer(i32 {{.*}}%address){{.*}}addrspace(1)
// IR:       call{{.*}} ptr addrspace(2) @llvm.c166.linear.to.far.p2(i32 %address)
// IR-LABEL: define{{.*}} i32 @function_pointer_to_long(ptr addrspace(1) {{.*}}%function){{.*}}addrspace(1)
// IR:       ptrtoint ptr addrspace(1) %function to i32
// IR-LABEL: define{{.*}} ptr addrspace(1) @long_to_function_pointer(i32 {{.*}}%address){{.*}}addrspace(1)
// IR:       inttoptr i32 %address to ptr addrspace(1)
// IR-LABEL: define{{.*}} i32 @data_pointer_roundtrip(i32 {{.*}}%address){{.*}}addrspace(1)
// IR:       call{{.*}} ptr addrspace(2) @llvm.c166.linear.to.far.p2(i32 %address)
// IR:       call{{.*}} i32 @llvm.c166.far.to.linear.p2(ptr addrspace(2) {{.*}})

// The optimized data-pointer conversions perform the default far<->huge
// conversion using native constant shifts.  Conversion from far preserves the
// complete 16-bit offset so a one-past pointer at a 16 KiB boundary carries
// into the next linear page.
// ASM-LABEL: _data_pointer_to_long:
// ASM:       shr {{r[0-9]+}}, #2
// ASM:       shl {{r[0-9]+}}, #14
// ASM:       shr {{r[0-9]+}}, #2
// ASM:       add
// ASM:       addc
// ASM:       rets
// ASM-LABEL: _long_to_data_pointer:
// ASM:       shl {{r[0-9]+}}, #1
// ASM-NEXT:  addc {{r[0-9]+}}, {{r[0-9]+}}
// ASM-NEXT:  shl {{r[0-9]+}}, #1
// ASM-NEXT:  addc {{r[0-9]+}}, {{r[0-9]+}}
// ASM:       and {{r[0-9]+}}, #16383
// ASM:       or
// ASM:       rets

// Function-pointer casts retain the raw segment:offset words.
// ASM-LABEL: _function_pointer_to_long:
// ASM:       mov r4, r12
// ASM-NEXT:  mov r5, r13
// ASM-NEXT:  rets
// ASM-LABEL: _long_to_function_pointer:
// ASM:       mov r4, r12
// ASM-NEXT:  mov r5, r13
// ASM-NEXT:  rets
// ASM-LABEL: _data_pointer_roundtrip:
// ASM:       and {{r[0-9]+}}, #16383
// ASM:       rets
