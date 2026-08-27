// RUN: %clang_cc1 -triple c166-none-elf -std=c++17 -fsyntax-only -verify %s

[[gnu::interrupt(-1), clang::c166_register_bank("FAST_BANK")]]
void namespaced_spelling() {}

// The unprefixed C++ attribute is intentionally not part of the public API.
[[c166_register_bank("FAST_BANK")]] // expected-warning {{unknown attribute 'c166_register_bank' ignored}}
void unqualified_spelling() {}
