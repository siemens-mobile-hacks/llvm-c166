// RUN: %clang_cc1 -triple c166-none-elf -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -fsyntax-only -verify %s

typedef unsigned int u16;
typedef unsigned char u8;

typedef u8 __attribute__((c166_far)) far_u8;
typedef u8 __attribute__((c166_near)) near_u8;
typedef u8 __attribute__((c166_xnear)) xnear_u8;
typedef u8 __attribute__((c166_huge)) huge_u8;
typedef u8 __attribute__((c166_shuge)) shuge_u8;

_Static_assert(sizeof(far_u8 *) == 4, "far data pointer width");
_Static_assert(sizeof(near_u8 *) == 2, "near data pointer width");
_Static_assert(sizeof(xnear_u8 *) == 2, "xnear data pointer width");
_Static_assert(sizeof(huge_u8 *) == 4, "huge data pointer width");
_Static_assert(sizeof(shuge_u8 *) == 4, "shuge data pointer width");

typedef u16 __attribute__((c166_near)) near_fn(u16);
typedef u16 __attribute__((c166_huge)) huge_fn(u16);
typedef u16 plain_fn(u16);

u16 __attribute__((c166_near)) near_decl(u16);
u16 __attribute__((c166_huge)) huge_decl(u16);

u16 __attribute__((c166_near)) redeclared_address(u16); // expected-note {{previous declaration is here}}
u16 __attribute__((c166_huge)) redeclared_address(u16); // expected-error {{conflicting types for 'redeclared_address'}}

_Static_assert(sizeof(near_fn *) == 2, "near function pointer width");
_Static_assert(sizeof(huge_fn *) == 4, "huge function pointer width");

void incompatible(near_fn *near_pointer, huge_fn *huge_pointer) {
  near_pointer = huge_pointer; // expected-error {{changes address space of pointer}}
  huge_pointer = near_pointer; // expected-error {{changes address space of pointer}}
}

typedef u16 __attribute__((c166_near, c166_huge)) conflicting_fn(u16); // expected-error {{'c166_huge' and 'c166_near' attributes are not compatible}} expected-note {{conflicting attribute is here}}
typedef u16 __attribute__((c166_near, c166_near)) duplicate_near_fn(u16); // expected-warning {{attribute 'c166_near' is already applied}}

u16 __attribute__((c166_near, c166_bank(1))) near_banked(u16); // expected-error {{'c166_bank' cannot be combined with a near function type}}
u16 __attribute__((c166_huge, c166_bank(1))) huge_banked(u16); // expected-error {{'c166_bank' and address_space attributes are not compatible}}

#if !__has_attribute(c166_near)
#error c166_near must be discoverable
#endif
#if !__has_attribute(c166_huge)
#error c166_huge must be discoverable
#endif
#if !__has_attribute(c166_far)
#error c166_far must be discoverable
#endif
#if !__has_attribute(c166_xnear)
#error c166_xnear must be discoverable
#endif
#if !__has_attribute(c166_shuge)
#error c166_shuge must be discoverable
#endif
