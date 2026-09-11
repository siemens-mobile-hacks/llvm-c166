# User Guide for the C166 Target

## Status and Scope

The `c166-none-elf` target provides initial support for the Siemens/Infineon
C166 architecture and its C ABI. Clang accepts the Large, Medium, and Small
Memory Models:

```console
clang --target=c166-none-elf -mcpu=c166 -mcmodel=large -ffreestanding -c input.c
ld.lld input.o -o output.elf
llvm-objdump -d output.elf
```

Use `-mcmodel=medium` for the Medium model. Its data model is the same paged
far data model as Large, but ordinary functions and function pointers are
near. An explicit `c166_huge` function keeps the inter-segment call class.
Use `-mcmodel=small` for 16-bit direct/DPP ordinary data pointers with the
Large far code model.

LLVM emits its own little-endian ELF32 C166 format. Foreign object formats are
not accepted as compatible inputs. C ABI compatibility refers to the machine
state at a C function boundary, independently of the object-file container.

## CPU Instruction Profile

The CPU names `c166` (the default) and `generic` select the same instruction
set. This profile includes `ATOMIC`, `EXTR`, `EXTP`, `EXTPR`, `EXTS`, and
`EXTSR`, as provided by later C166-family devices such as C167. There is no
separate first-generation SAB 8XC166(W) profile or per-device instruction
filtering. Those first-generation devices lack these instructions and have
a two-bit code segment number rather than the eight-bit segment field used
by this target. Do not interpret `-mcpu=c166` as selecting those devices.

These distinctions are described in the *C166 Family Instruction Set
Manual*, version 2.0, sections 1 and 3.2–3.4. The memory model selects the C
ABI, not the CPU instruction set. In particular, selecting Small does not
disable extension instructions: explicitly qualified far or huge accesses
may still require them. Peripheral registers and their side effects remain
device-specific; accepting an instruction is not a check of a device's
register map.

## Assembly Sources

Clang preprocesses `.S` inputs and assembles them with the integrated assembler.
`__C166_MEMORY_MODEL__` is 1 for Large, 2 for Medium, and 3 for Small.
Use the model's call/return class when hand-written assembly calls C functions.

Direct memory operands accept `sof(symbol + addend)` for a full 16-bit segment
offset, for example with `EXTS`:

```asm
exts #seg(object), #2
mov r4, sof(object)
mov sof(object+2), r4
```

The segment and offset expressions emit `R_C166_SEG8` and `R_C166_SOF16`.
The assembler does not track extension-instruction lifetime: the caller must
ensure each access uses the intended segment.

SFR operands without architectural names can be written as `sfr(address)`.
The short address must be an absolute constant in `0..239` defined before use;
addresses `240..255` belong to GPRs and use their register names. The operand
is available in the same instruction forms as named SFRs:

```asm
.equ control, 0xbd
extr #1
mov sfr(control), #1
push sfr(control)
```

The instruction determines whether an SFR operand uses a short-register field
or a long memory address. For example, `mov r4, sfr(control)` encodes a long
source address (`0xfe00 + 2 * control`), which is not redirected by `EXTR`.
This syntax does not imply that a particular device implements the register.

## Special-Function Registers in C

Clang defines `__near`, `__xnear`, `__far`, `__huge`, `__shuge`, `__sfr`, and
`__esfr` as named aliases for the corresponding C166 address-space attributes.
Device headers can use the register qualifiers with physical byte addresses:

```c
typedef unsigned short u16;

typedef union {
  u16 value;
  struct {
    unsigned reserved0 : 11;
    unsigned ien : 1;
    unsigned reserved1 : 4;
  };
} psw_type;

#define PSW (*(volatile __sfr psw_type *)0xff10U)
```

Pointers into either address space are 16 bits wide. A volatile one-bit field
assignment to a bit-addressable SFR is lowered to the corresponding `BSET`,
`BCLR`, or `BMOV` instruction. ESFR bit updates include the required `EXTR`
prefix. Word accesses use direct physical addresses. SFR addresses must be
even and lie in `0xfe00..0xffde`; ESFR addresses must be even and lie in
`0xf000..0xf1de`. Only their upper halves (`0xff00..0xffde` and
`0xf100..0xf1de`) support architectural bit instructions; bit-field accesses
outside those ranges retain ordinary volatile read-modify-write semantics.

An individual bit can instead be declared without defining a union:

```c
extern unsigned int IEN __attribute__((c166_sfrbit(0xff10, 11)));
extern unsigned int T7IR __attribute__((c166_esfrbit(0xf17a, 7)));
```

