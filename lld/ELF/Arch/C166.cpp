//===- C166.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "Symbols.h"
#include "Target.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MathExtras.h"

#include <algorithm>

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {

class C166 final : public TargetInfo {
public:
  explicit C166(Ctx &ctx) : TargetInfo(ctx) {
    defaultImageBase = 0;
    defaultCommonPageSize = 1;
    defaultMaxPageSize = 1;
    trapInstr = {0xcc, 0x00, 0xcc, 0x00};
  }

  uint32_t calcEFlags() const override;
  void finalizeRelax(int passes) const override;
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};

} // namespace

static uint32_t getC166EFlags(InputFile *file) {
  return cast<ObjFile<ELF32LE>>(file)->getObj().getHeader().e_flags;
}

static uint8_t invertJMPR(uint8_t opcode) {
  switch (opcode) {
  case 0x2d:
    return 0x3d;
  case 0x3d:
    return 0x2d;
  case 0x4d:
    return 0x5d;
  case 0x5d:
    return 0x4d;
  case 0x6d:
    return 0x7d;
  case 0x7d:
    return 0x6d;
  case 0x8d:
    return 0x9d;
  case 0x9d:
    return 0x8d;
  case 0xad:
    return 0xbd;
  case 0xbd:
    return 0xad;
  case 0xcd:
    return 0xdd;
  case 0xdd:
    return 0xcd;
  case 0xed:
    return 0xfd;
  case 0xfd:
    return 0xed;
  default:
    return 0;
  }
}

static uint8_t invertBitBranch(uint8_t opcode) {
  if (opcode == 0x8a)
    return 0x9a;
  if (opcode == 0x9a)
    return 0x8a;
  return 0;
}

uint32_t C166::calcEFlags() const {
  if (ctx.objectFiles.empty())
    return 0;

  uint32_t flags = getC166EFlags(ctx.objectFiles.front());
  for (InputFile *file : ArrayRef(ctx.objectFiles).drop_front()) {
    uint32_t other = getC166EFlags(file);
    if (other != flags)
      ErrAlways(ctx) << file << ": incompatible C166 e_flags 0x"
                     << utohexstr(other) << "; expected 0x" << utohexstr(flags);
  }
  return flags;
}

