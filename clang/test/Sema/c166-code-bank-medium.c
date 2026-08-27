// REQUIRES: c166-registered-target
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -fsyntax-only -verify %s

typedef unsigned int u16;

// C166-ABI: medium.special.code_bank_forbidden
typedef u16 __attribute__((c166_bank(1))) medium_banked_function(u16);
// expected-error@-1 {{'c166_bank' cannot be combined with a near function type}}