These attributes are valid only on external `unsigned int` declarations. The
first argument is an even physical address in the bit-addressable upper half
of the corresponding register area, and the second is a bit number from 0 to
15. A read produces zero or one. Assigning zero clears the bit and assigning
any nonzero value sets it. Accesses are implicitly volatile, the declarations
allocate no storage, and their addresses cannot be taken.

## Large, Medium, and Small C ABI

The data model has 8-bit `char`, 16-bit `short` and `int`, 32-bit `long`, and
64-bit `double` and `long double`. Default data pointers are 32-bit in Large
and Medium and 16-bit in Small; default function pointers are 32-bit in Large
and Small, and 16-bit in Medium. `long long` is a diagnosed 32-bit alias of
`long`, not a separate 64-bit integer type. `size_t` and the default
`ptrdiff_t` are 16-bit. `intptr_t` follows the default data-pointer width:
32-bit in Large and Medium, and 16-bit in Small. Scalars wider than a byte have
two-byte alignment. External C symbols have the C166 leading underscore.

`R0` is the downward-growing user-stack pointer. It is distinct from the
hardware system stack used by `CALLS` and `RETS`. The first four argument
words occupy `R12` through `R15`. A 32-bit scalar uses two consecutive words,
low word first, without even-register alignment. Once an argument cannot use
the remaining argument registers, that argument and all following arguments
are passed on the user stack. The caller removes outgoing stack arguments.
`R6` through `R9` are callee-saved.

An `int` result is returned in `R4`; a `long`, default pointer, or `float`
result is returned low word first in `R4:R5`. Aggregate and `double` results
use a caller-reserved user-stack block, whose near address is returned in
`R4`. Aggregate, `float`, and `double` arguments are copied by value to the
user stack and trigger the normal stack stop rule. Every unnamed variadic
argument is stack-only.

Default data pointers in Large are paged far pointers. Their low word is a
14-bit page offset and their high word is the page number. Pointer arithmetic
changes only the offset word; a far object must not cross a 16-KiB page.
Function pointers instead contain a code segment and offset. Indirect far
calls use the runtime entry point `__icall`, with the function pointer in
`R4:R5` and ordinary C arguments in their normal locations.

Code-bank function types are written as
`__attribute__((c166_bank(N)))` in GNU C or `[[clang::c166_bank(N)]]` in C++.
The bank number is part of the function type. Calls from bank zero or from a
different bank use the platform runtime entry `__banksw`; same-bank calls use
the ordinary direct or `__icall` path. Banked functions always reserve a
hidden two-byte bank word at `[R0]`, so their first public stack argument is at
`[R0+2]` regardless of the dynamic route.

The underlying data-address class attributes are `c166_near`, `c166_xnear`,
`c166_far`, `c166_huge`, and `c166_shuge`. For example,
`unsigned char __near *` is a near data pointer.
`c166_near` and `c166_xnear` pointers are 16-bit values addressed through
DPP2 and DPP1 respectively, and therefore use one ABI word. `c166_far` is the
default Large data-pointer representation described above. `c166_huge` and
`c166_shuge` pointers are 32-bit linear offset:segment values passed and
returned in two ABI words. Their memory accesses use `EXTS`.

`c166_huge` arithmetic carries through the complete linear address and permits
an object access to cross a 64-KiB segment boundary. `c166_shuge` addition
deliberately changes only its low 16-bit offset and wraps within one segment;
objects using that arithmetic must remain within their segment. Both
attributes use a 32-bit pointer-difference type and compare both stored words.
Casts between
these qualifiers and `long` preserve the raw linear value. Casts involving
near pointers use the corresponding DPP value, while casts involving `c166_far`
convert between page:offset and linear form.

In Small, an unqualified or explicit-`c166_near` object uses the complete
16-bit direct address: its upper two bits select DPP0 through DPP3 and its
lower 14 bits select the page offset. The initial ELF contract implements the
default linear map I and requires these objects to fit below 64 KiB.
`c166_xnear` is rejected in Small. Explicit `c166_far`,
`c166_huge`, and `c166_shuge` retain their two-word representations and their
EXTP/EXTS access rules.

On a function type, `c166_near` selects the 16-bit same-segment
`CALLA`/`CALLI`/`RET` class and `c166_huge` selects the 32-bit inter-segment
`CALLS`/`__icall`/`RETS` class. In Medium model an unqualified function is
implicitly near; `c166_huge` explicitly overrides that default. The function
address class is part of the type and is retained by typedefs and function
pointers.

## Interrupt Handlers