void C166::finalizeRelax(int passes) const {
  if (ctx.objectFiles.empty())
    return;

  uint32_t flags = getC166EFlags(ctx.objectFiles.front());

  // CP addresses internal register RAM, not an arbitrary near-data page.
  // Check input sections so an output-section rename cannot hide bad placement.
  for (InputSectionBase *sec : ctx.inputSections) {
    if (!sec->isLive() || sec->name != ".c166.regbank" || !sec->parent)
      continue;
    uint64_t start = sec->getVA();
    uint64_t size = sec->getSize();
    if ((start & 1) == 0 && start >= 0xf200 && start < 0xfe00 &&
        size <= 0xfe00 - start)
      continue;
    Err(ctx) << "C166 register bank section must be word-aligned within "
                "[0xF200, 0xFE00); use a linker script to select device IRAM";
  }

  auto CheckRange = [&](const Twine &Owner, uint64_t start, uint64_t size) {
    if (start < 0x10000 && size <= 0x10000 - start)
      return;
    Err(ctx) << Owner << ": Medium near code range [0x" << utohexstr(start)
             << ", 0x" << utohexstr(start + size)
             << ") is outside the first 64 KiB code segment";
  };

  DenseSet<Symbol *> CheckedSymbols;
  auto ForEachSymbol = [&](auto CheckSymbol) {
    for (InputFile *file : ctx.objectFiles) {
      auto *obj = cast<ObjFile<ELF32LE>>(file);
      for (Symbol *sym : obj->getLocalSymbols())
        CheckSymbol(sym);
      for (Symbol *sym : obj->getGlobalSymbols())
        CheckSymbol(sym);
    }
  };

  // IP is a 16-bit offset within the segment selected by CSP.  A huge
  // function may live in any segment, but its complete body must fit in that
  // segment; sequential execution and JMPI do not carry into the next one.
  ForEachSymbol([&](Symbol *sym) {
    if (!sym || !CheckedSymbols.insert(sym).second || sym->type != STT_FUNC)
      return;
    unsigned codeClass = sym->stOther & STO_C166_CODE_MASK;
    bool isHuge =
        codeClass == STO_C166_CODE_HUGE ||
        (codeClass == 0 && (flags & EF_C166_CODE_MASK) == EF_C166_CODE_HUGE);
    if (!isHuge)
      return;
    auto *defined = dyn_cast<Defined>(sym);
    if (!defined || !defined->section || !defined->section->isLive())
      return;
    uint64_t start = defined->getVA(ctx);
    uint64_t size = std::max<uint64_t>(defined->getSize(), 2);
    if (size <= 0x10000 - (start & 0xffff))
      return;
    Err(ctx) << "huge function '" << sym->getName() << "' range [0x"
             << utohexstr(start) << ", 0x" << utohexstr(start + size)
             << ") crosses a 64 KiB code segment boundary";
  });
  CheckedSymbols.clear();

  if ((flags & EF_C166_CODE_MASK) == EF_C166_CODE_NEAR) {
    // In the Medium model every ordinary function is near and must reside in
    // the first 64 KiB code segment.  Check input-section identity rather than
    // the output-section name so linker scripts cannot accidentally evade the
    // rule by renaming .c166.near.text.  Explicit huge functions live in the
    // ordinary .text class and are deliberately unrestricted here.
    for (InputSectionBase *sec : ctx.inputSections) {
      if (!sec->isLive() || sec->name != ".c166.near.text" || !sec->parent)
        continue;
      CheckRange(toStr(ctx, sec), sec->getVA(), sec->getSize());
    }

    // A source-level section attribute keeps its requested section name.  The
    // C166 processor-specific st_other marker carries the near/huge
    // distinction in that case and survives a textual assembly round trip.
    ForEachSymbol([&](Symbol *sym) {
      if (!sym || !CheckedSymbols.insert(sym).second || sym->type != STT_FUNC ||
          (sym->stOther & STO_C166_CODE_MASK) != STO_C166_CODE_NEAR)
        return;
      auto *defined = dyn_cast<Defined>(sym);
      if (!defined || !defined->section || !defined->section->isLive() ||
          defined->section->name == ".c166.near.text")
        return;
      CheckRange(Twine("near function '") + sym->getName() + "'",
                 defined->getVA(ctx),
                 std::max<uint64_t>(defined->getSize(), 2));
    });
  }

  if ((flags & EF_C166_DATA_MASK) != EF_C166_DATA_NEAR)
    return;

  auto CheckDataRange = [&](const Twine &Owner, StringRef Class, uint64_t start,
                            uint64_t size, uint64_t boundary,
                            uint64_t addressLimit) {
    if (start < addressLimit && size <= addressLimit - start &&
        (start % boundary) + size <= boundary)
      return;
    Err(ctx) << Owner << ": Small " << Class << " data range [0x"
             << utohexstr(start) << ", 0x" << utohexstr(start + size)
             << ") violates its " << boundary / 1024 << " KiB placement";
  };

  // The Small model uses the default linear LDAT map:
  // DPP0..DPP3 select pages 0..3.  Keep canonical normal data, including
  // constant pools without an STT_OBJECT symbol, inside that direct 64-KiB
  // window.  Explicitly qualified objects use separate sections below.
  for (InputSectionBase *sec : ctx.inputSections) {
    if (!sec->isLive() || !sec->parent ||
        !sec->name.starts_with(".c166.small.") ||
        sec->name.starts_with(".c166.small.far.") ||
        sec->name.starts_with(".c166.small.huge.") ||
        sec->name.starts_with(".c166.small.shuge."))
      continue;
    CheckDataRange(toStr(ctx, sec), "normal", sec->getVA(), sec->getSize(),
                   0x10000, 0x10000);
  }

  CheckedSymbols.clear();
  ForEachSymbol([&](Symbol *sym) {
    if (!sym || !CheckedSymbols.insert(sym).second || sym->type != STT_OBJECT)
      return;
    auto *defined = dyn_cast<Defined>(sym);
    if (!defined || !defined->section || !defined->section->isLive())
      return;
    uint64_t start = defined->getVA(ctx);
    uint64_t size = std::max<uint64_t>(defined->getSize(), 1);
    switch (sym->stOther & STO_C166_DATA_MASK) {
    case STO_C166_DATA_NEAR:
      if (!defined->section->name.starts_with(".c166.small.") ||
          defined->section->name.starts_with(".c166.small.far.") ||
          defined->section->name.starts_with(".c166.small.huge.") ||
          defined->section->name.starts_with(".c166.small.shuge."))
        CheckDataRange(Twine("data symbol '") + sym->getName() + "'", "normal",
                       start, size, 0x10000, 0x10000);
      break;
    case STO_C166_DATA_FAR:
      CheckDataRange(Twine("data symbol '") + sym->getName() + "'", "far",
                     start, size, 0x4000, 0x1000000);
      break;
    case STO_C166_DATA_HUGE:
      CheckDataRange(Twine("data symbol '") + sym->getName() + "'", "huge",
                     start, size, 0x1000000, 0x1000000);
      break;
    case STO_C166_DATA_SHUGE:
      CheckDataRange(Twine("data symbol '") + sym->getName() + "'", "shuge",
                     start, size, 0x10000, 0x1000000);
      break;
    default:
      break;
    }
  });
}

