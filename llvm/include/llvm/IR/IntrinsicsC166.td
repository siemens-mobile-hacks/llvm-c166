//===- IntrinsicsC166.td - C166 intrinsics -------------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "c166" in {
  // Extract the high word of a C166 32-bit pointer representation without
  // exposing the operation to target-independent pointer canonicalization.
  def int_c166_high_word : Intrinsic<[llvm_i16_ty], [llvm_i32_ty],
                                      [IntrNoMem, IntrSpeculatable,
                                       IntrWillReturn]>;

  // Add a byte offset to the 14-bit page offset of a C166 far pointer while
  // preserving its page word.
  def int_c166_far_add : Intrinsic<[llvm_i32_ty],
                                   [llvm_i32_ty, llvm_i16_ty],
                                   [IntrNoMem, IntrSpeculatable, IntrWillReturn]>;

  // Language conversions between default far data pointers and
  // long use a huge/linear value rather than the stored page:offset words.
  // Keep these operations explicit until the target's late IR lowering so
  // target-independent optimization cannot fold them as identity casts.
  def int_c166_far_to_linear : Intrinsic<[llvm_i32_ty], [llvm_anyptr_ty],
                                         [IntrNoMem, IntrSpeculatable,
                                          IntrWillReturn]>;
  def int_c166_linear_to_far : Intrinsic<[llvm_anyptr_ty], [llvm_i32_ty],
                                         [IntrNoMem, IntrSpeculatable,
                                          IntrWillReturn]>;

  // A C166 near/xnear pointer stores only a 16-bit DPP-selected address.
  // Integer conversion must incorporate the current DPP2/DPP1 page, while
  // conversion back must force the corresponding selector bits.  Keep these
  // explicit for the same reason as the far conversions above.
  def int_c166_near_to_linear : Intrinsic<[llvm_i32_ty], [llvm_anyptr_ty],
                                          [IntrNoMem, IntrSpeculatable,
                                           IntrWillReturn]>;
  def int_c166_linear_to_near : Intrinsic<[llvm_anyptr_ty], [llvm_i32_ty],
                                          [IntrNoMem, IntrSpeculatable,
                                           IntrWillReturn]>;

  // Widen a raw near/xnear pointer to the C166 far offset:page form. The
  // immediate selects DPP1 or DPP2 and the operation preserves a raw null.
  def int_c166_near_to_far : Intrinsic<[llvm_anyptr_ty],
                                       [llvm_i16_ty, llvm_i16_ty],
                                       [IntrNoMem, IntrSpeculatable,
                                        IntrWillReturn,
                                        ImmArg<ArgIndex<1>>]>;

  // Read one of the ABI-reserved data-page registers.  The selector is an
  // immediate so instruction selection can map it directly to MOVgsfr.
  def int_c166_read_dpp : Intrinsic<[llvm_i16_ty], [llvm_i16_ty],
                                    [IntrNoMem, IntrSpeculatable,
                                     IntrWillReturn, ImmArg<ArgIndex<0>>]>;
}
