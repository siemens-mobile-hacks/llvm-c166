// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: llvm-objdump -d %t-o0.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t-o2.o

unsigned char local_byte(unsigned char value) {
  volatile unsigned char local = value;
  return local;
}

unsigned long local_byte_zext(unsigned char value) {
  volatile unsigned char local = value;
  return local;
}

long local_byte_sext(signed char value) {
  volatile signed char local = value;
  return local;
}

unsigned long local_word_zext(unsigned int value) {
  volatile unsigned int local = value;
  return local;
}

long local_word_sext(int value) {
  volatile int local = value;
  return local;
}

volatile unsigned int extension_global;
volatile signed char extension_signed_byte;
volatile unsigned char extension_unsigned_byte;

void truncating_global_word_store(unsigned long value) {
  extension_global = value;
}

void truncating_global_width_stores(unsigned long value) {
  extension_signed_byte = value;
  extension_unsigned_byte = value;
  extension_global = value;
}

long mixed_casts(signed char sc, unsigned char uc, int i, unsigned int ui,
                 long l) {
  return (long)sc + (unsigned long)uc + (long)i + (unsigned long)ui +
         (signed char)l + (unsigned char)l + (int)l + (unsigned int)l;
}

unsigned int local_byte_array(unsigned char value) {
  volatile unsigned char bytes[3];
  bytes[0] = value;
  bytes[1] = value + 1;
  bytes[2] = value + 2;
  return bytes[0] + bytes[1] + bytes[2];
}

// Byte locals are addressed directly through the R0 user stack.
// An EXTP sequence here would incorrectly reinterpret a near frame address as
// a paged far address.
// CHECK-LABEL: <_local_byte>:
// CHECK-NOT:   extp
// CHECK:       movb [r0{{.*}}], {{r[lh][0-7]}}
// CHECK:       movb {{r[lh][0-7]}}, [r0{{.*}}]
// CHECK-NOT:   extp
// CHECK:       rets

// CHECK-LABEL: <_local_byte_zext>:
// CHECK-NOT:   extp
// CHECK:       movbz [[BYTE_VALUE:r[0-9]+]],
// CHECK:       mov r4, [[BYTE_VALUE]]
// CHECK-NEXT:  mov r5, #0
// CHECK-NOT:   extp
// CHECK:       rets

// CHECK-LABEL: <_local_byte_sext>:
// CHECK-NOT:   extp
// CHECK:       movbs [[BYTE_SIGN:r[0-9]+]],
// CHECK:       mov r4, [[BYTE_SIGN]]
// CHECK-NEXT:  mov r5, r4
// CHECK-NEXT:  ashr r5, #15
// CHECK-NOT:   extp
// CHECK:       rets

// CHECK-LABEL: <_local_word_zext>:
// CHECK:       mov [[WORD_VALUE:r[0-9]+]], [r0{{.*}}]
// CHECK:       mov r4, [[WORD_VALUE]]
// CHECK-NEXT:  mov r5, #0
// CHECK:       rets

// CHECK-LABEL: <_local_word_sext>:
// CHECK:       mov [[WORD_SIGN:r[0-9]+]], [r0{{.*}}]
// CHECK:       mov r4, [[WORD_SIGN]]
// CHECK-NEXT:  mov r5, r4
// CHECK-NEXT:  ashr r5, #15
// CHECK:       rets

// CHECK-LABEL: <_local_byte_array>:
// CHECK-NOT:   extp
// CHECK:       movb [r0 + #{{[0-9]+}}], {{r[lh][0-7]}}
// CHECK:       movb {{r[lh][0-7]}}, [r0 + #{{[0-9]+}}]
// CHECK-NOT:   extp
// CHECK:       rets
