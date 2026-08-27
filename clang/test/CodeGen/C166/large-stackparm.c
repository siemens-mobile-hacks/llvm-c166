// REQUIRES: c166-registered-target
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang --target=c166-none-elf -mcmodel=large -O1 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: %clang --target=c166-none-elf -mcmodel=medium -O1 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s --check-prefix=MEDIUM
// RUN: %clang --target=c166-none-elf -mcmodel=large -O0 -mllvm -verify-machineinstrs -c %s -o %t.o
// C166-ABI: args.stackparm
// C166-ABI: floating.stackparm_boundary
// C166-ABI: medium.stackparm.boundary

typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#define STACKPARM __attribute__((c166_stackparm))

extern u16 STACKPARM stackparm_external(u8 a, u16 b, u32 c, u16 d);
typedef u16 STACKPARM stackparm_function(u8 a, u16 b, u32 c, u16 d);
extern float STACKPARM stackparm_float_external(float value, u16 tail);
extern double STACKPARM stackparm_double_external(double value, u16 tail);
typedef float STACKPARM stackparm_float_function(float value, u16 tail);
typedef double STACKPARM stackparm_double_function(double value, u16 tail);

u16 STACKPARM stackparm_callee(u8 a, u16 b, u32 c, u16 d) {
  return (u16)a + b + (u16)c + d;
}

u16 stackparm_direct(u8 a, u16 b, u32 c, u16 d) {
  return stackparm_external(a, b, c, d);
}

u16 stackparm_indirect(stackparm_function *function, u8 a, u16 b, u32 c,
                       u16 d) {
  return function(a, b, c, d);
}

float STACKPARM stackparm_float_identity(float value, u16 tail) {
  return value;
}

double STACKPARM stackparm_double_identity(double value, u16 tail) {
  return value;
}

float stackparm_float_direct(float value, u16 tail) {
  return stackparm_float_external(value, tail);
}

double stackparm_double_direct(double value, u16 tail) {
  return stackparm_double_external(value, tail);
}

float stackparm_float_indirect(stackparm_float_function *function,
                               float value, u16 tail) {
  return function(value, tail);
}

double stackparm_double_indirect(stackparm_double_function *function,
                                 double value, u16 tail) {
  return function(value, tail);
}

// IR: define{{.*}}cc128{{.*}}i16 @stackparm_callee(i8{{.*}}, i16{{.*}}, i32{{.*}}, i16{{.*}})
// IR: call{{.*}}cc128{{.*}}i16 @stackparm_external
// IR: call{{.*}}cc128{{.*}}i16 %{{.*}}

// ASM-LABEL: _stackparm_callee:
// ASM-DAG:   movb {{r[lh][0-7]}}, [r0]
// ASM-DAG:   mov {{r[0-9]+}}, [r0 + #2]
// ASM-DAG:   mov {{r[0-9]+}}, [r0 + #4]
// ASM-DAG:   mov {{r[0-9]+}}, [r0 + #8]
// ASM:       rets

