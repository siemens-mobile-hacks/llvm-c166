// RUN: %clang_cc1 -triple c166-none-elf -fsyntax-only -verify %s

__attribute__((interrupt(-1), c166_register_bank("FAST_BANK")))
void bank_after_interrupt(void) {}

__attribute__((c166_register_bank("FAST_BANK"), interrupt(1)))
void bank_before_interrupt(void) {}

__attribute__((c166_register_bank("FAST_BANK"))) // expected-error {{'c166_register_bank' attribute requires a C166 interrupt handler}}
void ordinary_function(void) {}

__attribute__((interrupt(2), c166_register_bank(""))) // expected-error {{C166 register bank name must be a non-empty ASCII identifier}}
void empty_name(void) {}

__attribute__((interrupt(3), c166_register_bank("bad-name"))) // expected-error {{C166 register bank name must be a non-empty ASCII identifier}}
void invalid_name(void) {}

__attribute__((interrupt(4), c166_register_bank(4))) // expected-error {{expected string literal as argument of 'c166_register_bank' attribute}}
void non_string_name(void) {}

__attribute__((interrupt(5), c166_register_bank("BANK_A"))) // expected-note {{conflicting attribute is here}}
void conflicting_bank(void);
__attribute__((interrupt(5), c166_register_bank("BANK_B"))) // expected-error {{'c166_register_bank' and 'c166_register_bank' attributes are not compatible}}
void conflicting_bank(void) {}
