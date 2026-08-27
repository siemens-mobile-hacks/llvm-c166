// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -S %s -o %t-large.s
// RUN: FileCheck %s --check-prefix=TEXT < %t-large.s
// RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t-large.s -o %t-large.o
// RUN: llvm-readobj --relocations %t-large.o | FileCheck %s --check-prefix=LARGE
// RUN: llvm-objdump -s --section=.data %t-large.o | FileCheck %s --check-prefix=ABSOLUTE
// RUN: %clang --target=c166-none-elf -mcmodel=medium -c %s -o %t-medium.o
// RUN: llvm-readobj --relocations %t-medium.o | FileCheck %s --check-prefix=MEDIUM
// RUN: %clang --target=c166-none-elf -mcmodel=small -c %s -o %t-small.o
// RUN: llvm-readobj --relocations %t-small.o | FileCheck %s --check-prefix=SMALL

typedef int __attribute__((c166_far)) far_int;
typedef int __attribute__((c166_huge)) huge_int;

int object;
int array[4];
far_int far_object;
huge_int huge_object;

int *object_pointer = &object;
int *array_pointer = &array[3];
const char *string_pointer = "C166";
far_int *far_pointer = &far_object;
huge_int *huge_pointer = &huge_object;

int function(void) { return 1; }
int (*function_pointer)(void) = function;

struct holder {
  int *pointer;
  unsigned long marker;
};

struct holder aggregate = {&object, 0x12345678UL};
int *pointer_array[2] = {&object, &array[1]};
int *absolute_pointer = (int *)0x500074UL;

// TEXT: .long paged(_object)
// TEXT: .long paged(_array+6)
// TEXT: .long paged({{.*}}str)
// TEXT: .long paged(_far_object)
// TEXT: .long _huge_object
// TEXT: .long _function
// TEXT: .long paged(_object)
// TEXT: .long paged(_object)
// TEXT: .long paged(_array+2)
// TEXT: .long paged(5242996)

// ABSOLUTE: 74004001

// LARGE-DAG: R_C166_PAGED32 _object
// LARGE-DAG: R_C166_PAGED32 _array 0x6
// LARGE-DAG: R_C166_PAGED32 {{.*}}rodata
// LARGE-DAG: R_C166_PAGED32 _far_object
// LARGE-DAG: R_C166_32 _huge_object
// LARGE-DAG: R_C166_32 _function
// LARGE-DAG: R_C166_PAGED32 _object
// LARGE-DAG: R_C166_PAGED32 _object
// LARGE-DAG: R_C166_PAGED32 _array 0x2

// MEDIUM-DAG: R_C166_PAGED32 _object
// MEDIUM-DAG: R_C166_PAGED32 _array 0x6
// MEDIUM-DAG: R_C166_PAGED32 {{.*}}rodata
// MEDIUM-DAG: R_C166_PAGED32 _far_object
// MEDIUM-DAG: R_C166_32 _huge_object
// MEDIUM-DAG: R_C166_16 _function
// MEDIUM-DAG: R_C166_PAGED32 _object
// MEDIUM-DAG: R_C166_PAGED32 _object
// MEDIUM-DAG: R_C166_PAGED32 _array 0x2

// SMALL-DAG: R_C166_16 _object
// SMALL-DAG: R_C166_16 _array 0x6
// SMALL-DAG: R_C166_16 {{.*}}rodata
// SMALL-DAG: R_C166_PAGED32 _far_object
// SMALL-DAG: R_C166_32 _huge_object
// SMALL-DAG: R_C166_32 _function
// SMALL-DAG: R_C166_16 _object
// SMALL-DAG: R_C166_16 _object
// SMALL-DAG: R_C166_16 _array 0x2
