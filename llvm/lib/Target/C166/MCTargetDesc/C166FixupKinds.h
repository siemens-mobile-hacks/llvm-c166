//===-- C166FixupKinds.h - C166 specific fixups ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_C166_MCTARGETDESC_C166FIXUPKINDS_H
#define LLVM_LIB_TARGET_C166_MCTARGETDESC_C166FIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm::C166 {

enum Fixups {
  fixup_c166_seg8 = FirstTargetFixupKind,
  fixup_c166_seg24,
  fixup_c166_sof16,
  fixup_c166_cof16,
  fixup_c166_pag10,
  fixup_c166_pof14,
  fixup_c166_pc8,
  fixup_c166_bit_pc8,
  fixup_c166_pc8_relax,
  fixup_c166_pc16,
  fixup_c166_dpp1_16,
  fixup_c166_dpp2_16,
  fixup_c166_address16,
  fixup_c166_bit_offset,
  fixup_c166_bit_set,
  fixup_c166_bit_src,
  fixup_c166_bit_dst,
  fixup_c166_bit_low4,
  fixup_c166_bit_high4,

  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};

} // namespace llvm::C166

#endif
