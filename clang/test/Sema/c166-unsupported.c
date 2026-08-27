// RUN: %clang_cc1 -triple c166-none-elf -fsyntax-only -verify %s

int musttail_callee(int value);

int musttail_caller(int value) {
  __attribute__((musttail)) // expected-warning {{unknown attribute 'musttail' ignored}}
  return musttail_callee(value);
}

int __attribute__((stdcall)) // expected-warning {{'stdcall' calling convention is not supported for this target}}
stdcall_definition(int value) {
  return value + 1;
}

int __attribute__((fastcall)) // expected-warning {{'fastcall' calling convention is not supported for this target}}
fastcall_declaration(int value);

typedef int generic_vector __attribute__((vector_size(4))); // expected-error {{fixed-size vector types are not supported on this target}}
typedef int extended_vector __attribute__((ext_vector_type(2))); // expected-error {{fixed-size vector types are not supported on this target}}

_Float16 unsupported_float16; // expected-error {{_Float16 is not supported on this target}}
__int128 unsupported_int128; // expected-error {{__int128 is not supported on this target}}

int vla(int count) {
  int values[count]; // expected-error {{variable length arrays are not supported for the current target}}
  return values[0];
}

void *dynamic_alloca(unsigned count) {
  return __builtin_alloca(count); // expected-error {{builtin is not supported on this target}}
}

void invalid_immediate_constraint(void) {
  __asm__ volatile("" : : "I"(16)); // expected-error {{value '16' out of range for constraint 'I'}}
}

enum bitfield_enum { BITFIELD_ZERO, BITFIELD_ONE };

struct invalid_bitfield_base_types {
  signed char signed_char_field : 3; // expected-error {{bit-field type 'signed char' is not supported on this target}}
  unsigned char unsigned_char_field : 5; // expected-error {{bit-field type 'unsigned char' is not supported on this target}}
  signed short signed_short_field : 7; // expected-error {{bit-field type 'short' is not supported on this target}}
  unsigned short unsigned_short_field : 9; // expected-error {{bit-field type 'unsigned short' is not supported on this target}}
  signed long signed_long_field : 11; // expected-error {{bit-field type 'long' is not supported on this target}}
  unsigned long unsigned_long_field : 13; // expected-error {{bit-field type 'unsigned long' is not supported on this target}}
  enum bitfield_enum enum_field : 1; // expected-error {{bit-field type 'enum bitfield_enum' is not supported on this target}}
};

struct valid_bitfield_base_types {
  int plain_int_field : 3;
  signed int signed_int_field : 5;
  unsigned int unsigned_int_field : 8;
  _Bool bool_field : 1;
};
