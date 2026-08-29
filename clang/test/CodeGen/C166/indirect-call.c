// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-readobj --symbols --relocations %t.o | FileCheck %s --check-prefix=OBJ
// RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS
// C166-ABI: calls.indirect_function_pointer

typedef unsigned int (*fn0_t)(void);
typedef unsigned int (*fn2_t)(unsigned int, unsigned int);
typedef unsigned int (*fn5_t)(unsigned int, unsigned int, unsigned int,
                              unsigned int, unsigned int);

extern unsigned int target(unsigned int, unsigned int);
fn2_t global_target = target;

fn2_t get_target(void) {
  return target;
}

unsigned int call_global(unsigned int a, unsigned int b) {
  return global_target(a, b);
}

void set_global(fn2_t fn) {
  global_target = fn;
}

unsigned int call_indirect0(fn0_t fn) {
  return fn();
}

unsigned int call_indirect2(fn2_t fn, unsigned int a, unsigned int b) {
  return fn(a, b);
}

unsigned int call_indirect5(fn5_t fn, unsigned int a, unsigned int b,
                            unsigned int c, unsigned int d, unsigned int e) {
  return fn(a, b, c, d, e);
}

// The Large-model __icall ABI receives the function pointer in
// R4:R5.  Its C arguments independently use the normal R12-R15/user-stack ABI.
// OBJ-DAG:     R_C166_SOF16 _target
// OBJ-DAG:     R_C166_SEG8 _target
// OBJ-DAG:     R_C166_32 _target
// OBJ-DAG:     R_C166_PAG10 _global_target
// OBJ-DAG:     R_C166_PAG10 _global_target
// OBJ-DAG:     R_C166_POF14 _global_target
// OBJ-DAG:     R_C166_POF14 _global_target
// OBJ-DAG:     R_C166_POF14 _global_target
// OBJ-DAG:     R_C166_POF14 _global_target
// OBJ-DAG:     R_C166_SEG24 __icall
// OBJ-DAG:     R_C166_SEG24 __icall
// OBJ-DAG:     R_C166_SEG24 __icall
// OBJ-DAG:     R_C166_SEG24 __icall
// OBJ-DAG:     Name: __icall
// OBJ-DAG:     Name: _get_target
// OBJ-DAG:     Name: _call_indirect0
// OBJ-DAG:     Name: _call_indirect2
// OBJ-DAG:     Name: _call_indirect5

// DIS-LABEL: <_get_target>:
// DIS-NEXT:  mov r4, #0
// DIS-NEXT:  mov r5, #0
// DIS-NEXT:  rets

// DIS-LABEL: <_call_global>:
// DIS:       extp 0, #2
// DIS-NEXT:  mov r4, 0
// DIS-NEXT:  mov r5, 0
// DIS-NEXT:  calls
// DIS-NEXT:  rets

// DIS-LABEL: <_set_global>:
// DIS:       extp 0, #2
// DIS-NEXT:  mov 0, r12
// DIS-NEXT:  mov 0, r13
// DIS-NEXT:  rets

// DIS-LABEL: <_call_indirect0>:
// DIS-NEXT:  mov r4, r12
// DIS-NEXT:  mov r5, r13
// DIS-NEXT:  calls
// DIS-NEXT:  rets

// DIS-LABEL: <_call_indirect2>:
// DIS:       mov r4, r12
// DIS-NEXT:  mov r5, r13
// DIS-NEXT:  mov r12, r14
// DIS-NEXT:  mov r13, r15
// DIS-NEXT:  calls
// DIS-NEXT:  rets

// DIS-LABEL: <_call_indirect5>:
// DIS:       mov r4, r12
// DIS-NEXT:  mov r5, r13
// DIS:       mov [[LAST:r[0-9]+]], [r0 + #4]
// DIS-NEXT:  mov [-r0], [[LAST]]
// DIS-NEXT:  mov r14, [r0 + #2]
// DIS-NEXT:  mov r15, [r0 + #4]
// DIS:       calls
// DIS-NEXT:  add r0, #2
// DIS-NEXT:  rets
