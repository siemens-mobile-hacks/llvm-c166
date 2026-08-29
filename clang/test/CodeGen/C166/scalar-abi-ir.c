// REQUIRES: c166-registered-target
// RUN: %clang_cc1 -triple c166-none-elf -emit-llvm -o - %s | FileCheck %s

signed char return_signed_char(signed char value) { return value; }
unsigned char return_unsigned_char(unsigned char value) { return value; }
short return_short(short value) { return value; }
unsigned short return_unsigned_short(unsigned short value) { return value; }
int return_int(int value) { return value; }
unsigned int return_unsigned_int(unsigned int value) { return value; }

// Integer arguments and results use their natural C166 register width.  No
// LLVM signext/zeroext contract may make a caller depend on another register.
// CHECK: define dso_local i8 @return_signed_char(i8 noundef %value)
// CHECK: define dso_local i8 @return_unsigned_char(i8 noundef %value)
// CHECK: define dso_local i16 @return_short(i16 noundef %value)
// CHECK: define dso_local i16 @return_unsigned_short(i16 noundef %value)
// CHECK: define dso_local i16 @return_int(i16 noundef %value)
// CHECK: define dso_local i16 @return_unsigned_int(i16 noundef %value)
