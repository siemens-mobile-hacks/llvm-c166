// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O2 -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: llvm-objdump -d %t-o0.o | FileCheck %s --check-prefix=O0

long sub_long(long a, long b) { return a - b; }
long neg_long(long value) { return -value; }

int eq_long(long a, long b) { return a == b; }
int ne_long(long a, long b) { return a != b; }
int lt_long(long a, long b) { return a < b; }
int le_long(long a, long b) { return a <= b; }
int gt_long(long a, long b) { return a > b; }
int ge_long(long a, long b) { return a >= b; }
int ult_long(unsigned long a, unsigned long b) { return a < b; }
int ule_long(unsigned long a, unsigned long b) { return a <= b; }
int ugt_long(unsigned long a, unsigned long b) { return a > b; }
int uge_long(unsigned long a, unsigned long b) { return a >= b; }

int branch_long(long a, long b) { return a < b ? 7 : 9; }
long select_long(long a, long b) { return a < b ? a : b; }
long select_long_on_int(long a, long b, int condition) {
  return condition ? a : b;
}
extern void less_sink(void);
void direct_branch_long(long a, long b) {
  if (a < b)
    less_sink();
}

int shl_int(int value, unsigned int amount) { return value << amount; }
int ashr_int(int value, unsigned int amount) { return value >> amount; }
unsigned int lshr_int(unsigned int value, unsigned int amount) {
  return value >> amount;
}

// Arithmetic subtraction uses the native low-word SUB/high-word SUBC chain.
// CHECK-LABEL: <_sub_long>:
// CHECK:       sub r4, r14
// CHECK-NEXT:  subc r5, r15
// CHECK:       rets
// CHECK-LABEL: <_neg_long>:
// CHECK:       sub r4, r12
// CHECK-NEXT:  subc r5, r13
// CHECK:       rets

// A comparison checks the high words first, signed for signed long and
// unsigned otherwise, then checks the low words as unsigned when the highs
// are equal.  Every CMP must remain adjacent to its JMPR because MOV updates
// N/Z/E on C166.
// CHECK-LABEL: <_eq_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_{{(eq|ne)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_{{(eq|ne)}}
// CHECK-LABEL: <_ne_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_{{(eq|ne)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_{{(eq|ne)}}
// CHECK-LABEL: <_lt_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK-LABEL: <_le_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK-LABEL: <_gt_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK-LABEL: <_ge_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK-LABEL: <_ult_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK-LABEL: <_ule_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK-LABEL: <_ugt_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK-LABEL: <_uge_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}

// CHECK-LABEL: <_branch_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK-LABEL: <_select_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_s{{(lt|le|gt|ge)}}
// CHECK:       cmp
// CHECK-NEXT:  jmpr cc_u{{(lt|le|gt|ge)}}
// CHECK-LABEL: <_select_long_on_int>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr
// CHECK-LABEL: <_direct_branch_long>:
// CHECK:       cmp
// CHECK-NEXT:  jmpr
// CHECK:       cmp
// CHECK-NEXT:  jmpr
// CHECK:       cmp
// CHECK-NEXT:  jmpr

// At O0 the boolean values are spilled before the compare tree.  These
// adjacency checks catch a flag-clobbering spill between CMP and signed JMPR.
// O0-LABEL: <_lt_long>:
// O0:       cmp
// O0-NEXT:  jmpr cc_slt
// O0:       cmp
// O0-NEXT:  jmpr cc_ult
// O0:       cmp
// O0-NEXT:  jmpr cc_sgt

// CHECK-LABEL: <_shl_int>:
// CHECK:       shl r4, r13
// CHECK-LABEL: <_ashr_int>:
// CHECK:       ashr r4, r13
// CHECK-LABEL: <_lshr_int>:
// CHECK:       shr r4, r13
