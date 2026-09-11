// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=tiny -target-abi tiny -fsyntax-only -verify %s

typedef int __attribute__((c166_near)) near_int;
typedef int __attribute__((c166_far)) far_int; // expected-error {{'c166_far' attribute is not available in the tiny memory model}}
typedef int __attribute__((c166_xnear)) xnear_int; // expected-error {{'c166_xnear' is only available in the Medium, Large, and Huge memory models}}
typedef int __attribute__((c166_huge)) huge_int; // expected-error {{'c166_huge' attribute is not available in the tiny memory model}}
typedef int __attribute__((c166_shuge)) shuge_int; // expected-error {{'c166_shuge' attribute is not available in the tiny memory model}}

int __attribute__((c166_near)) near_function(void);
int __attribute__((c166_huge)) huge_function(void); // expected-error {{'c166_huge' attribute is not available in the tiny memory model}}
int __attribute__((c166_bank(1))) banked_function(void); // expected-error {{'c166_bank' attribute is not available in the tiny memory model}}