RelExpr C166::getRelExpr(RelType type, const Symbol &s,
                         const uint8_t *loc) const {
  switch (type) {
  case R_C166_NONE:
    return R_NONE;
  case R_C166_PC8:
  case R_C166_BIT_PC8:
  case R_C166_PC8_RELAX:
  case R_C166_PC16:
  case R_C166_COF16:
    return R_PC;
  case R_C166_8:
  case R_C166_16:
  case R_C166_32:
  case R_C166_PAGED32:
  case R_C166_SEG8:
  case R_C166_SEG24:
  case R_C166_SOF16:
  case R_C166_PAG10:
  case R_C166_POF14:
  case R_C166_DPP1_16:
  case R_C166_DPP2_16:
    return R_ABS;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unknown relocation (" << type.v
             << ") against symbol " << &s;
    return R_NONE;
  }
}

void C166::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  if (rel.type == R_C166_PC8 || rel.type == R_C166_BIT_PC8) {
    uint64_t target = rel.sym->getVA(ctx, rel.addend);
    unsigned offset = rel.type == R_C166_PC8 ? 1 : 2;
    uint64_t instruction = target - val - offset;
    checkUInt(ctx, loc, target, 24, rel);
    if ((target >> 16) != (instruction >> 16)) {
      Err(ctx) << getErrorLoc(ctx, loc)
               << "relative code relocation crosses a 64 KiB code boundary";
      return;
    }
  }
  switch (rel.type) {
  case R_C166_NONE:
    return;
  case R_C166_COF16: {
    // Undefined weak symbols have the ELF value zero.  Keep their addend
    // usable, just as for the other absolute and PC-relative C166
    // relocations; unresolved strong symbols have already been diagnosed by
    // the generic relocation scan before reaching this point.
    uint64_t target = rel.sym->getVA(ctx, rel.addend);
    // R_PC supplies S + A - P.  The relocation field begins two bytes
    // after the CALLA/JMPA opcode, so reconstruct the instruction address
    // before checking the architectural 64 KiB code segment.
    uint64_t place = target - val;
    uint64_t instruction = place - 2;
    checkUInt(ctx, loc, target, 24, rel);
    if ((target >> 16) != (instruction >> 16)) {
      Err(ctx) << getErrorLoc(ctx, loc) << "same-segment code relocation to '"
               << rel.sym->getName() << "' crosses a 64 KiB code boundary";
      return;
    }
    write16le(loc, target & 0xffff);
  }
    return;
  case R_C166_8:
    checkIntUInt(ctx, loc, val, 8, rel);
    *loc = val;
    return;
  case R_C166_16:
    checkIntUInt(ctx, loc, val, 16, rel);
    write16le(loc, val);
    return;
  case R_C166_32:
    checkIntUInt(ctx, loc, val, 32, rel);
    write32le(loc, val);
    return;
  case R_C166_PAGED32:
    checkUInt(ctx, loc, val, 24, rel);
    write16le(loc, val & 0x3fff);
    write16le(loc + 2, (val >> 14) & 0x03ff);
    return;
  case R_C166_SEG8:
    checkUInt(ctx, loc, val, 24, rel);
    *loc = (val >> 16) & 0xff;
    return;
  case R_C166_SEG24:
    checkUInt(ctx, loc, val, 24, rel);
    *loc = (val >> 16) & 0xff;
    write16le(loc + 1, val & 0xffff);
    return;
  case R_C166_SOF16:
    checkUInt(ctx, loc, val, 24, rel);
    write16le(loc, val & 0xffff);
    return;
  case R_C166_PAG10:
    checkUInt(ctx, loc, val, 24, rel);
    write16le(loc, (read16le(loc) & 0xfc00) | ((val >> 14) & 0x03ff));
    return;
  case R_C166_POF14:
    checkUInt(ctx, loc, val, 24, rel);
    if (rel.sym && rel.sym->isDefined() && rel.sym->getSize() != 0) {
      uint64_t start = rel.sym->getVA(ctx);
      uint64_t size = rel.sym->getSize();
      if ((start & 0x3fff) + size > 0x4000)
        Err(ctx) << getErrorLoc(ctx, loc) << "paged data symbol '"
                 << rel.sym->getName() << "' of size " << size
                 << " crosses a 16 KiB page boundary";
    }
    write16le(loc, (read16le(loc) & 0xc000) | (val & 0x3fff));
    return;
  case R_C166_DPP1_16:
  case R_C166_DPP2_16:
    checkUInt(ctx, loc, val, 24, rel);
    if (rel.sym && rel.sym->isDefined() && rel.sym->getSize() != 0) {
      uint64_t start = rel.sym->getVA(ctx);
      uint64_t size = rel.sym->getSize();
      if ((start & 0x3fff) + size > 0x4000)
        Err(ctx) << getErrorLoc(ctx, loc) << "near data symbol '"
                 << rel.sym->getName() << "' of size " << size
                 << " crosses a 16 KiB DPP page boundary";
    }
    write16le(loc,
              (rel.type == R_C166_DPP1_16 ? 0x4000 : 0x8000) | (val & 0x3fff));
    return;
  case R_C166_PC8: {
    // IP wraps at 16 bits; the segment was checked before selecting the delta.
    int64_t delta = SignExtend64<16>(val - 1);
    if (delta & 1) {
      Err(ctx) << getErrorLoc(ctx, loc)
               << "R_C166_PC8 target is not word-aligned";
      return;
    }
    int64_t words = delta / 2;
    checkInt(ctx, loc, words, 8, rel);
    *loc = words;
  }
    return;
  case R_C166_BIT_PC8: {
    int64_t delta = SignExtend64<16>(val - 2);
    if (delta & 1) {
      Err(ctx) << getErrorLoc(ctx, loc)
               << "R_C166_BIT_PC8 target is not word-aligned";
      return;
    }
    int64_t words = delta / 2;
    checkInt(ctx, loc, words, 8, rel);
    *loc = words;
  }
    return;
  case R_C166_PC8_RELAX: {
    uint8_t opcode = loc[0];
    uint8_t inverseJMPR = invertJMPR(opcode);
    uint8_t inverseBit = invertBitBranch(opcode);
    bool isNET = opcode == 0x1d;
    bool bitWriteback = opcode == 0xaa || opcode == 0xba;
    if (opcode != 0x0d && !isNET && !bitWriteback && inverseJMPR == 0 &&
        inverseBit == 0) {
      Err(ctx) << getErrorLoc(ctx, loc)
               << "R_C166_PC8_RELAX does not refer to a relative branch";
      return;
    }

    bool isBitBranch = inverseBit != 0 || bitWriteback;
    uint64_t target = rel.sym->getVA(ctx, rel.addend);
    uint64_t instruction = target - val;
    checkUInt(ctx, loc, target, 24, rel);
    int64_t delta = SignExtend64<16>(val - (isBitBranch ? 4 : 2));
    if (delta & 1) {
      Err(ctx) << getErrorLoc(ctx, loc)
               << "R_C166_PC8_RELAX target is not word-aligned";
      return;
    }
    int64_t words = delta / 2;
    if ((target >> 16) == (instruction >> 16) && isInt<8>(words)) {
      loc[isBitBranch ? 2 : 1] = words;
      return;
    }

    // The replacement is fetched using the current CSP throughout.
    unsigned size = bitWriteback             ? 10
                    : (isBitBranch || isNET) ? 8
                    : opcode == 0x0d         ? 4
                                             : 6;
    if ((instruction & 0xffff) + size > 0x10000) {
      Err(ctx) << getErrorLoc(ctx, loc)
               << "relative branch replacement crosses a 64 KiB code boundary";
      return;
    }

    if (bitWriteback) {
      // Preserve the original test, bit writeback and flags. Its taken path
      // reaches JMPS; the other path skips it using a flag-neutral JMPR.
      loc[2] = 1;
      loc[4] = 0x0d;
      loc[5] = 2;
      loc[6] = 0xfa;
      loc[7] = (target >> 16) & 0xff;
      write16le(loc + 8, target & 0xffff);
      return;
    }
    if (isBitBranch || isNET) {
      if (isNET) {
        // NET true reaches JMPS; false skips it without changing PSW.
        loc[1] = 1;
        loc[2] = 0x0d;
        loc[3] = 2;
      } else {
        loc[0] = inverseBit;
        loc[2] = 2;
      }
      loc[4] = 0xfa;
      loc[5] = (target >> 16) & 0xff;
      write16le(loc + 6, target & 0xffff);
      return;
    }
    if (opcode == 0x0d) {
      loc[0] = 0xfa;
      loc[1] = (target >> 16) & 0xff;
      write16le(loc + 2, target & 0xffff);
      return;
    }

    // Invert the condition to skip the four-byte segmented jump when the
    // original branch is not taken.
    loc[0] = inverseJMPR;
    loc[1] = 2;
    loc[2] = 0xfa;
    loc[3] = (target >> 16) & 0xff;
    write16le(loc + 4, target & 0xffff);
  }
    return;
  case R_C166_PC16:
    checkInt(ctx, loc, val, 16, rel);
    write16le(loc, val);
    return;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation " << rel.type;
  }
}

void elf::setC166TargetInfo(Ctx &ctx) { ctx.target.reset(new C166(ctx)); }
