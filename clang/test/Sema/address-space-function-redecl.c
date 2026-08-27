// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fsyntax-only -verify %s

typedef void __attribute__((address_space(1))) as1_function(void);
typedef void __attribute__((address_space(2))) as2_function(void);

as1_function same_address_space;
as1_function same_address_space;

as1_function different_address_space; // expected-note {{previous declaration is here}}
as2_function different_address_space; // expected-error {{conflicting types for 'different_address_space'}}

void default_address_space(void); // expected-note {{previous declaration is here}}
as1_function default_address_space; // expected-error {{conflicting types for 'default_address_space'}}
