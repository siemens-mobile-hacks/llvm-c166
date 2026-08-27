// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t-o2.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -Oz -mllvm -verify-machineinstrs -c %s -o %t-oz.o

// Keep promoted signed and unsigned words, a long, and a far pointer live at
// once.  The overlapping GR32 class must not spill a tuple before the high
// half produced by sign/zero extension has been defined.

#include <stdarg.h>

__attribute__((noinline))
unsigned long varargs_extension_pressure(unsigned int fixed0,
                                         unsigned int fixed1,
                                         unsigned int fixed2,
                                         unsigned int fixed3, ...) {
  va_list arguments;
  int signed_value;
  unsigned int unsigned_byte;
  unsigned int enum_value;
  unsigned int word_value;
  unsigned long long_value;
  volatile unsigned int *pointer_value;
  unsigned long result;

  va_start(arguments, fixed3);
  signed_value = va_arg(arguments, int);
  unsigned_byte = (unsigned int)va_arg(arguments, int);
  enum_value = (unsigned int)va_arg(arguments, int);
  word_value = va_arg(arguments, unsigned int);
  long_value = va_arg(arguments, unsigned long);
  pointer_value = va_arg(arguments, volatile unsigned int *);
  va_end(arguments);

  result = long_value + fixed0 + (unsigned long)fixed1 * 3UL +
           (unsigned long)fixed2 * 5UL + (unsigned long)fixed3 * 7UL +
           signed_value;
  result ^= (unsigned long)unsigned_byte << 8;
  result += (unsigned long)enum_value * 0x101UL;
  result ^= (unsigned long)word_value << 16;
  result += *pointer_value;
  return result;
}