Declare an interrupt handler with `__attribute__((interrupt(number)))`. The
function must have return type `void` and prototype `(void)`, and ordinary C
code cannot call it directly. Numbers 0 through 127 identify a vector; `-1`
declares an unnumbered handler. The backend emits the required interrupt entry
and `RETI` exit sequences but does not provide startup code or an
interrupt-vector table.

`__attribute__((c166_register_bank("name")))` may be added to an interrupt
handler to select a named 16-register memory-mapped bank. Handlers using the
same name share the bank. The attribute is rejected on non-interrupt
functions.

Bank storage is emitted in `.c166.regbank`, not ordinary near BSS. Place this
section in the device's internal register RAM with a linker script, reserving
it from the system stack and other contexts. LLD checks word alignment and the
architectural address window `[0xF200, 0xFE00)`; the device may implement a
smaller RAM range. For example, on a device with RAM available at `0xF600`:

```ld
.c166.regbank 0xF600 (NOLOAD) : { *(.c166.regbank) }
```

The entry sequence initializes the new bank's R0 from the interrupted context.
Handlers that can interrupt each other must use distinct banks.

## Assembly Bit Operands

Bit instructions use a short word offset followed by a bit number from 0
through 15. Use spaces around the dot for numeric offsets, for example
`bset 0 . 7`; without spaces, `0.7` is a real-number token. Named operands
use `r4.7`, `psw.11`, or `mdc.0`. Absolute constants may also name offsets
and bit numbers. Use spaces around the dot when referring to forward-defined
components, for example `bset WORD . BIT`; each component is range-checked
independently. A packed operand `(word_offset << 4) | bit_number`
or a BFLD word offset may also use a forward-defined absolute constant. These
are assembler constants, not link-time addresses; unresolved or section-relative
values are rejected. An explicitly
defined dotted symbol is used as a whole symbol, before interpreting the dot
as an offset/bit separator.

Offsets `0x00..0x7f` select RAM words at `0xfd00 + 2 * offset`, not the lower
SFR half. Offsets `0x80..0xef` select the upper SFR half (or ESFR under the
corresponding extended-addressing mode); `0xf0..0xff` select GPRs. Lower SFR
names such as DPP0, CSP, MDL, and SP are therefore invalid bit operands.
They remain valid operands of ordinary word and byte instructions.

## ELF Linking

LLVM uses RELA records for its C166 ELF objects. The object ABI defines 16
value-bearing relocations for absolute, segmented, paged, DPP-relative,
PC-relative, and same-segment code references, plus `R_C166_NONE` as a marker.
LLD resolves local, global, weak, common, and section symbols using ordinary
static ELF rules. An undefined weak symbol has value zero and may still carry
an addend; an unresolved strong symbol is an error. Field-width, alignment,
page, and code-segment checks are applied after symbol resolution.

`R_C166_SEG24` represents the contiguous 24-bit operand of `CALLS` and `JMPS`:
one segment byte followed by a little-endian 16-bit offset. MC emits this single
relocation when both instruction fields refer to the same expression. The
independent `R_C166_SEG8` and `R_C166_SOF16` relocations remain available when
the fields use different expressions or are stored separately.

`R_C166_PAGED32` represents a complete 32-bit far-data pointer in a
static initializer. LLD converts a 24-bit linear symbol address to the stored
`offset:page` form: the low word receives bits 0 through 13 and the high word
receives bits 14 through 23. This relocation is distinct from `R_C166_32`,
which keeps the linear value used by integers, huge-data pointers, and far
function pointers. Clang emits `R_C166_PAGED32` for default Large/Medium and
explicit `c166_far` pointer initializers, including pointers nested in
aggregates and arrays.

`R_C166_PC8_RELAX` marks a fixed-size slot beginning at a relative branch
opcode. MC reserves four bytes for unconditional JMPR, six for invertible
conditional JMPR, eight for JMPR NET or JB/JNB, and ten for JBC/JNBS when the
displacement is unresolved or outside the signed 8-bit word range. LLD keeps
the short form
and NOP padding when the target is reachable within the current code segment;
otherwise it emits JMPS. Invertible conditions skip JMPS using the opposite
condition. NET has no inverse: `JMPR cc_net, 1; JMPR cc_uc, 2; JMPS seg, off`
preserves its condition and flags. JBC/JNBS similarly retain the original bit
operation followed by `JMPR cc_uc, 2; JMPS seg, off`: the bit branch skips
JMPR when taken, preserving the bit writeback and flags on both paths. A
replacement must fit within its code segment.

Local short branches retain `R_C166_PC8` or `R_C166_BIT_PC8` so LLD can check
the final segment without enlarging the instruction. These relocations have
no replacement space and diagnose invalid segment, range, or alignment. Their
displacements wrap with the 16-bit IP; they do not change CSP.

