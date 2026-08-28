// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: llvm-objdump -d %t-o0.o | FileCheck %s --check-prefixes=ADDR,OFFSET
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t-o1.o
// RUN: llvm-objdump -d %t-o1.o | FileCheck %s --check-prefix=ADDR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t-o2.o
// C166-ABI: stack.escaped_address
// C166-ABI: stack.fixed_offset_256

typedef unsigned int u16;

extern u16 read_stack_pointer(volatile u16 *pointer);

u16 pass_local_address(u16 value) {
  volatile u16 local = value;
  return read_stack_pointer(&local);
}

u16 index_local_array(u16 index, u16 value) {
  volatile u16 words[8];
  words[index & 7] = value;
  return words[index & 7];
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
// ADDR:       mov {{r[0-9]+}}, #16383
// ADDR:       and
// ADDR:       mov {{r[0-9]+}}, dpp1
// ADDR:       calls
// ADDR:       rets

// A dynamically indexed automatic array must use the same DPP1 far address.
// ADDR-LABEL: <_index_local_array>:
// ADDR:       mov {{r[0-9]+}}, #16383
// ADDR:       and
// ADDR:       mov {{r[0-9]+}}, dpp1
// ADDR:       extp {{r[0-9]+}}, #1
// ADDR:       mov [{{r[0-9]+}}], {{r[0-9]+}}
// ADDR:       extp {{r[0-9]+}}, #1
// ADDR:       mov r4, [{{r[0-9]+}}]
// ADDR:       rets

// A constant in-page displacement remains a direct user-stack access.
// OFFSET-LABEL: <_access_stack_offset_256>:
// OFFSET:       mov [r0 + #256], {{r[0-9]+}}
// OFFSET-NEXT:  mov r4, [r0 + #256]
// OFFSET-NOT:   extp
// OFFSET:       rets
