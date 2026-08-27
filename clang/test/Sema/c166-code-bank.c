// RUN: %clang_cc1 -triple c166-none-elf -fsyntax-only -verify %s

typedef unsigned int u16;

typedef u16 __attribute__((c166_bank(1))) bank1_fn(u16);
typedef u16 __attribute__((c166_bank(2))) bank2_fn(u16);
typedef u16 plain_fn(u16);

u16 __attribute__((c166_bank(1))) bank1_decl(u16);
u16 __attribute__((c166_bank(1))) bank1_decl(u16 value) { return value; }

void pointer_types(bank1_fn *one, bank2_fn *two, plain_fn *plain) {
  one = two;   // expected-error {{changes address space of pointer}}
  one = plain; // expected-error {{changes address space of pointer}}
}

u16 __attribute__((c166_bank(0))) bank_zero(u16); // expected-error {{C166 code bank number must be in the range 1 to 255}}
u16 __attribute__((c166_bank(256))) bank_too_large(u16); // expected-error {{C166 code bank number must be in the range 1 to 255}}
u16 __attribute__((c166_bank(-1))) bank_negative(u16); // expected-error {{C166 code bank number must be in the range 1 to 255}}

u16 __attribute__((c166_near, c166_bank(1))) near_banked(u16); // expected-error {{'c166_bank' cannot be combined with a near function type}}

u16 __attribute__((c166_bank(1))) redeclared_bank(u16); // expected-note {{previous declaration is here}}
u16 __attribute__((c166_bank(2))) redeclared_bank(u16); // expected-error {{conflicting types for 'redeclared_bank'}}

#if !__has_attribute(c166_bank)
#error c166_bank must be discoverable
#endif