`R_C166_NONE` carries no value or symbol semantics. The three published
relocation numbers 253 through 255 are reserved and are not emitted or accepted
as LLVM relocation names.

Large objects carry ELF `e_flags` value `0x121` (8x166 core, far data, huge
code); Medium objects carry `0x221` (8x166 core, far data, near code); Small
objects carry `0x111` (8x166 core, direct data, huge code). LLD rejects inputs
with different C166 model flags. In a Medium link, ordinary
near code must fit completely in the first 64-KiB code segment. This is
checked using input-section identity and processor-specific symbol metadata,
so a custom source section or linker-script output-section rename cannot hide
an invalid placement. Explicitly huge functions may be placed in another code
segment. In a Small link, ordinary direct data must fit below 64 KiB; explicit
far objects must stay within one 16-KiB page, shuge objects within one 64-KiB
segment, and all explicit far/huge/shuge objects within the 16-MiB address
space. Processor-specific object-symbol metadata preserves these checks when
source section attributes or linker scripts rename output sections.

Optimized dense switches use tables of 16-bit code offsets and dispatch with
`JMPI`. Large and Medium address the table as paged read-only data; Small uses
its direct read-only-data range. Large and Small table entries use the segment
offset of each destination, while Medium entries use the near code offset.
LLD rejects a paged table that crosses a 16-KiB data page and any huge function
whose body crosses a 64-KiB code-segment boundary. These checks ensure that the
table load and the current-`CSP` `JMPI` transfer remain valid after final
layout.

The textual assembly directives `.c166_model`, `.c166_function`, and
`.c166_data` preserve the model and function/data address classes through a
Clang assembly round trip. ELF objects encode custom-section function and
object classes in processor-specific bits of `st_other`. This is LLVM's
standard ELF32 encoding for its C166 format.

For `.s` and preprocessed `.S` input, the Clang driver forwards the selected
`-mcmodel` to the integrated assembler. Assembly sources therefore inherit
Large, Medium, or Small ELF identity without spelling `.c166_model`
themselves. The directive remains useful for standalone `llvm-mc` input and
textual assembly that must carry its model explicitly. A conflicting
command-line model and directive is diagnosed.

## Required Runtime Entries

The backend can leave the following ELF symbols undefined. A freestanding
environment using the corresponding operations must provide them:

| ELF symbol | Operation | Input and result convention |
| --- | --- | --- |
| `___mulsi3` | signed/unsigned 32-bit multiply | two `long` values in `R12:R13` and `R14:R15`; result in `R4:R5` |
| `___divsi3`, `___modsi3` | signed 32-bit divide/remainder | same binary `long` convention |
| `___udivsi3`, `___umodsi3` | unsigned 32-bit divide/remainder | same binary `unsigned long` convention |
| `___ashlsi3`, `___ashrsi3`, `___lshrsi3` | 32-bit variable shift | value in `R12:R13`, 16-bit count in `R14`, result in `R4:R5` |
| `___clzsi2` | 32-bit count-leading-zero | value in `R12:R13`; 16-bit result in `R4` |
| `___addsf3`, `___subsf3`, `___mulsf3`, `___divsf3` | IEEE-754 binary32 arithmetic | operand bit patterns in `R12:R13` and `R14:R15`; result bits in `R4:R5` |
| `___adddf3`, `___subdf3`, `___muldf3`, `___divdf3` | IEEE-754 binary64 arithmetic | ordinary stack arguments and caller-reserved result block |
| `___fixsfsi`, `___fixunssfsi`, `___floatsisf`, `___floatunsisf` | binary32/integer conversions | integer or binary32 bit-pattern boundary appropriate to the operation |
| `___fixdfsi`, `___fixunsdfsi`, `___floatsidf`, `___floatunsidf` | binary64/integer conversions | ordinary C166 binary64 stack/result convention |
| `___extendsfdf2`, `___truncdfsf2` | binary32/binary64 width conversion | ordinary C166 floating boundary |
| `___eqsf2`, `___nesf2`, `___ltsf2`, `___lesf2`, `___gtsf2`, `___gesf2`, `___unordsf2` | binary32 comparisons | binary32 bit patterns in the ordinary two-word helper slots; `int` result |
| `___eqdf2`, `___nedf2`, `___ltdf2`, `___ledf2`, `___gtdf2`, `___gedf2`, `___unorddf2` | binary64 comparisons | ordinary stack arguments; `int` result |
| `_memcpy`, `_memmove`, `_memset` | memory operations not expanded inline | ordinary C function boundary |
| `__icall` | indirect far call thunk | target in `R4:R5`; preserves the public callee ABI |
| `__banksw` | platform-specific code-bank switch and indirect call | `RH3` current bank, `RL3` target bank, target in `R4:R5`, identical bank word at `[R0]` |

