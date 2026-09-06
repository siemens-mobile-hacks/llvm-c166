// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: llvm-objdump -d %t-o0.o | FileCheck %s --check-prefixes=ADDR,OFFSET
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t-o1.o
// RUN: llvm-objdump -d %t-o1.o | FileCheck %s --check-prefixes=ADDR,XNEAR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t-o2.o
// C166-ABI: stack.escaped_address
// C166-ABI: stack.fixed_offset_256

typedef unsigned int u16;
typedef unsigned char u8;
typedef unsigned long u32;

extern u16 read_stack_pointer(volatile u16 *pointer);

#if __C166_MEMORY_MODEL__ != 3
typedef volatile u16 __attribute__((c166_xnear)) *xnear_u16_ptr;
extern u16 read_xnear_stack_pointer(xnear_u16_ptr pointer);
#endif

u16 pass_local_address(u16 value) {
  volatile u16 local = value;
  return read_stack_pointer(&local);
}

#if __C166_MEMORY_MODEL__ != 3
u16 pass_local_xnear_address(u16 value) {
  volatile u16 local = value;
  return read_xnear_stack_pointer((xnear_u16_ptr)&local);
}
#endif

u16 index_local_array(u16 index, u16 value) {
  volatile u16 words[8];
  words[index & 7] = value;
  return words[index & 7];
}

u8 index_local_byte_array(u16 index, u8 value) {
  volatile u8 bytes[8];
  bytes[index & 7] = value;
  return bytes[index & 7];
}

u32 index_local_long_array(u16 index, u32 value) {
  volatile u32 longs[8];
  longs[index & 7] = value;
  return longs[index & 7];
}

u16 pass_indexed_local_address(u16 index, u16 value) {
  volatile u16 words[8];
  words[index & 7] = value;
  return read_stack_pointer(&words[index & 7]);
}

struct stack_offset_256 {
  unsigned char padding[256];
  u16 value;
};

u16 access_stack_offset_256(u16 value) {
  volatile struct stack_offset_256 local;
  local.value = value;
  return local.value;
}

// An automatic object's address converts to a default far
// pointer as { R0 & 0x3fff, DPP1 }.  A zero high word would incorrectly place
// the user stack in page zero.
// ADDR-LABEL: <_pass_local_address>:
// ADDR:       and {{r[0-9]+}}, #16383
// ADDR:       mov {{r[0-9]+}}, dpp1
// ADDR:       calls
// ADDR:       rets

// A DPP1 xnear cast of a frame object is already represented by its direct
// user-stack address.  It must not reconstruct a far pointer or force a second
// DPP selector around that address.
// XNEAR-LABEL: <_pass_local_xnear_address>:
// XNEAR-NOT:   dpp1
// XNEAR-NOT:   and
// XNEAR-NOT:   or
// XNEAR:       mov r12, r0
// XNEAR:       calls
// XNEAR:       rets

// A dynamically indexed automatic array stays on the compact direct stack
// path. Its address becomes far only when the C pointer is observable.
// ADDR-LABEL: <_index_local_array>:
// ADDR-NOT:   dpp1
// ADDR-NOT:   extp
// ADDR:       mov [{{r[0-9]+}}], {{r[0-9]+}}
// ADDR:       mov r4, [{{r[0-9]+}}]
// ADDR-NOT:   dpp1
// ADDR-NOT:   extp
// ADDR:       rets

// ADDR-LABEL: <_index_local_byte_array>:
// ADDR-NOT:   extp
// ADDR:       movb [{{r[0-9]+}}], {{r[lh][0-7]}}
// ADDR:       movb {{r[lh][0-7]}}, [{{r[0-9]+}}]
// ADDR-NOT:   extp
// ADDR:       rets

// ADDR-LABEL: <_index_local_long_array>:
// ADDR-NOT:   extp
// ADDR:       mov [{{r[0-9]+}}], {{r[0-9]+}}
// ADDR:       mov [{{r[0-9]+}} + #2], {{r[0-9]+}}
// ADDR:       mov r4, [{{r[0-9]+}}]
// ADDR:       mov r5, [{{r[0-9]+}} + #2]
// ADDR-NOT:   extp
// ADDR:       rets

// The same indexed object is still passed as the ABI's DPP1 far pointer.
// ADDR-LABEL: <_pass_indexed_local_address>:
// ADDR:       mov [{{r[0-9]+}}], {{r[0-9]+}}
// ADDR:       and {{r[0-9]+}}, #16383
// ADDR:       mov {{r[0-9]+}}, dpp1
// ADDR:       calls
// ADDR:       rets

// A constant in-page displacement remains a direct user-stack access.
// OFFSET-LABEL: <_access_stack_offset_256>:
// OFFSET:       mov [r0 + #256], {{r[0-9]+}}
// OFFSET-NEXT:  mov r4, [r0 + #256]
// OFFSET-NOT:   extp
// OFFSET:       rets
