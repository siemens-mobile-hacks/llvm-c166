// REQUIRES: c166-registered-target
// RUN: %clang_cc1 -triple c166-none-elf -O0 -emit-llvm %s -o - | FileCheck %s --check-prefixes=IR,FAR
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=medium -O0 -emit-llvm %s -o - | FileCheck %s --check-prefixes=IR,FAR
// RUN: %clang_cc1 -triple c166-none-elf -mcmodel=small -O0 -emit-llvm %s -o - | FileCheck %s --check-prefixes=IR,SMALL
// RUN: %clang --target=c166-none-elf -O0 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O0 -mllvm -verify-machineinstrs -S %s -o %t-medium.s
// RUN: %clang --target=c166-none-elf -mcmodel=small -O0 -mllvm -verify-machineinstrs -S %s -o %t-small.s
// RUN: %clang --target=c166-none-elf -O2 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=OPT
// RUN: %clang_cc1 -triple c166-none-elf -O1 -debug-info-kind=limited -dwarf-version=5 -emit-obj %s -o %t.o
// RUN: llvm-dwarfdump --verify %t.o 2>&1 | FileCheck %s --check-prefix=VERIFY
// RUN: llvm-dwarfdump --debug-frame %t.o | FileCheck %s --check-prefix=CFI

typedef unsigned int u16;

extern void consume(void *, u16 *);

// IR-LABEL: define{{.*}} void @variable_array(
// FAR: alloca i16, align 2, addrspace(2)
// FAR: alloca i8, i16 {{.*}}, align 1, addrspace(2)
// SMALL: alloca i16, align 2, addrspace(3)
// SMALL: alloca i8, i16 {{.*}}, align 1, addrspace(3)
void variable_array(u16 count) {
  u16 fixed = count;
  unsigned char bytes[count];
  bytes[0] = (unsigned char)fixed;
  consume(bytes, &fixed);
}

// IR-LABEL: define{{.*}} void @builtin_alloca(
// FAR: alloca i8, i16 {{.*}}, align 2, addrspace(2)
// SMALL: alloca i8, i16 {{.*}}, align 2, addrspace(3)
void builtin_alloca(u16 count) {
  u16 fixed = count;
  unsigned char *bytes = __builtin_alloca(count);
  bytes[0] = (unsigned char)count;
  consume(bytes, &fixed);
}

// IR-LABEL: define{{.*}} i16 @aligned_builtin_alloca(
// FAR: alloca i8, i16 {{.*}}, align 4, addrspace(2)
// SMALL: alloca i8, i16 {{.*}}, align 4, addrspace(3)
u16 aligned_builtin_alloca(u16 count) {
  u16 fixed = count;
  unsigned char *bytes = __builtin_alloca_with_align(count, 32);
  bytes[0] = (unsigned char)count;
  consume(bytes, &fixed);
  return (u16)((unsigned long)bytes & 3UL);
}

// ASM-LABEL: _variable_array:
// ASM:       mov [-r0], r6
// ASM:       mov r6, r0
// ASM:       sub [[NEW:r[0-9]+]], [[SIZE:r[0-9]+]]
// ASM:       mov r0, [[NEW]]
// ASM:       calls seg(_consume), sof(_consume)
// ASM:       mov r0, r6
// ASM:       mov r6, [r0+]

// A final call in a dynamic frame cannot become a tail call: the fixed frame
// and the saved frame register must be restored first.
// OPT-LABEL: _builtin_alloca:
// OPT:       calls seg(_consume), sof(_consume)
// OPT-NEXT:  mov r0, r6
// OPT:       mov r6, [r0+]
// OPT-NEXT:  rets

// VERIFY: No errors.
// CFI: DW_CFA_val_expression: R0 DW_OP_breg6 R6+
// CFI: DW_CFA_expression: R6 {{.*}}DW_OP_breg6 R6+
