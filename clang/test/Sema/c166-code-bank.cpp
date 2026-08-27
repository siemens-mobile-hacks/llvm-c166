// RUN: %clang_cc1 -triple c166-none-elf -std=c++17 -fsyntax-only -verify %s

using u16 = unsigned int;

using bank3_fn = u16 (u16) [[clang::c166_bank(3)]];

u16 bank3(u16 value) [[clang::c166_bank(3)]] { return value; }

bank3_fn *get_bank3() { return &bank3; }

[[c166_bank(3)]] u16 unqualified_namespace(u16); // expected-warning {{unknown attribute 'c166_bank' ignored}}
