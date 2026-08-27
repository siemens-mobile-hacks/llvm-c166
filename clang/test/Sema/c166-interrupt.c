// RUN: %clang_cc1 -triple c166-none-elf -fsyntax-only -verify %s

__attribute__((interrupt(-1))) void unnumbered(void) {}
__attribute__((interrupt(0))) void first_vector(void) {}
__attribute__((interrupt(127))) void last_vector(void) {}

__attribute__((interrupt(-2))) void negative_out_of_range(void) {} // expected-error {{'interrupt' attribute parameter -2 is out of bounds}}
__attribute__((interrupt(128))) void positive_out_of_range(void) {} // expected-error {{'interrupt' attribute parameter 128 is out of bounds}}

extern int vector_number;
__attribute__((interrupt(vector_number))) void nonconstant(void) {} // expected-error {{'interrupt' attribute requires an integer constant}}

__attribute__((interrupt(1))) int nonvoid(void) { return 0; } // expected-error {{C166 interrupt handler must have return type 'void' and prototype '(void)'}}
__attribute__((interrupt(2))) void with_parameter(int value) {} // expected-error {{C166 interrupt handler must have return type 'void' and prototype '(void)'}}

__attribute__((interrupt(3))) void directly_called(void) {}
void caller(void) {
  directly_called(); // expected-error {{C166 interrupt handler cannot be called directly}}
}
