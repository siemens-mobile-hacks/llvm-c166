<!-- This document is written in Markdown and uses extra directives provided by
MyST (https://myst-parser.readthedocs.io/en/latest/). -->

<!-- If you want to modify sections/contents permanently, you should modify both
ReleaseNotes.md and ReleaseNotesTemplate.txt. -->

# LLVM {{env.config.release}} Release Notes


::::{only} PreRelease
:::{warning} These are in-progress notes for the upcoming LLVM {{env.config.release}}
             release. Release notes for previous releases can be found on
             [the Download Page](https://releases.llvm.org/download.html).
:::
::::

## Introduction

This document contains the release notes for the LLVM Compiler Infrastructure,
release {{env.config.release}}.  Here we describe the status of LLVM, including
major improvements from the previous release, improvements in various subprojects
of LLVM, and some of the current users of the code.  All LLVM releases may be
downloaded from the [LLVM releases web site](https://llvm.org/releases/).

For more information about LLVM, including information about the latest
release, please check out the [main LLVM web site](https://llvm.org/).  If you
have questions or comments, the [Discourse forums](https://discourse.llvm.org)
is a good place to ask them.

Note that if you are reading this file from a Git checkout or the main
LLVM web page, this document applies to the *next* release, not the current
one.  To see the release notes for a specific release, please see the
[releases page](https://llvm.org/releases/).

## Non-comprehensive list of changes in this release

<!-- For small 1-3 sentence descriptions, just add an entry at the end of
this list. If your description won't fit comfortably in one bullet
point (e.g. maybe you would like to give an example of the
functionality, or simply have a lot to talk about), see the comment below
for adding a new subsection. -->

* ...

<!-- If you would like to document a larger change, then you can add a
subsection about it right here. You can copy the following boilerplate:

### Special New Feature

Makes programs 10x faster by doing Special New Thing.
-->

### Changes to the LLVM IR

* Added `llvm.vector.reduce.fmaximumnum` and `llvm.vector.reduce.fminimumnum`
  intrinsics, the reduction variants of `llvm.maximumnum` and
  `llvm.minimumnum`. 
* Added `nofreeobj` attribute for attributes and returns, which forbids
  freeing the underlying object (as opposed to only frees through that specific
  pointer). Renamed `!nofree` metadata to `!nofreeobj`, as it has the same
  semantics.
* The following VP intrinsics have been removed:
  * `llvm.vp.select.*`
  * `llvm.vp.add.*`
  * `llvm.vp.sub.*`
  * `llvm.vp.mul.*`
  * `llvm.vp.ashr.*`
  * `llvm.vp.lshr.*`
  * `llvm.vp.shl.*`
  * `llvm.vp.or.*`
  * `llvm.vp.and.*`
  * `llvm.vp.xor.*`
  * `llvm.vp.abs.*`
  * `llvm.vp.smax.*`
  * `llvm.vp.smin.*`
  * `llvm.vp.umax.*`
  * `llvm.vp.umin.*`
  * `llvm.vp.copysign.*`
  * `llvm.vp.minnum.*`
  * `llvm.vp.maxnum.*`
  * `llvm.vp.minimum.*`
  * `llvm.vp.maximum.*`
  * `llvm.vp.fadd.*`
  * `llvm.vp.fsub.*`
  * `llvm.vp.fmul.*`
  * `llvm.vp.fdiv.*`
  * `llvm.vp.frem.*`
  * `llvm.vp.fneg.*`
  * `llvm.vp.fabs.*`
  * `llvm.vp.sqrt.*`
  * `llvm.vp.fma.*`
  * `llvm.vp.fmuladd.*`
  * `llvm.vp.trunc.*`
  * `llvm.vp.zext.*`
  * `llvm.vp.sext.*`
  * `llvm.vp.fptrunc.*`
  * `llvm.vp.fpext.*`
  * `llvm.vp.fptoui.*`
  * `llvm.vp.fptosi.*`
  * `llvm.vp.uitofp.*`
  * `llvm.vp.sitofp.*`
  * `llvm.vp.ptrtoint.*`
  * `llvm.vp.inttoptr.*`
  * `llvm.vp.fcmp.*`
  * `llvm.vp.icmp.*`
  * `llvm.vp.ceil.*`
  * `llvm.vp.floor.*`
  * `llvm.vp.rint.*`
  * `llvm.vp.nearbyint.*`
  * `llvm.vp.round.*`
  * `llvm.vp.roundeven.*`
  * `llvm.vp.roundtozero.*`
  * `llvm.vp.lrint.*`
  * `llvm.vp.llrint.*`
  * `llvm.vp.bitreverse.*`
  * `llvm.vp.bswap.*`
  * `llvm.vp.ctpop.*`
  * `llvm.vp.ctlz.*`
  * `llvm.vp.cttz.*`
  * `llvm.vp.sadd.sat.*`
  * `llvm.vp.uadd.sat.*`
  * `llvm.vp.ssub.sat.*`
  * `llvm.vp.usub.sat.*`
  * `llvm.vp.fshl.*`
  * `llvm.vp.fshr.*`
  * `llvm.vp.is.fpclass.*`

  These intrinsics previously only set masked-off lanes to poison, and will be
  automatically upgraded to their non-VP equivalent.  On RISC-V the VL optimizer
  should automatically infer `vl` in most cases from a store or reduction
  instruction, so passing around an explicit EVL operand shouldn't be required.
  If needed a "root" EVL can be synthesized with `llvm.vp.merge`, e.g:

  ```llvm
  %x = add <vscale x 2 x i32> %y, %z
  %res = call <vscale x 2 x i32> @llvm.vp.merge(<vscale x 2 x i32> %x, <vscale x 2 x i32> poison, <vscale x 2 x i1> splat (i1 true), i32 %evl)
  ```

  The `llvm.vp.merge` will be folded away but the `%evl` will be propagated to
  the add instruction.

### Changes to LLVM infrastructure

* Removed `TargetOptions::FloatABIType`. The soft float ABI should be
  controlled by setting the `"float-abi"` module flag.

### Changes to building LLVM

### Changes to TableGen

* `!cond` operator short-circuits at the first `true` condition.  Subsequent
  `condition : value` pairs, along with their corresponding side effects,
  are left unresolved.

### Changes to Interprocedural Optimizations

- Interprocedural passes no longer rewrite the signature of functions marked
  `optnone`, so their argument list, return type, and calling convention are
  preserved. Interprocedural analysis and transformation of such functions is
  otherwise unaffected.

- The IR Outliner has been removed, due to lack of a maintainer and the presence
  of correctness issues.

### Changes to Vectorizers

### Changes to the AArch64 Backend

### Changes to the AMDGPU Backend

* Replaced `xnack` and `sramecc` target features with `amdgpu.xnack`
  and `amdgpu.sramecc` module flags.
* `llvm.amdgcn.make.buffer.rsrc` now accepts any integer width for its
  `numRecords` argument to account for targets that use 32-bit and 45-bit
  `numRecords` widths more accurately. If an integer of the incorrect width
  is used, it will be zero-extended or truncated as needed.

### Changes to the ARM Backend

* Using the hard-float procedure call standard without floating-point registers
  is now an error. Previously this would fall back to the soft-float PCS while
  still emitting the hard-float ABI attribute tag.

### Changes to the AVR Backend

### Changes to the C166 Backend

* Added an experimental C166 backend with assembly, disassembly, ELF object
  emission, static linking, and initial C code generation for the Tiny, Small,
  Medium, Large, and Huge memory models.

### Changes to the DirectX Backend

### Changes to the Hexagon Backend

* v79 and later targets get code generation support for XQFloat, an
  extended-precision floating point format, along with an extraneous
  conversion removal pass and a post-register-allocation compliance checker.
* IEEE HVX floating point intrinsics are automatically translated to their
  QFloat equivalents on v79+ targets.
* `vselect` can now be lowered for HVX.
* Partial reduction intrinsics are now supported.
* HVX gained V128i1/V64i1/V32i1 predicate load and store support.
* The Machine Combiner pass is enabled for Hexagon.
* A new AggressiveRDF copy propagation pass extends RDF copy propagation
  with super-register and sub-register handling.
* The HexagonGlobalScheduler pass was added.
* ShadowCallStack (`-fsanitize=shadow-call-stack`) is now supported, along
  with a corresponding multilib.
* Stack clash protection can use `probe-stack=inline-asm`.
* KCFI is now supported.
* The CFI indirect-call sanitizer is now supported.
* `-ffixed-rXX` can reserve caller-saved registers r16-r28.
* A new HVX caller-save remark pass diagnoses call sites that force HVX
  register spills.
* Several Hexagon IR and MIR passes now emit optimization remarks.
* XRay custom and typed events are now supported.
* JITLink gained an ELF backend for Hexagon.
* The `.reloc` assembler directive is now supported.
* HVX build-vector lowering now reuses word splats for repeated words.
* Sign-extend-then-multiply patterns are now recognized and lowered to
  `vmpyh`.

### Changes to the LoongArch Backend

### Changes to the MIPS Backend

### Changes to the PowerPC Backend

* Added backend support for partial reductions and cost modeling for length-type
  VP intrinsic load/store.
* `vec_rl()` on `v4i32` now generates `xvrlw` when targeting `-mcpu=future`.
* `v256i1` loads and stores use paired vector instructions (`lxvp`/`stxvp`) on `-mcpu=future`.
* Added support for the `MSGSNDP` (Message Send Privileged) instruction.
* Enabled custom lowering for `__builtin_bswap64` on Power8 in 64-bit mode
  by splitting the 64-bit byte-swap into independent 32-bit high/low swaps
  for improved instruction-level parallelism. Power9 and later continue to
  use their existing paths.
* Added missing `BR_CC` handler in `DAGTypeLegalizer` for soft-promoted half
  operands, fixing a crash when a half-typed `fcmp` result feeds directly
  into a conditional branch.
* Various codegen improvements and bug fixes.
* AIX: Implemented support for the `ifunc` attribute.
* AIX: Implemented Function Multi-Versioning using `target_clones`; versioning by cpu only, and diagnosis of invalid feature strings on the `target` attribute.
* AIX: Added sorting of relocations in the XCOFF object writer.
* AIX: The default code model for 64-bit targets has been changed from `small`
  to `large`. This avoids the need for expensive linker fixups (e.g.
  `-Wl,-bigtoc`) that many applications require with the small code model.

### Changes to the RISC-V Backend

* Added experimental MC support for the `Smcsps` and `Sscsps`
  conditional stack pointer swap extensions.
* Adds experimental assembler/CodeGen support for the `Zilx` (Indexed Integer
  Load) extension.
* Added experimental MC support for the `Smijt` and `Ssijt` interrupt jump
  table extensions and the `Smehv` and `Ssehv` synchronous exception hardware
  vectoring extensions.
* Added experimental MC support for the `Smip` and `Ssip` interrupt handler
  push/pop extensions.
* Bump Svukte extension to 1.0.
* Remove experimental from Zicfiss.

### Changes to the SystemZ Backend

The SystemZ backend now contains initial support to generate code for z/OS using the XPLINK 64 bit
ABI and emitting the code into GOFF object files:

* Adds support for writing GOFF object files.
* Adds new class `SystemZHASMAsmStreamer` to emit assembly in HLASM syntax.
* The temporary GNU AS assembly output is removed; all assembly output is in
  HLASM syntax, and all z/OS-specific test cases are updated.
* Jump tables are now emitted into the text section.
* PPA1 data is now collected and written at the end of the code generation into
  the text section.
* The PPA1 now contains the prologue length and the offset to the stack update
  symbol.
* Adds a new attribute to control if the symbol name is emitted into the PPA1.
* Changed the order of caller-saved registers to match legacy compiler.
* Register R5 is no longer restored, matching the XPLINK specification.
* Implements stack guard support for XPLINK

Code-generation changes:
* Add support for the dataflow sanitizer.
* Add support for the `-mstack-protector-guard=global` and
  `-mstack-protector-guard-record` command line options.
* Fix incorrect code generated for the `vec_insert` intrinsic
   when used with the `vector float` data type.

Performance enhancements:
* Added a SystemZ-specific pre-RA scheduling strategy with latency-aware
   heuristics, liveness-reduction, and a minimum-latency-5 filter.
* Enabled interleaving for vectorized loops and epilogue loop vectorization.
* Enabled scalar load rematerialization.
* Cost model improvements to enable better auto-vectorization
* Improved codegen for minimum/maximum operations.
* Allow folding memory accesses across basic-block boundaries.
* Avoid stack overalignment for vector types.
* Avoid unaligned vector loads/stores in memcpy/memmove/memset lowering.
* Avoid redundant zero-extension after VLGV[BHF].

### Changes to the WebAssembly Backend

* Added support for emitting common symbols (.comm) using the WASM_SYMBOL_BINDING_COMMON
  flag (see https://github.com/WebAssembly/tool-conventions/pull/267)

### Changes to the Windows Target

### Changes to the X86 Backend

### Changes to the OCaml bindings

### Changes to the Python bindings

### Changes to the C API

### Changes to the CodeGen infrastructure

### Changes to the Metadata Info

### Changes to the Debug Info

### Changes to the LLVM tools

* llvm-mca no longer defaults -mcpu to "native"

### Changes to LLDB

#### SBAPI

* A [bug](https://github.com/llvm/llvm-project/issues/211787) involving SBValues
  representing a register set was fixed. The methods `GetIndexOfChildWithName`
  and `GetChildMemberWithName` were incorrectly looking up values in all
  register sets. This meant that `GetIndexOfChildWithName` could return an index
  greater than the size of the set, and that `GetChildMemberWithName` could
  return values that were actually in a different set. Both methods are now fixed
  so that they are limited to the registers within the register set. Scripts
  using these methods may have to be updated as a result.

#### Windows

* Python 3.11 or later is now required for building LLDB 24 on Windows.
* For better performance, LLDB now turns off the Windows debug heap by default when debugging.
  If you need the debug heap enabled, set `platform.plugin.windows.disable-debug-heap` to `false`.

### Changes to BOLT

* BOLT supports AArch64 binaries using Pointer Authentication (PAC) and Branch
  Target Identification (BTI). For PAC-enabled binaries, BOLT preserves pointer
  authentication CFI state during optimization. For BTI-enabled binaries, BOLT
  can patch PLT entries or indirect branch targets with BTI landing pads where
  possible.

* BOLT adds compact-code-model support for Armv9.6-A FEAT_CMPBR
  compare-and-branch instructions, including support for block reordering,
  function splitting, branch inversion where legal.

* BOLT supports AArch64 profile data collected with Arm SPE and branch-stack
  profiles from hardware such as BRBE. LLVM 23 adds pre-parsed perf-script and
  profile-format support for these workflows.

### Changes to Sanitizers

### Other Changes

* `cas::ObjectStore::getMemoryBuffer()` was documented as returning a buffer
  whose lifetime is independent of the CAS, but the buffer it returns may alias
  storage the CAS owns and so cannot outlive it. The documentation now matches
  the behavior, and the new `getStandaloneMemoryBuffer()` provides a buffer that
  does stay valid after the `ObjectStore` is destroyed.

## External Open Source Projects Using LLVM {{env.config.release}}

## Additional Information

A wide variety of additional information is available on the
[LLVM web page](https://llvm.org/), in particular in the
[documentation](https://llvm.org/docs/) section.  The web page also contains
versions of the API documentation which is up-to-date with the Git version of
the source code.  You can access versions of these documents specific to this
release by going into the `llvm/docs/` directory in the LLVM tree.

If you have any questions or comments about LLVM, please feel free to contact
us via the [Discourse forums](https://discourse.llvm.org).
