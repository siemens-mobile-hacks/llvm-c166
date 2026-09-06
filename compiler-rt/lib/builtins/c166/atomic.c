//===-- atomic.c - C166 atomic runtime ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// C166 is a single-core architecture. C threads which can preempt
// one another are interrupt/PEC contexts, controlled globally by PSW.IEN.
// Preserve its incoming state so these helpers are safe both in ordinary code
// and when called recursively from an interrupt handler.  The leading ATOMIC
// sequence closes the sampling/clearing race and also masks PEC/Class-A traps
// while PSW is captured and IEN is cleared.
//
// The public generic entry points use the model's ordinary data pointers and
// 16-bit size_t/int. LLVM's generic AtomicExpand helper ABI assumes
// intptr_t == size_t and a 32-bit int, neither of which describes every C166
// model. Multi-byte operations are therefore non-lock-free but remain
// indivisible with respect to every standard interrupt and PEC service.
//
//===----------------------------------------------------------------------===//

#include "../int_lib.h"

#pragma redefine_extname __c166_atomic_is_lock_free ___atomic_is_lock_free
#pragma redefine_extname __c166_atomic_load ___atomic_load
#pragma redefine_extname __c166_atomic_store ___atomic_store
#pragma redefine_extname __c166_atomic_exchange ___atomic_exchange
#pragma redefine_extname __c166_atomic_compare_exchange ___atomic_compare_exchange

typedef unsigned char c166_atomic_byte;

enum {
  C166_ATOMIC_ADD,
  C166_ATOMIC_SUB,
  C166_ATOMIC_AND,
  C166_ATOMIC_OR,
  C166_ATOMIC_XOR,
  C166_ATOMIC_NAND,
  C166_ATOMIC_MAX,
  C166_ATOMIC_MIN,
  C166_ATOMIC_UMAX,
  C166_ATOMIC_UMIN,
};

static __inline __attribute__((always_inline)) unsigned int
c166_atomic_enter(void) {
  unsigned int SavedPSW;
  __asm__ volatile("atomic #2\n\tmov %0, psw\n\tbclr psw.11"
                   : "=r"(SavedPSW)
                   :
                   : "cc", "memory");
  return SavedPSW;
}

static __inline __attribute__((always_inline)) void
c166_atomic_leave(unsigned int SavedPSW) {
  __asm__ volatile("bmov psw.11, %0.11" : : "r"(SavedPSW) : "cc", "memory");
}

static __inline __attribute__((always_inline)) void
c166_atomic_copy(c166_atomic_byte volatile *Destination,
                 const c166_atomic_byte volatile *Source, unsigned int Size) {
  while (Size != 0) {
    *Destination++ = *Source++;
    --Size;
  }
}

static __inline __attribute__((always_inline)) int
c166_atomic_compare(const c166_atomic_byte volatile *Left,
                    const c166_atomic_byte volatile *Right, unsigned int Size,
                    unsigned int Signed) {
  if (Signed && Size != 0) {
    unsigned int LeftNegative = Left[Size - 1] >> 7;
    unsigned int RightNegative = Right[Size - 1] >> 7;
    if (LeftNegative != RightNegative)
      return LeftNegative ? -1 : 1;
  }

  while (Size != 0) {
    c166_atomic_byte L = Left[Size - 1];
    c166_atomic_byte R = Right[Size - 1];
    if (L != R)
      return L < R ? -1 : 1;
    --Size;
  }
  return 0;
}

COMPILER_RT_ABI _Bool __c166_atomic_is_lock_free(unsigned int size,
                                                 const volatile void *pointer) {
  (void)size;
  (void)pointer;
  return 0;
}

COMPILER_RT_ABI void __c166_atomic_load(unsigned int Size,
                                        const volatile void *Object,
                                        void *Result, int Order) {
  unsigned int SavedPSW;
  (void)Order;
  SavedPSW = c166_atomic_enter();
  c166_atomic_copy((c166_atomic_byte volatile *)Result,
                   (const c166_atomic_byte volatile *)Object, Size);
  c166_atomic_leave(SavedPSW);
}

COMPILER_RT_ABI void __c166_atomic_store(unsigned int Size,
                                         volatile void *Object,
                                         const void *Value, int Order) {
  unsigned int SavedPSW;
  (void)Order;
  SavedPSW = c166_atomic_enter();
  c166_atomic_copy((c166_atomic_byte volatile *)Object,
                   (const c166_atomic_byte volatile *)Value, Size);
  c166_atomic_leave(SavedPSW);
}

