// REQUIRES: c166-registered-target
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=small -E -dM %s | FileCheck %s --check-prefix=MACRO
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=small -O1 -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=small -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-readobj --file-headers --sections --relocations %t.o | FileCheck %s --check-prefix=ELF
// RUN: llvm-objdump -dr %t.o | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -S %s -o %t.s
// RUN: FileCheck %s --check-prefix=TEXT < %t.s
// RUN: llvm-mc -filetype=obj -triple=c166-none-elf %t.s -o %t.roundtrip.o
// RUN: llvm-readobj --file-headers --sections --relocations %t.roundtrip.o | FileCheck %s --check-prefix=ELF

#include <stdarg.h>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u16;
typedef unsigned long u32;
typedef u16 __attribute__((c166_near)) near_u16;
typedef u16 __attribute__((c166_far)) far_u16;
typedef u16 __attribute__((c166_huge)) huge_u16;
typedef u16 __attribute__((c166_shuge)) shuge_u16;
typedef u16 __attribute__((c166_near)) near_function(u16);
typedef u16 default_function(u16);
typedef u16 __attribute__((c166_bank(1))) banked_function(u16);

_Static_assert(sizeof(void *) == 2, "Small default data pointer is direct16");
_Static_assert(sizeof(near_u16 *) == 2, "explicit near is direct16 in Small");
_Static_assert(sizeof(far_u16 *) == 4, "explicit far pointer remains paged");
_Static_assert(sizeof(huge_u16 *) == 4, "explicit huge pointer remains linear");
_Static_assert(sizeof(shuge_u16 *) == 4,
               "explicit shuge pointer remains segmented");
_Static_assert(sizeof(default_function *) == 4,
               "Small default function pointer is huge");
_Static_assert(sizeof(near_function *) == 2,
               "explicit near function pointer is near");
_Static_assert(sizeof(banked_function *) == 4,
               "banked function pointer is inter-segment");

volatile u16 default_word;
volatile u8 default_byte;
volatile s8 default_signed_byte;
volatile u32 default_long;
volatile near_u16 near_word;
volatile far_u16 far_word;
volatile huge_u16 huge_word;
volatile shuge_u16 shuge_word;
u16 default_initialized = 0x1111;
const u16 default_constant = 0x2222;
far_u16 far_initialized = 0x3333;
const far_u16 far_constant = 0x4444;
huge_u16 huge_initialized = 0x5555;
const huge_u16 huge_constant = 0x6666;
shuge_u16 shuge_initialized = 0x7777;
const shuge_u16 shuge_constant = 0x8888;

u16 read_default_word(void) { return default_word; }
void write_default_word(u16 value) { default_word = value; }
u8 read_default_byte(void) { return default_byte; }
s8 read_default_signed_byte(void) { return default_signed_byte; }
void write_default_byte(u8 value) { default_byte = value; }
u32 read_default_long(void) { return default_long; }
void write_default_long(u32 value) { default_long = value; }
u32 shift_runtime(u32 value, u16 amount) { return value << amount; }
u16 *address_default_word(void) { return (u16 *)&default_word; }
u16 read_near_word(void) { return near_word; }
u16 read_far_word(void) { return far_word; }
u16 read_huge_word(void) { return huge_word; }
u16 read_shuge_word(void) { return shuge_word; }

extern u16 default_external(u16);
extern u16 __attribute__((c166_near)) near_external(u16);

u16 call_default(u16 value) { return default_external(value); }
u16 call_default_indirect(default_function *function, u16 value) {
  return function(value);
}
u16 call_near(u16 value) { return near_external(value); }

__attribute__((noinline)) u16 take_pointer(u16 tag, ...) {
  va_list args;
  va_start(args, tag);
  u16 *pointer = va_arg(args, u16 *);
  va_end(args);
  return *pointer;
}

u16 call_take_pointer(u16 *pointer) { return take_pointer(1, pointer); }

// C166-ABI: small.elf.identity
// C166-ABI: small.data.default_direct16
// C166-ABI: small.functions.default_huge
// C166-ABI: small.varargs.default_pointer_word

// MACRO: #define __C166_MEMORY_MODEL__ 3
// MACRO: #define __INTPTR_TYPE__ int
// MACRO: #define __SIZEOF_POINTER__ 2

