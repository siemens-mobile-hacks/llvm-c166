// REQUIRES: c166-registered-target
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=small -fsyntax-only -verify %s

typedef int __attribute__((c166_xnear)) xnear_int; // expected-error {{'c166_xnear' is only available in the Medium and Large memory models}}
// expected-error@+1 {{'c166_xnear' is only available in the Medium and Large memory models}}
int __attribute__((c166_xnear)) xnear_object;
// expected-error@+1 {{'c166_xnear' is only available in the Medium and Large memory models}}
void use_xnear(int __attribute__((c166_xnear)) *p);

typedef int __attribute__((c166_near)) near_int;
typedef int __attribute__((c166_far)) far_int;
typedef int __attribute__((c166_huge)) huge_int;
typedef int __attribute__((c166_shuge)) shuge_int;

_Static_assert(sizeof(near_int *) == 2, "Small _near pointer width");
_Static_assert(sizeof(far_int *) == 4, "Small _far pointer width");
_Static_assert(sizeof(huge_int *) == 4, "Small _huge pointer width");
_Static_assert(sizeof(shuge_int *) == 4, "Small _shuge pointer width");
