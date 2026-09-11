// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=tiny -O1 -mllvm -verify-machineinstrs -c %s -o %t.tiny.o
// RUN: %clang --target=c166-none-elf -mcmodel=huge -O1 -mllvm -verify-machineinstrs -c %s -o %t.huge.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-readobj --file-header --symbols --relocations %t.o | FileCheck %s --check-prefix=OBJ
// RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t-medium.o
// RUN: llvm-objdump -dr %t-medium.o | FileCheck %s --check-prefix=MEDIUM
// C166-ABI: calls.fifth_word_cleanup
// C166-ABI: pointers.far_argument_words
// C166-ABI: pointers.far_return_words
// C166-ABI: medium.ordinary.scalar_stack_result

extern unsigned long callee_mix(unsigned int, unsigned long, unsigned int);
extern unsigned int callee_five_words(unsigned int, unsigned int, unsigned int,
                                      unsigned int, unsigned int);
extern unsigned int far_global;
extern volatile unsigned char far_byte_global;
extern volatile signed char far_signed_byte_global;

unsigned int add_words(unsigned int lhs, unsigned int rhs) {
  return lhs + rhs;
}

unsigned long add_mixed(unsigned long base, unsigned int delta) {
  return base + delta;
}

unsigned int load_far(const unsigned int *address) {
  return *address;
}

void store_far(unsigned int *address, unsigned int value) {
  *address = value;
}

unsigned char load_far_byte(const unsigned char *address) {
  return *address;
}

void store_far_byte(unsigned char *address, unsigned char value) {
  *address = value;
}

unsigned long load_far_long(const unsigned long *address) {
  return *address;
}

unsigned long load_far_long_keep_address(const unsigned long *address) {
  return *address + (unsigned long)address;
}

void store_far_long(unsigned long *address, unsigned long value) {
  *address = value;
}

unsigned int *next_far_word(unsigned int *address) {
  return address + 1;
}

unsigned int load_far_index(const unsigned int *address, unsigned int index) {
  return address[index];
}

unsigned int load_global(void) {
  return far_global;
}

void store_global(unsigned int value) {
  far_global = value;
}

unsigned char load_global_byte(void) {
  return far_byte_global;
}

void store_global_byte(unsigned char value) {
  far_byte_global = value;
}

int extend_global_signed_byte(void) {
  return far_signed_byte_global;
}

unsigned long forward_mix(unsigned int first, unsigned long second,
                          unsigned int third) {
  return callee_mix(first, second, third);
}

unsigned int forward_five_words(unsigned int a, unsigned int b, unsigned int c,
                                unsigned int d, unsigned int e) {
  return callee_five_words(a, b, c, d, e);
}

// OBJ:      Format: elf32-c166
// OBJ:      Machine: EM_C166 (0x74)
// OBJ:      Flags [ (0x121)
// OBJ-DAG:  R_C166_PAG10 _far_global
// OBJ-DAG:  R_C166_POF14 _far_global
// OBJ-DAG:  R_C166_PAG10 _far_byte_global
// OBJ-DAG:  R_C166_POF14 _far_byte_global
// OBJ-DAG:  R_C166_PAG10 _far_signed_byte_global
// OBJ-DAG:  R_C166_POF14 _far_signed_byte_global
// OBJ:      R_C166_SEG24 _callee_mix
// OBJ-DAG:    Name: _add_words
// OBJ-DAG:    Name: _add_mixed
// OBJ-DAG:    Name: _load_far
// OBJ-DAG:    Name: _store_far
// OBJ-DAG:    Name: _load_far_byte
// OBJ-DAG:    Name: _store_far_byte
// OBJ-DAG:    Name: _load_far_long
// OBJ-DAG:    Name: _load_far_long_keep_address
// OBJ-DAG:    Name: _store_far_long
// OBJ-DAG:    Name: _next_far_word
// OBJ-DAG:    Name: _load_far_index
// OBJ-DAG:    Name: _load_global
// OBJ-DAG:    Name: _store_global
// OBJ-DAG:    Name: _load_global_byte
// OBJ-DAG:    Name: _store_global_byte
// OBJ-DAG:    Name: _extend_global_signed_byte
// OBJ-DAG:    Name: _forward_mix
// OBJ-DAG:    Name: _forward_five_words
// OBJ-DAG:    Name: _callee_mix
// OBJ-DAG:    Name: _callee_five_words

// DIS-LABEL: <_add_words>:
// DIS:       mov r4, r{{1[23]}}
// DIS-NEXT:  add r4, r{{1[23]}}
// DIS-NEXT:  rets

