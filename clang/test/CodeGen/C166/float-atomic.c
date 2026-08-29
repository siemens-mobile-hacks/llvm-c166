// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-nm -u %t.o | FileCheck %s --check-prefix=NM
// RUN: llvm-objdump -d -r %t.o | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -DUNSUPPORTED -Wno-atomic-alignment -c %s -o %t-atomic.o
// RUN: llvm-nm -u %t-atomic.o | FileCheck %s --check-prefix=ATOMIC-NM

_Atomic(float) atomic_float;
_Atomic(double) atomic_double;

void init_atomic_float(float value) {
  __c11_atomic_init(&atomic_float, value);
}

void init_atomic_double(double value) {
  __c11_atomic_init(&atomic_double, value);
}

int float_is_lock_free(void) {
  return __c11_atomic_is_lock_free(sizeof(atomic_float));
}

int double_is_lock_free(void) {
  return __c11_atomic_is_lock_free(sizeof(atomic_double));
}

// The query is a runtime call using the C166 size_t ABI. C166's
// compiler-rt implementation returns false for every size.
// NM: U ___atomic_is_lock_free

// atomic_init is explicitly non-atomic and stores public MSW-first words.
// ASM-LABEL: <_init_atomic_float>:
// ASM:       R_C166_POF14 _atomic_float
// ASM:       R_C166_POF14 _atomic_float+0x2
// ASM-LABEL: <_init_atomic_double>:
// ASM-DAG:   R_C166_POF14 _atomic_double{{$}}
// ASM-DAG:   R_C166_POF14 _atomic_double+0x2{{$}}
// ASM-DAG:   R_C166_POF14 _atomic_double+0x4{{$}}
// ASM-DAG:   R_C166_POF14 _atomic_double+0x6{{$}}

#ifdef UNSUPPORTED
float load_atomic_float(void) {
  return __c11_atomic_load(&atomic_float, __ATOMIC_SEQ_CST);
}

void store_atomic_float(float value) {
  __c11_atomic_store(&atomic_float, value, __ATOMIC_SEQ_CST);
}

double exchange_atomic_double(double value) {
  return __c11_atomic_exchange(&atomic_double, value, __ATOMIC_SEQ_CST);
}
#endif

// Multi-word floating accesses use the C166 runtime after physical
// MSW-first storage lowering.  They remain non-lock-free, but are no longer
// split or rejected.
// ATOMIC-NM: U ___atomic_load
// ATOMIC-NM: U ___atomic_store
// ATOMIC-NM: U ___c166_atomic_rmw