// In Medium the argument/result protocol and cleanup sizes are unchanged,
// while default stackparm functions and their pointers use the near call
// class.
// MEDIUM-LABEL: _stackparm_callee:
// MEDIUM-DAG:   movb {{r[lh][0-7]}}, [r0]
// MEDIUM-DAG:   mov {{r[0-9]+}}, [r0 + #8]
// MEDIUM:       ret
// MEDIUM-LABEL: _stackparm_direct:
// MEDIUM:       sub r0, #6
// MEDIUM-NEXT:  sub r0, #4
// MEDIUM:       calla cc_uc, cof(_stackparm_external)
// MEDIUM-NEXT:  add r0, #6
// MEDIUM-NEXT:  add r0, #4
// MEDIUM-NEXT:  ret
// MEDIUM-LABEL: _stackparm_indirect:
// MEDIUM:       calli cc_uc, [r{{[0-9]+}}]
// MEDIUM:       add r0, #4
// MEDIUM-NEXT:  ret
// MEDIUM-LABEL: _stackparm_float_identity:
// MEDIUM:       mov r4, [r0]
// MEDIUM-NEXT:  mov r5, [r0 + #2]
// MEDIUM-NEXT:  ret
// MEDIUM-LABEL: _stackparm_double_identity:
// MEDIUM:       mov [r0 + #10], {{r[0-9]+}}
// MEDIUM:       mov [r0 + #12], {{r[0-9]+}}
// MEDIUM-NEXT:  mov r10, r4
// MEDIUM-NEXT:  ret
// MEDIUM-LABEL: _stackparm_float_direct:
// MEDIUM:       calla cc_uc, cof(_stackparm_float_external)
// MEDIUM:       ret
// MEDIUM-LABEL: _stackparm_double_direct:
// MEDIUM:       calla cc_uc, cof(_stackparm_double_external)
// MEDIUM:       ret
// MEDIUM-LABEL: _stackparm_float_indirect:
// MEDIUM:       calli cc_uc, [r{{[0-9]+}}]
// MEDIUM:       ret
// MEDIUM-LABEL: _stackparm_double_indirect:
// MEDIUM:       calli cc_uc, [r{{[0-9]+}}]
// MEDIUM:       ret
// ASM-LABEL: _stackparm_direct:
// ASM:       sub r0, #6
// ASM-NEXT:  sub r0, #4
// ASM-DAG:   mov [r0], {{r[0-9]+}}
// ASM-DAG:   mov [r0 + #2], {{r[0-9]+}}
// ASM-DAG:   mov [r0 + #4], {{r[0-9]+}}
// ASM-DAG:   mov [r0 + #6], {{r[0-9]+}}
// ASM-DAG:   mov [r0 + #8], {{r[0-9]+}}
// ASM:       calls seg(_stackparm_external), sof(_stackparm_external)
// ASM-NEXT:  add r0, #6
// ASM-NEXT:  add r0, #4
// ASM-NEXT:  rets
// ASM-LABEL: _stackparm_indirect:
// ASM:       sub r0, #6
// ASM-NEXT:  sub r0, #4
// ASM:       calls seg(__icall), sof(__icall)
// ASM-NEXT:  add r0, #6
// ASM-NEXT:  add r0, #4
// ASM-NEXT:  rets

// IR: define{{.*}}cc128{{.*}}float @stackparm_float_identity
// IR: define{{.*}}cc128 void @stackparm_double_identity
// IR: call{{.*}}cc128{{.*}}float @stackparm_float_external
// IR: call{{.*}}cc128{{.*}}void @stackparm_double_external
// IR: call{{.*}}cc128{{.*}}float %{{.*}}
// IR: call{{.*}}cc128{{.*}}void %{{.*}}

// ASM-LABEL: _stackparm_float_identity:
// ASM:       mov r4, [r0]
// ASM-NEXT:  mov r5, [r0 + #2]
// ASM-NEXT:  rets
// ASM-LABEL: _stackparm_double_identity:
// ASM:       mov {{r[0-9]+}}, [r0]
// ASM:       mov [r0 + #10], {{r[0-9]+}}
// ASM:       mov r10, r4
// ASM-NEXT:  rets
// ASM-LABEL: _stackparm_float_direct:
// ASM:       sub r0, #6
// ASM:       calls seg(_stackparm_float_external), sof(_stackparm_float_external)
// ASM-NEXT:  add r0, #6
// ASM-NEXT:  rets
// ASM-LABEL: _stackparm_double_direct:
// ASM:       calls seg(_stackparm_double_external), sof(_stackparm_double_external)
// ASM:       mov {{r[0-9]+}}, [r4]
// ASM:       rets
// ASM-LABEL: _stackparm_float_indirect:
// ASM:       sub r0, #6
// ASM:       calls seg(__icall), sof(__icall)
// ASM-NEXT:  add r0, #6
// ASM-NEXT:  rets
// ASM-LABEL: _stackparm_double_indirect:
// ASM:       calls seg(__icall), sof(__icall)
// ASM:       mov {{r[0-9]+}}, [r4]
// ASM:       rets