The extra leading underscore in the ELF names is the normal C166 symbol
decoration. For example, a C runtime source can define `__mulsi3` with a
`long (long, long)` signature and LLVM emits the ELF symbol `___mulsi3`.
The binary32 helpers use integer bit-pattern arguments internally; defining
them as ordinary C functions with `float` parameters would incorrectly apply
the public stack-passed float convention. The C166 compiler-rt port provides
these entries, including the different calling conventions needed by binary32
helper internals and ordinary binary64 functions. It installs
separate `libclang_rt.builtins.a` (Large),
`libclang_rt.builtins-medium.a` (Medium), and
`libclang_rt.builtins-small.a` (Small) archives; Clang selects the matching
archive from `-mcmodel`. Mixing their objects is rejected by the ELF model
flags. Compiler-generated Medium helper calls and assembly adapter returns use
the near `CALLA`/`RET` class; Small helpers use the Large far code class.

`__icall` is an assembly interface, not a C function.  In Large it is reached
with `CALLS`, so the caller's system-stack frame already contains both IP and
CSP:

```asm
__icall:
  atomic #3
  push r5
  push r4
  rets
```

Small uses the same far `__icall` contract as Large. In Medium `__icall` is a
near entry reached with `CALLA`, which saves only the return IP. An explicitly
huge target returns with `RETS` and needs an IP:CSP frame. The Medium runtime
therefore converts the caller's frame
before transferring control; `R1` and `R2` are caller-clobbered ABI registers:

```asm
__icall:
  mov r2, csp
  atomic #3
  pop r1
  push r2
  push r1
  atomic #3
  push r5
  push r4
  rets
```

`__banksw` is also an assembly interface rather than a C function, but unlike
`__icall` it cannot have a target-independent implementation. Its contract is:

- the entry must reside in non-banked code;
- `RL3` selects the target bank and `R4:R5` contains the target's
  inter-segment address;
- `RH3` is the caller's bank, or zero when no restoration is required;
- the caller stores the complete `R3` word at `[R0]`; `__banksw` must not move
  `R0`, because the bank word, C stack arguments, and automatics have
  compiler-fixed offsets;
- after activating `RL3`, the entry invokes the target with the ordinary C
  arguments still in `R12`-`R15` and on the user stack;
- after a nested call returns, a non-zero `RH3` must be reactivated before
  returning to the caller;
- `R6`-`R9`, `R12`-`R15`, the DPP context, and the balanced user/system stacks
  are preserved. `R1`-`R3`, `R10`, and `R11` are scratch. `R4:R5` initially
  carry the target and subsequently carry the callee result when applicable.

The platform bank-selection register and its mapping policy are device- or
board-specific. Consequently compiler-rt intentionally does not define
`__banksw`; a BSP or freestanding platform runtime must supply it. A link that
contains a cross-bank edge and no platform implementation fails with an
undefined `__banksw` symbol instead of silently selecting a fictitious
mechanism.

The ABI passes `RH3 = 0` for a call made by non-banked code.
Accordingly a banked-function -> non-banked-function -> other-bank chain does
not restore the original bank.

## Current Limitations

- Tiny and Huge memory models are not implemented. Large, Medium, and Small
  compile, assembly, ELF link placement, runtime multilib, and DWARF frame
  classes are implemented. Small currently uses the default linear DPP map I.
- The public C data model has no 64-bit integer type. Private
  compiler-rt `_BitInt(64)` containers do not create a public 64-bit ABI.
- Variable-length arrays and dynamic allocation on the user stack are not
  supported. C11 atomics are always non-lock-free and use the C166 runtime
  critical-section contract; lock-free queries conservatively return false.
- C++ ABI support, exceptions, RTTI, TLS, PIC/PIE, shared objects, and dynamic
  linking are outside the initial target scope.
- Debug-only DWARF call-frame information is supported for ordinary
  Large/Small far, Medium/explicit-near, interrupt, and register-bank frames.
  Runtime `.eh_frame` unwinding is intentionally diagnosed because C166 far
  returns use a hardware system stack separate from the `R0` user stack.
- Cross-bank calls require a platform-provided `__banksw`; compiler-rt cannot
  infer the hardware bank-selection mechanism.
- The included compiler-rt port covers operations emitted by the current
  supported subset. A C library, production startup code, device linker
  script, and device SDK are not included.
