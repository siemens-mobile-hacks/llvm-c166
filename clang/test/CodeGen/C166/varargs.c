// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -c %s -o %t.medium.o
// RUN: %clang --target=c166-none-elf -mcmodel=small -O1 -mllvm -verify-machineinstrs -c %s -o %t.small.o
// RUN: %clang --target=c166-none-elf -mcmodel=huge -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.huge.o
// RUN: llvm-objdump -d %t.huge.o | FileCheck %s --check-prefix=HUGE
// RUN: %clang --target=c166-none-elf -mcmodel=huge -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.huge-o0.o
// RUN: llvm-objdump -d %t.huge-o0.o | FileCheck %s --check-prefix=HUGE-O0
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t.o
// RUN: llvm-objdump -d %t.o | FileCheck %s
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -fno-inline -mllvm -verify-machineinstrs -c %s -o %t-o0.o
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=OPT
// C166-ABI: varargs.unnamed_words_on_stack
// C166-ABI: varargs.va_list_stack_pointer
// C166-ABI: varargs.long_word_order
// C166-ABI: varargs.long_long_alias
// C166-ABI: varargs.far_pointer_word_order
// C166-ABI: varargs.float_word_order

#include <stdarg.h>

__attribute__((noinline))
unsigned int sum_words(unsigned int count, ...) {
  va_list args;
  unsigned int result = 0;
  va_start(args, count);
  while (count--)
    result += va_arg(args, unsigned int);
  va_end(args);
  return result;
}

__attribute__((noinline)) unsigned int consume_one(va_list *args) {
  return va_arg(*args, unsigned int);
}

__attribute__((noinline)) unsigned int escaped_varargs(unsigned int tag, ...) {
  va_list args;
  va_start(args, tag);
  unsigned int first = consume_one(&args);
  unsigned int second = va_arg(args, unsigned int);
  va_end(args);
  return first + second;
}

__attribute__((noinline, optnone))
unsigned int unoptimized_varargs(unsigned int tag, ...) {
  va_list args;
  va_start(args, tag);
  unsigned int result = va_arg(args, unsigned int);
  va_end(args);
  return result;
}

// Keep the va_list cursor in SSA so repeated va_arg operations do not spill it
// to a local stack slot.
// OPT-LABEL: define{{.*}} i16 @sum_words(
// OPT-NOT:   alloca
// OPT:       call addrspace(1) ptr addrspace(2) @llvm.c166.va.start.p2()
// OPT:       phi ptr addrspace(2)
// OPT:       load i16, ptr addrspace(2)
// OPT-NOT:   @llvm.va_start
// OPT:       ret i16

__attribute__((noinline))
unsigned long take_long(unsigned int tag, ...) {
  va_list args;
  va_start(args, tag);
  unsigned long value = va_arg(args, unsigned long);
  va_end(args);
  return value;
}

__attribute__((noinline))
unsigned long long take_long_long(unsigned int tag, ...) {
  va_list args;
  va_start(args, tag);
  unsigned long long value = va_arg(args, unsigned long long);
  va_end(args);
  return value;
}

__attribute__((noinline))
void *take_pointer(unsigned int tag, ...) {
  va_list args;
  va_start(args, tag);
  void *value = va_arg(args, void *);
  va_end(args);
  return value;
}

__attribute__((noinline))
unsigned int fixed_stack_then_vararg(unsigned int first, unsigned int second,
                                     unsigned int third, unsigned int fourth,
                                     unsigned int fifth, ...) {
  va_list args;
  va_start(args, fifth);
  return first + second + third + fourth + fifth +
         va_arg(args, unsigned int);
}

__attribute__((noinline))
long take_promoted_float(unsigned int tag, ...) {
  va_list args;
  va_start(args, tag);
  double value = va_arg(args, double);
  va_end(args);
  return (long)value;
}

__attribute__((noinline))
double take_double_words(unsigned int tag, ...) {
  va_list args;
  va_start(args, tag);
  double value = va_arg(args, double);
  va_end(args);
  return value;
}

unsigned int call_sum_words(void) {
  return sum_words(3, 10, 20, 30);
}

unsigned long call_take_long(unsigned long value) {
  return take_long(1, value);
}

unsigned long long call_take_long_long(unsigned long long value) {
  return take_long_long(1, value);
}

void *call_take_pointer(void *value) {
  return take_pointer(1, value);
}

unsigned int call_fixed_stack_then_vararg(void) {
  return fixed_stack_then_vararg(1, 2, 3, 4, 5, 6);
}

long call_take_promoted_float(float value) {
  return take_promoted_float(1, value);
}

struct aggregate_pair {
  unsigned int first;
  unsigned int second;
};

struct __attribute__((packed)) aggregate_packed {
  unsigned char first;
  unsigned int second;
};

unsigned int take_aggregate_varargs(unsigned int tag, ...) {
  va_list args;
  struct aggregate_pair pair;
  struct aggregate_packed packed;
  va_start(args, tag);
  pair = va_arg(args, struct aggregate_pair);
  packed = va_arg(args, struct aggregate_packed);
  va_end(args);
  return pair.first + pair.second + packed.first + packed.second;
}

