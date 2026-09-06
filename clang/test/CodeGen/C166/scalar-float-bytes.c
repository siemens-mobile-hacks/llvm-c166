// RUN: %clang --target=c166-none-elf -O2 -S -o - %s | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O2 -S -o - %s | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=small -O2 -S -o - %s | FileCheck %s
// RUN: %clang --target=c166-none-elf -O3 -S -o - %s | FileCheck %s
// RUN: %clang --target=c166-none-elf -Oz -S -o - %s | FileCheck %s
// RUN: %clang --target=c166-none-elf -O2 -S -emit-llvm -o - %s | FileCheck %s --check-prefix=IR

// Byte observations must see the ABI representation even for scalar objects.
unsigned int local_float_word(void) {
  float value = 1.0f;
  unsigned int word;
  __builtin_memcpy(&word, &value, sizeof(word));
  return word;
}
// CHECK-LABEL: local_float_word:
// CHECK: {{16256|0x3f80}}

static const float constant_value = 1.0f;
unsigned int global_float_word(void) {
  unsigned int word;
  __builtin_memcpy(&word, &constant_value, sizeof(word));
  return word;
}
// CHECK-LABEL: global_float_word:
// CHECK: mov r4, #16256

unsigned int local_double_word(void) {
  double value = 1.0;
  unsigned int word;
  __builtin_memcpy(&word, &value, sizeof(word));
  return word;
}
// CHECK-LABEL: local_double_word:
// CHECK: {{16368|0x3ff0}}

static const double constant_double = 1.0;
unsigned int global_double_word(void) {
  unsigned int word;
  __builtin_memcpy(&word, &constant_double, sizeof(word));
  return word;
}
// CHECK-LABEL: global_double_word:
// CHECK: mov r4, #16368

// A byval attribute alone does not make scalar storage type-exclusive.
// IR-LABEL: define{{.*}} @observed_parameter(
// IR: call{{.*}} double @llvm.c166.float.load.f64
double observed_parameter(double value) {
  unsigned int word;
  __builtin_memcpy(&word, &value, sizeof(word));
  return value + word;
}

static unsigned int first_word(double value) {
  unsigned int word;
  __builtin_memcpy(&word, &value, sizeof(word));
  return word;
}

unsigned int inlined_double_word(void) { return first_word(1.0); }
// CHECK-LABEL: inlined_double_word:
// CHECK: mov r4, #16368
