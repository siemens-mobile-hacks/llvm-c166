// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -ffp-contract=off -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -ffp-contract=off -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -dr %t.o | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t.contract.o
// RUN: llvm-objdump -dr %t.contract.o | FileCheck %s --check-prefix=CONTRACT

double repeated_product(double left, double right) {
  return left * right + left * right;
}

double contracted_multiply_add(double left, double right, double addend) {
  return left * right + addend;
}

double constant_addend(double left, double right) {
  return left * right + 1.0;
}

double chained_arithmetic(double left, double right, double factor) {
  return (left + right) * factor;
}

double selected_operation(unsigned operation, double left, double right) {
  switch (operation) {
  case 0:
    return left + right;
  case 1:
    return left - right;
  case 2:
    return left * right;
  default:
    return left / right;
  }
}

unsigned compare_mask(double left, double right) {
  unsigned result = 0;
  if (left == right)
    result |= 1u;
  if (left != right)
    result |= 2u;
  if (left < right)
    result |= 4u;
  if (left <= right)
    result |= 8u;
  if (left > right)
    result |= 16u;
  if (left >= right)
    result |= 32u;
  return result;
}

volatile double global_left;
volatile double global_right;

double selected_global_operation(unsigned operation) {
  double left = global_left;
  double right = global_right;
  switch (operation) {
  case 0:
    return left + right;
  case 1:
    return left - right;
  case 2:
    return left * right;
  case 3:
    return left / right;
  default:
    return 0.0;
  }
}

// Floating arithmetic remains visible to the generic optimizer before the
// C166 runtime boundary is formed.  GVN therefore shares the multiplication.
// IR-LABEL: define{{.*}} void @repeated_product(
// IR:       [[PRODUCT:%.*]] = fmul double {{.*}}, {{.*}}
// IR-NOT:   fmul double
// IR:       fadd double [[PRODUCT]], [[PRODUCT]]

// ASM-LABEL: <_repeated_product>:
// ASM:       R_C166_SEG24 ___c166_muldf3
// ASM-NOT:   R_C166_SEG24 ___c166_muldf3
// ASM:       R_C166_SEG24 ___c166_adddf3

// The product of a contracted multiply-add is written directly into the
// final result block.  The in-place add then needs no local binary64 slot.
// CONTRACT-LABEL: <_contracted_multiply_add>:
// CONTRACT-NOT:   sub r0
// CONTRACT:       R_C166_SEG24 ___c166_muldf3
// CONTRACT:       R_C166_SEG24 ___c166_adddf3
// CONTRACT-NOT:   add r0
// CONTRACT:       rets

// A constant runtime operand is initialized after its entry-block slot.
// CONTRACT-LABEL: <_constant_addend>:
// CONTRACT:       R_C166_SEG24 ___c166_muldf3
// CONTRACT:       R_C166_SEG24 ___c166_adddf3
// CONTRACT:       rets

// Keep the expression on the direct-stack runtime boundary. Intermediate
// snapshots may need local storage when memory accesses separate operations.
// ASM-LABEL: <_chained_arithmetic>:
// ASM:       R_C166_SEG24 ___c166_adddf3
// ASM:       R_C166_SEG24 ___c166_muldf3
// ASM:       rets

// A stack-only double argument remains usable when control flow carries its
// address into another basic block.
// ASM-LABEL: <_selected_operation>:
// ASM-DAG:    R_C166_SEG24 ___c166_adddf3
// ASM-DAG:    R_C166_SEG24 ___c166_subdf3
// ASM-DAG:    R_C166_SEG24 ___c166_muldf3
// ASM-DAG:    R_C166_SEG24 ___c166_divdf3

// Comparisons of the same values share one direct-stack four-way comparison.
// The predicate tests consume its relation mask without rebuilding either
// public binary64 argument for each operator.
// ASM-LABEL: <_compare_mask>:
// ASM-NOT:   sub r0
// ASM:       R_C166_SEG24 ___c166_cmpdf2
// ASM-NOT:   R_C166_SEG24 ___c166_cmpdf2
// ASM-NOT:   R_C166_SEG24 ___{{(cmp|eq|ne|lt|le|gt|ge|unord)}}df2
// ASM:       rets

// Values shared by several switch successors are copied into their runtime
// operand slots before the dispatch branches.  Every case therefore observes
// initialized operands rather than depending on whichever case was lowered
// first.
// ASM-LABEL: <_selected_global_operation>:
// ASM:         mov [r0 + #{{[0-9]+}}],
// ASM:         mov [r0 + #{{[0-9]+}}],
// ASM:         mov [r0 + #{{[0-9]+}}],
// ASM:         mov [r0 + #{{[0-9]+}}],
// ASM:         cmp
// ASM-NOT:     jmpi
// ASM:         rets