// Medium preserves scalar/pointer registers and the stack-stop rule. Default
// functions use near control flow; data pointers remain 32-bit far.
// MEDIUM-LABEL: <_add_words>:
// MEDIUM:       mov r4, r{{1[23]}}
// MEDIUM:       ret
// MEDIUM-LABEL: <_add_mixed>:
// MEDIUM:       mov r4, r14
// MEDIUM:       mov r5, #0
// MEDIUM:       add r4, r12
// MEDIUM:       addc r5, r13
// MEDIUM:       ret
// MEDIUM-LABEL: <_load_far>:
// MEDIUM:       extp r13, #1
// MEDIUM-NEXT:  mov r4, [r12]
// MEDIUM-NEXT:  ret
// MEDIUM-LABEL: <_forward_mix>:
// MEDIUM:       jmpa
// MEDIUM:       R_C166_COF16 _callee_mix
// MEDIUM-LABEL: <_forward_five_words>:
// MEDIUM:       mov [[ARG:r[0-9]+]], [r0]
// MEDIUM-NEXT:  mov [-r0], [[ARG]]
// MEDIUM:       calla
// MEDIUM:       R_C166_COF16 _callee_five_words
// MEDIUM:       add r0, #2
// MEDIUM-NEXT:  ret
// DIS-LABEL: <_add_mixed>:
// DIS:       mov r4, r14
// DIS-NEXT:  mov r5, #0
// DIS-NEXT:  add r4, r12
// DIS-NEXT:  addc r5, r13
// DIS-NEXT:  rets
// DIS-LABEL: <_load_far>:
// DIS:       extp r13, #1
// DIS-NEXT:  mov r4, [r12]
// DIS-NEXT:  rets
// DIS-LABEL: <_store_far>:
// DIS:       extp r13, #1
// DIS-NEXT:  mov [r12], r14
// DIS-NEXT:  rets
// DIS-LABEL: <_load_far_byte>:
// DIS:       extp r13, #1
// DIS-NEXT:  movb [[BYTE:r[lh][0-7]]], [r12]
// DIS-NEXT:  movbz r4, [[BYTE]]
// DIS-NEXT:  rets
// DIS-LABEL: <_store_far_byte>:
// DIS:       extp r13, #1
// DIS-NEXT:  movb [r12], r{{[lh][0-7]}}
// DIS-NEXT:  rets
// DIS-LABEL: <_load_far_long>:
// DIS:       extp r13, #2
// DIS-NEXT:  mov r4, [r12+]
// DIS-NEXT:  mov r5, [r12]
// DIS-NEXT:  rets
// DIS-LABEL: <_load_far_long_keep_address>:
// Casting the data pointer to long uses the default huge/linear value,
// while the load still uses the original far page and offset.
// DIS:       shr {{r[0-9]+}}, #2
// DIS:       shl {{r[0-9]+}}, #14
// DIS:       shr {{r[0-9]+}}, #2
// DIS:       extp [[KEEP_PAGE:r[0-9]+]], #2
// DIS-NEXT:  mov r4, [{{r[0-9]+}}+]
// DIS-NEXT:  mov r5, [{{r[0-9]+}}]
// DIS:       add r4, {{r[0-9]+}}
// DIS-NEXT:  addc r5, {{r[0-9]+}}
// DIS:       rets
// DIS-LABEL: <_store_far_long>:
// DIS:       extp r13, #2
// DIS-NEXT:  mov [r12 + #2], r15
// DIS-NEXT:  mov [r12], r14
// DIS-NEXT:  rets
// DIS-LABEL: <_next_far_word>:
// DIS:       mov r4, r12
// DIS-NEXT:  mov r5, r13
// DIS-NEXT:  add r4, #2
// DIS-NEXT:  rets
// DIS-LABEL: <_load_far_index>:
// DIS:       shl r14, #1
// DIS-NEXT:  add r12, r14
// DIS-NEXT:  extp r13, #1
// DIS-NEXT:  mov r4, [r12]
// DIS-NEXT:  rets
// DIS-LABEL: <_load_global>:
// DIS:       extp 0, #1
// DIS-NEXT:  mov r4, 0
// DIS-NEXT:  rets
// DIS-LABEL: <_store_global>:
// DIS:       extp 0, #1
// DIS-NEXT:  mov 0, r12
// DIS-NEXT:  rets
// DIS-LABEL: <_load_global_byte>:
// DIS:       extp 0, #1
// DIS-NEXT:  movbz r4, 0
// DIS-NEXT:  rets
// DIS-LABEL: <_store_global_byte>:
// DIS:       extp 0, #1
// DIS-NEXT:  movb 0, rl{{[0-7]}}
// DIS-NEXT:  rets
// DIS-LABEL: <_extend_global_signed_byte>:
// DIS:       extp 0, #1
// DIS-NEXT:  movbs r4, 0
// DIS-NEXT:  rets
// DIS-LABEL: <_forward_mix>:
// DIS:       jmps
// DIS-LABEL: <_forward_five_words>:
// DIS:       mov [[ARG:r[0-9]+]], [r0]
// DIS-NEXT:  mov [-r0], [[ARG]]
// DIS-NEXT:  calls
// DIS-NEXT:  add r0, #2
// DIS-NEXT:  rets
