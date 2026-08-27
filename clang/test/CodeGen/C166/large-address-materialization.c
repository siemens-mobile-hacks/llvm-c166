// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=OBJ
// RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

const unsigned char message[] = "OK";
extern unsigned int target(void);

const unsigned char *get_message(void) {
  return message;
}

unsigned int (*get_target(void))(void) {
  return target;
}

// Large-model data pointers contain POF14 in the low word and PAG10 in the
// high word.  Function pointers remain SOF16:SEG8 and must not share the data
// address pseudo merely because both are represented as LLVM i32 values.
// OBJ-DAG:  R_C166_POF14 _message
// OBJ-DAG:  R_C166_PAG10 _message
// OBJ-DAG:  R_C166_SOF16 _target
// OBJ-DAG:  R_C166_SEG8 _target

// DIS-LABEL: <_get_message>:
// DIS-NEXT:  mov r4, #0
// DIS-NEXT:  mov r5, #0
// DIS-NEXT:  rets
// DIS-LABEL: <_get_target>:
// DIS-NEXT:  mov r4, #0
// DIS-NEXT:  mov r5, #0
// DIS-NEXT:  rets