COMPILER_RT_ABI void __c166_atomic_exchange(unsigned int Size,
                                            volatile void *Object,
                                            const void *Value, void *Result,
                                            int Order) {
  unsigned int SavedPSW;
  c166_atomic_byte volatile *ObjectBytes = (c166_atomic_byte volatile *)Object;
  const c166_atomic_byte volatile *ValueBytes =
      (const c166_atomic_byte volatile *)Value;
  c166_atomic_byte volatile *ResultBytes = (c166_atomic_byte volatile *)Result;
  (void)Order;

  SavedPSW = c166_atomic_enter();
  while (Size != 0) {
    c166_atomic_byte Old = *ObjectBytes;
    *ResultBytes++ = Old;
    *ObjectBytes++ = *ValueBytes++;
    --Size;
  }
  c166_atomic_leave(SavedPSW);
}

COMPILER_RT_ABI _Bool
__c166_atomic_compare_exchange(unsigned int Size, volatile void *Object,
                               void *Expected, const void *Desired,
                               int SuccessOrder, int FailureOrder) {
  unsigned int SavedPSW;
  unsigned int I;
  _Bool Equal = 1;
  c166_atomic_byte volatile *ObjectBytes = (c166_atomic_byte volatile *)Object;
  c166_atomic_byte volatile *ExpectedBytes =
      (c166_atomic_byte volatile *)Expected;
  const c166_atomic_byte volatile *DesiredBytes =
      (const c166_atomic_byte volatile *)Desired;
  (void)SuccessOrder;
  (void)FailureOrder;

  SavedPSW = c166_atomic_enter();
  for (I = 0; I != Size; ++I)
    if (ObjectBytes[I] != ExpectedBytes[I]) {
      Equal = 0;
      break;
    }
  if (Equal)
    c166_atomic_copy(ObjectBytes, DesiredBytes, Size);
  else
    c166_atomic_copy(ExpectedBytes, ObjectBytes, Size);
  c166_atomic_leave(SavedPSW);
  return Equal;
}

COMPILER_RT_ABI void __c166_atomic_rmw(unsigned int Size, volatile void *Object,
                                       const void *Value, void *Result,
                                       unsigned int Operation) {
  unsigned int SavedPSW;
  unsigned int Carry = 0;
  int Comparison = 0;
  c166_atomic_byte volatile *ObjectBytes = (c166_atomic_byte volatile *)Object;
  const c166_atomic_byte volatile *ValueBytes =
      (const c166_atomic_byte volatile *)Value;
  c166_atomic_byte volatile *ResultBytes = (c166_atomic_byte volatile *)Result;
  SavedPSW = c166_atomic_enter();
  if (Operation >= C166_ATOMIC_MAX && Operation <= C166_ATOMIC_UMIN)
    Comparison = c166_atomic_compare(ObjectBytes, ValueBytes, Size,
                                     Operation < C166_ATOMIC_UMAX);

  while (Size != 0) {
    unsigned int Old = *ObjectBytes;
    unsigned int Operand = *ValueBytes++;
    unsigned int NewValue = Old;
    *ResultBytes++ = (c166_atomic_byte)Old;
    switch (Operation) {
    case C166_ATOMIC_ADD:
      NewValue = Old + Operand + Carry;
      Carry = NewValue >> 8;
      break;
    case C166_ATOMIC_SUB: {
      unsigned int Subtrahend = Operand + Carry;
      NewValue = Old - Subtrahend;
      Carry = Old < Subtrahend;
      break;
    }
    case C166_ATOMIC_AND:
      NewValue = Old & Operand;
      break;
    case C166_ATOMIC_OR:
      NewValue = Old | Operand;
      break;
    case C166_ATOMIC_XOR:
      NewValue = Old ^ Operand;
      break;
    case C166_ATOMIC_NAND:
      NewValue = ~(Old & Operand);
      break;
    case C166_ATOMIC_MAX:
    case C166_ATOMIC_UMAX:
      if (Comparison < 0)
        NewValue = Operand;
      break;
    case C166_ATOMIC_MIN:
    case C166_ATOMIC_UMIN:
      if (Comparison > 0)
        NewValue = Operand;
      break;
    default:
      break;
    }
    *ObjectBytes++ = (c166_atomic_byte)NewValue;
    --Size;
  }
  c166_atomic_leave(SavedPSW);
}

COMPILER_RT_ABI void __c166_atomic_fence(int Order) {
  (void)Order;
  // The call is the compiler barrier. C166 is in-order and has no
  // coherent second CPU requiring a hardware memory barrier.
  __asm__ volatile("" ::: "memory");
}