// IR: target datalayout = "{{.*}}-P1-G3-A3-p:32:16-{{.*}}"
// IR: @default_word = {{.*}}addrspace(3) global i16
// IR: @near_word = {{.*}}addrspace(3) global i16
// IR: @far_word = {{.*}}addrspace(2) global i16
// IR: @huge_word = {{.*}}addrspace(5) global i16
// IR: @shuge_word = {{.*}}addrspace(6) global i16
// IR-LABEL: define{{.*}} i16 @call_default(
// IR-SAME: addrspace(1)
// IR: call addrspace(1) i16 @default_external
// IR-LABEL: define{{.*}} i16 @call_default_indirect(ptr addrspace(1)
// IR-SAME: addrspace(1)
// IR: call addrspace(1) i16 %

// TEXT: .c166_model{{[[:space:]]+}}small
// TEXT: .c166_function{{[[:space:]]+}}huge, _call_default
// TEXT: .c166_data{{[[:space:]]+}}near, _default_word
// TEXT: .c166_data{{[[:space:]]+}}near, _near_word
// TEXT: .c166_data{{[[:space:]]+}}far, _far_word
// TEXT: .c166_data{{[[:space:]]+}}huge, _huge_word
// TEXT: .c166_data{{[[:space:]]+}}shuge, _shuge_word

// ELF: Flags [ (0x111)
// ELF-NEXT: EF_C166_CODE_HUGE (0x100)
// ELF-NEXT: EF_C166_CORE_8X166 (0x1)
// ELF-NEXT: EF_C166_DATA_NEAR (0x10)
// ELF-DAG: Name: .c166.small.data
// ELF-DAG: Name: .c166.small.bss
// ELF-DAG: Name: .c166.small.rodata
// ELF-DAG: Name: .c166.small.far.data
// ELF-DAG: Name: .c166.small.far.bss
// ELF-DAG: Name: .c166.small.far.rodata
// ELF-DAG: Name: .c166.small.huge.data
// ELF-DAG: Name: .c166.small.huge.bss
// ELF-DAG: Name: .c166.small.huge.rodata
// ELF-DAG: Name: .c166.small.shuge.data
// ELF-DAG: Name: .c166.small.shuge.bss
// ELF-DAG: Name: .c166.small.shuge.rodata
// ELF-DAG: R_C166_16 _default_word
// ELF-DAG: R_C166_16 _near_word
// ELF-DAG: R_C166_16 _default_byte
// ELF-DAG: R_C166_16 _default_signed_byte
// ELF-DAG: R_C166_16 _default_long
// ELF-DAG: R_C166_PAG10 _far_word
// ELF-DAG: R_C166_POF14 _far_word
// ELF-DAG: R_C166_SEG8 _huge_word
// ELF-DAG: R_C166_SOF16 _huge_word
// ELF-DAG: R_C166_SEG8 _shuge_word
// ELF-DAG: R_C166_SOF16 _shuge_word
// ELF-DAG: R_C166_SEG24 _default_external
// ELF-DAG: R_C166_SEG24 ___ashlsi3
// ELF-DAG: R_C166_SEG24 __icall
// ELF-DAG: R_C166_COF16 _near_external

// ASM-LABEL: <_read_default_word>:
// ASM: R_C166_16{{[[:space:]]+}}_default_word
// ASM-NOT: extp
// ASM: rets
// ASM-LABEL: <_shift_runtime>:
// ASM: calls
// ASM: R_C166_SEG24{{[[:space:]]+}}___ashlsi3
// ASM: rets
// ASM-LABEL: <_read_far_word>:
// ASM: extp
// ASM: R_C166_PAG10{{[[:space:]]+}}_far_word
// ASM: R_C166_POF14{{[[:space:]]+}}_far_word
// ASM: rets
// ASM-LABEL: <_read_huge_word>:
// ASM: exts
// ASM: rets
// ASM-LABEL: <_read_shuge_word>:
// ASM: exts
// ASM: rets
// ASM-LABEL: <_call_default>:
// ASM: jmps
// ASM: R_C166_SEG24{{[[:space:]]+}}_default_external
// ASM-LABEL: <_call_default_indirect>:
// ASM: calls
// ASM: R_C166_SEG24{{[[:space:]]+}}__icall
// ASM: rets
// ASM-LABEL: <_call_near>:
// ASM: calla
// ASM: R_C166_COF16{{[[:space:]]+}}_near_external
// ASM: rets
