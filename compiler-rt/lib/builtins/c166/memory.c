//===-- memory.c - C166 freestanding memory builtins ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The C166 compiler-rt build is freestanding, so libc cannot be assumed to
// provide the memory calls emitted for non-inlined aggregate operations.
// `size_t` is the target's 16-bit unsigned integer type while data pointers
// are 32-bit far pointers in the Large model.
//
//===----------------------------------------------------------------------===//

#include "../int_lib.h"

typedef __SIZE_TYPE__ c166_size_t;

COMPILER_RT_ABI void *memcpy(void *destination, const void *source,
                             c166_size_t count) {
  unsigned char *dst = (unsigned char *)destination;
  const unsigned char *src = (const unsigned char *)source;

  for (c166_size_t index = 0; index != count; ++index)
    dst[index] = src[index];
  return destination;
}

COMPILER_RT_ABI void *memmove(void *destination, const void *source,
                              c166_size_t count) {
  unsigned char *dst = (unsigned char *)destination;
  const unsigned char *src = (const unsigned char *)source;

  if ((__UINTPTR_TYPE__)dst < (__UINTPTR_TYPE__)src) {
    for (c166_size_t index = 0; index != count; ++index)
      dst[index] = src[index];
  } else if (dst != src) {
    while (count != 0) {
      --count;
      dst[count] = src[count];
    }
  }
  return destination;
}

COMPILER_RT_ABI void *memset(void *destination, int value, c166_size_t count) {
  unsigned char *dst = (unsigned char *)destination;

  for (c166_size_t index = 0; index != count; ++index)
    dst[index] = (unsigned char)value;
  return destination;
}
