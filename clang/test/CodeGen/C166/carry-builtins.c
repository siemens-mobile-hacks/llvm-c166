// RUN: %clang_cc1 -triple c166-none-elf -O0 -S -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple c166-none-elf -O2 -S -o /dev/null %s

unsigned int add_with_carry(unsigned int left, unsigned int right,
                            unsigned int carry_in) {
  unsigned int carry_out;
  unsigned int result = __builtin_addc(left, right, carry_in, &carry_out);
  return result ^ carry_out;
}

unsigned long subtract_with_carry(unsigned long left, unsigned long right,
                                  unsigned long carry_in) {
  unsigned long carry_out;
  unsigned long result = __builtin_subcl(left, right, carry_in, &carry_out);
  return result ^ carry_out;
}

// CHECK-LABEL: add_with_carry:
// CHECK: addc
// CHECK-LABEL: subtract_with_carry:
// CHECK: subc