// Aggregate varargs are values stored inline in the user-stack stream.  In
// particular, their first two words must not be emitted as an address-space-0
// pointer value as DefaultABIInfo does for an indirect/byval aggregate.
// IR-LABEL: define{{.*}} i16 @take_aggregate_varargs(
// IR:       %[[PAIR_CUR:[^ ]+]] = load ptr addrspace(2), ptr addrspace(2) {{[^,]+}}, align 2
// IR-NEXT:  %[[PAIR_NEXT:[^ ]+]] = getelementptr inbounds i8, ptr addrspace(2) %[[PAIR_CUR]], i16 4
// IR-NEXT:  store ptr addrspace(2) %[[PAIR_NEXT]], ptr addrspace(2) {{[^,]+}}, align 2
// IR:       call addrspace(1) void @llvm.memcpy.p2.p2.i16({{.*}}ptr addrspace(2) align 2 %[[PAIR_CUR]], i16 4, i1 false)
// IR:       %[[PACKED_CUR:[^ ]+]] = load ptr addrspace(2), ptr addrspace(2) {{[^,]+}}, align 2
// IR-NEXT:  %[[PACKED_NEXT:[^ ]+]] = getelementptr inbounds i8, ptr addrspace(2) %[[PACKED_CUR]], i16 4
// IR-NEXT:  store ptr addrspace(2) %[[PACKED_NEXT]], ptr addrspace(2) {{[^,]+}}, align 2
// IR:       call addrspace(1) void @llvm.memcpy.p2.p2.i16({{.*}}ptr addrspace(2) align 2 %[[PACKED_CUR]], i16 3, i1 false)

// Named arguments use R12-R15, but every unnamed argument is stack-only.
// va_start constructs a far pointer from the 14-bit user-stack offset and
// DPP1.  A dynamic va_arg cursor uses that pointer, while fixed accesses
// proven to remain in the current frame use the shorter direct stack path.
// CHECK-LABEL: <_sum_words>:
// CHECK:       and
// CHECK:       mov {{r[0-9]+}}, dpp1
// CHECK:       extp {{r[0-9]+}}, #1
// CHECK:       rets

// HUGE-LABEL: <_sum_words>:
// HUGE:       and [[OFFSET:r[0-9]+]], #16383
// HUGE:       mov [[PAGE:r[0-9]+]], dpp1
// HUGE-NEXT:  shl [[PAGE]], #14
// HUGE-NEXT:  or [[OFFSET]], [[PAGE]]
// HUGE:       mov [[SEGMENT:r[0-9]+]], dpp1
// HUGE-NEXT:  shr [[SEGMENT]], #2
// HUGE:       exts [[SEGMENT]], #1

// The canonical O0 va_arg path must dereference that SEG:SOF pointer with
// EXTS as well; using EXTP would reinterpret the segment word as a DPP page.
// HUGE-O0-LABEL: <_unoptimized_varargs>:
// HUGE-O0:       shl {{r[0-9]+}}, #14
// HUGE-O0:       or
// HUGE-O0:       shr {{r[0-9]+}}, #2
// HUGE-O0:       exts {{r[0-9]+}}, #1
// CHECK-LABEL: <_take_long>:
// CHECK-NOT:   extp
// CHECK:       mov r4, [r0]
// CHECK:       mov r5, [r0 + #2]
// CHECK:       rets
// CHECK-LABEL: <_take_long_long>:
// CHECK-NOT:   extp
// CHECK:       mov r4, [r0]
// CHECK:       mov r5, [r0 + #2]
// CHECK:       rets
// CHECK-LABEL: <_take_pointer>:
// CHECK-NOT:   extp
// CHECK:       mov r4, [r0]
// CHECK:       mov r5, [r0 + #2]
// CHECK:       rets
// CHECK-LABEL: <_fixed_stack_then_vararg>:
// CHECK:       mov {{r[0-9]+}}, [r0]
// CHECK-NOT:   extp
// CHECK:       mov r4, [r0 + #2]
// CHECK:       rets
// CHECK-LABEL: <_take_promoted_float>:
// CHECK-NOT:   extp
// CHECK:       mov {{r[0-9]+}}, [r0 + #4]
// CHECK:       mov {{r[0-9]+}}, [r0 + #6]
// CHECK:       calls
// CHECK:       rets
// CHECK-LABEL: <_take_double_words>:
// CHECK-NOT:   extp
// CHECK:       mov {{r[0-9]+}}, r0
// CHECK:       mov [[DOUBLE_CURSOR:r[0-9]+]], r0
// CHECK-NEXT:  mov {{r[0-9]+}}, [[[DOUBLE_CURSOR]]+]
// CHECK-NEXT:  mov {{r[0-9]+}}, [[[DOUBLE_CURSOR]]+]
// CHECK-NEXT:  mov {{r[0-9]+}}, [[[DOUBLE_CURSOR]]+]
// CHECK-NEXT:  mov {{r[0-9]+}}, [[[DOUBLE_CURSOR]]]
// CHECK:       rets
// CHECK-LABEL: <_call_sum_words>:
// CHECK-COUNT-3: mov [-r0], {{r[0-9]+}}
// CHECK:       mov r12, #3
// CHECK:       calls
// CHECK:       add r0, #6
// CHECK:       rets
// CHECK-LABEL: <_call_take_long>:
// CHECK:       mov [-r0], r13
// CHECK-NEXT:  mov [-r0], r12
// CHECK:       calls
// CHECK:       add r0, #4
// CHECK:       rets
// CHECK-LABEL: <_call_take_long_long>:
// CHECK:       mov [-r0], r13
// CHECK-NEXT:  mov [-r0], r12
// CHECK:       calls
// CHECK:       add r0, #4
// CHECK:       rets
// CHECK-LABEL: <_call_take_pointer>:
// CHECK:       mov [-r0], r13
// CHECK-NEXT:  mov [-r0], r12
// CHECK:       calls
// CHECK:       add r0, #4
// CHECK:       rets
// CHECK-LABEL: <_call_fixed_stack_then_vararg>:
// CHECK-COUNT-2: mov [-r0], {{r[0-9]+}}
// CHECK:       mov r12, #1
// CHECK:       mov r13, #2
// CHECK:       mov r14, #3
// CHECK:       mov r15, #4
// CHECK:       calls
// CHECK:       add r0, #4
// CHECK:       rets
