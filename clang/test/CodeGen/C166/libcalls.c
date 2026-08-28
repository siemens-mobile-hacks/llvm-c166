// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-medium.o
// RUN: llvm-objdump -dr %t-medium.o | FileCheck %s --check-prefix=MEDIUM
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-large.o
// RUN: llvm-objdump -dr %t-large.o | FileCheck %s --check-prefix=HUGE
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-small.o
// RUN: llvm-objdump -dr %t-small.o | FileCheck %s --check-prefix=HUGE

long mul_long(long a, long b) { return a * b; }
long shift_long(long value, unsigned int amount) { return value << amount; }
float add_float(float a, float b) { return a + b; }
double add_double(double a, double b) { return a + b; }

_Atomic(unsigned long) atomic_long;
unsigned long load_atomic_long(void) {
  return __c11_atomic_load(&atomic_long, __ATOMIC_SEQ_CST);
}

// C166-ABI: medium.runtime.default_helpers_near
// The Medium model declares integer and floating runtime helpers NEAR
// and reaches them with CALLA.  SelectionDAG synthesizes these calls without
// an IR CallBase, so this is also a regression for model-aware synthetic
// external-symbol lowering.
// MEDIUM-LABEL: <_mul_long>:
// MEDIUM:       calla
// MEDIUM:       R_C166_COF16 ___mulsi3
// MEDIUM-NOT:   R_C166_SEG8 ___mulsi3
// MEDIUM-LABEL: <_shift_long>:
// MEDIUM:       calla
// MEDIUM:       R_C166_COF16 ___ashlsi3
// MEDIUM-NOT:   R_C166_SEG8 ___ashlsi3
// MEDIUM-LABEL: <_add_float>:
// MEDIUM:       calla
// MEDIUM:       R_C166_COF16 ___addsf3
// MEDIUM-NOT:   R_C166_SEG8 ___addsf3
// MEDIUM-LABEL: <_add_double>:
// MEDIUM:       calla
// MEDIUM:       R_C166_COF16 ___adddf3
// MEDIUM-NOT:   R_C166_SEG8 ___adddf3
// MEDIUM-LABEL: <_load_atomic_long>:
// MEDIUM:       calla
// MEDIUM:       R_C166_COF16 ___atomic_load
// MEDIUM-NOT:   R_C166_SEG8 ___atomic_load

// The same helpers remain inter-segment ordinary functions in Large and Small.
// HUGE-LABEL: <_mul_long>:
// HUGE:       calls
// HUGE:       R_C166_SEG8 ___mulsi3
// HUGE-NEXT:  {{.*}}R_C166_SOF16 ___mulsi3
// HUGE-LABEL: <_shift_long>:
// HUGE:       calls
// HUGE:       R_C166_SEG8 ___ashlsi3
// HUGE-NEXT:  {{.*}}R_C166_SOF16 ___ashlsi3
// HUGE-LABEL: <_add_float>:
// HUGE:       calls
// HUGE:       R_C166_SEG8 ___addsf3
// HUGE-NEXT:  {{.*}}R_C166_SOF16 ___addsf3
// HUGE-LABEL: <_add_double>:
// HUGE:       calls
// HUGE:       R_C166_SEG8 ___adddf3
// HUGE-NEXT:  {{.*}}R_C166_SOF16 ___adddf3
// HUGE-LABEL: <_load_atomic_long>:
// HUGE:       calls
// HUGE:       R_C166_SEG8 ___atomic_load
// HUGE-NEXT:  {{.*}}R_C166_SOF16 ___atomic_load
