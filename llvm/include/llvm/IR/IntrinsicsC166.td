//===- IntrinsicsC166.td - C166 intrinsics -------------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

def llvm_c166_near_ptr_ty : LLVMQualPointerType<3>;

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

  // Execute the architectural unsigned 32-by-16 division. The low result
  // word is the quotient and the high result word is the remainder.
  def int_c166_divlu : Intrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i16_ty],
                                 [IntrNoMem, IntrWillReturn]>;

  // Return the address of the first unnamed user-stack argument.  Keeping the
  // address as an SSA value lets ordinary optimization promote a va_list
  // instead of forcing every va_arg update through a stack slot.
  def int_c166_va_start : Intrinsic<[llvm_anyptr_ty], [],
                                    [IntrNoMem, IntrWillReturn]>;

  // Recover the raw direct address of an object on the C166 user stack.  This
  // is intentionally narrower than an address-space cast: it is valid only
  // for frame objects and fixed stack arguments selected by the backend.
  def int_c166_stack_address : Intrinsic<[llvm_i16_ty],
                                         [llvm_anyptr_ty],
                                         [IntrNoMem, IntrSpeculatable,
                                          IntrWillReturn]>;

  // Preserve the ABI's floating-point word order through target-independent
  // optimization.  The C166 IR passes replace these with ordinary integer
  // memory operations immediately before instruction selection.
  def int_c166_float_load : Intrinsic<[llvm_anyfloat_ty],
                                      [llvm_anyptr_ty, llvm_i32_ty],
                                      [IntrArgMemOnly, IntrReadMem,
                                       ReadOnly<ArgIndex<0>>,
                                       NoCapture<ArgIndex<0>>,
                                       ImmArg<ArgIndex<1>>, IntrWillReturn]>;
  def int_c166_float_store : Intrinsic<[],
                                       [llvm_anyfloat_ty, llvm_anyptr_ty,
                                        llvm_i32_ty],
                                       [IntrArgMemOnly, IntrWriteMem,
                                        WriteOnly<ArgIndex<1>>,
                                        NoCapture<ArgIndex<1>>,
                                        ImmArg<ArgIndex<2>>, IntrWillReturn]>;
}
