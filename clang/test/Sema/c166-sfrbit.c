// RUN: %clang_cc1 -triple c166-none-elf -fsyntax-only -verify %s

extern unsigned int ien __attribute__((c166_sfrbit(0xff10, 11)));
extern unsigned int ien __attribute__((c166_sfrbit(0xff10, 11)));
extern unsigned int ien;
extern unsigned int t7ir __attribute__((c166_esfrbit(0xf17a, 7)));

unsigned int read_ien(void) { return ien; }
void write_ien(unsigned int value) { ien = value; }

void invalid_address_of(void) {
  (void)&ien; // expected-error {{cannot take the address of a C166 special-function register bit}}
}

extern int wrong_type // expected-error@+1 {{'c166_sfrbit' attribute requires an external 'unsigned int' declaration without an initializer}}
    __attribute__((c166_sfrbit(0xff10, 11)));
static unsigned int internal // expected-error@+1 {{'c166_sfrbit' attribute only applies to external global variables}}
    __attribute__((c166_sfrbit(0xff10, 11)));
unsigned int definition // expected-error@+1 {{'c166_sfrbit' attribute requires an external 'unsigned int' declaration without an initializer}}
    __attribute__((c166_sfrbit(0xff10, 11)));
extern unsigned int initialized // expected-error@+1 {{'c166_sfrbit' attribute requires an external 'unsigned int' declaration without an initializer}}
    __attribute__((c166_sfrbit(0xff10, 11))) = 0;

extern unsigned int sfr_low // expected-error@+1 {{'c166_sfrbit' attribute address must be an even value in the bit-addressable range}}
    __attribute__((c166_sfrbit(0xfefe, 0)));
extern unsigned int sfr_odd // expected-error@+1 {{'c166_sfrbit' attribute address must be an even value in the bit-addressable range}}
    __attribute__((c166_sfrbit(0xff11, 0)));
extern unsigned int sfr_high // expected-error@+1 {{'c166_sfrbit' attribute address must be an even value in the bit-addressable range}}
    __attribute__((c166_sfrbit(0xffe0, 0)));
extern unsigned int esfr_low // expected-error@+1 {{'c166_esfrbit' attribute address must be an even value in the bit-addressable range}}
    __attribute__((c166_esfrbit(0xf0fe, 0)));
extern unsigned int esfr_high // expected-error@+1 {{'c166_esfrbit' attribute address must be an even value in the bit-addressable range}}
    __attribute__((c166_esfrbit(0xf1e0, 0)));
extern unsigned int bad_bit // expected-error@+1 {{'c166_sfrbit' attribute bit number must be in the range 0 to 15}}
    __attribute__((c166_sfrbit(0xff10, 16)));

extern unsigned int conflict __attribute__((c166_sfrbit(0xff10, 0))); // expected-note {{conflicting attribute is here}}
extern unsigned int conflict
    __attribute__((c166_esfrbit(0xf110, 0))); // expected-error {{attributes are not compatible}}

void local(void) {
  extern unsigned int local_extern // expected-error@+1 {{'c166_sfrbit' attribute only applies to external global variables}}
      __attribute__((c166_sfrbit(0xff10, 0)));
}
