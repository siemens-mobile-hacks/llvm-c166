//===-- C166AsmParser.cpp - Parse C166 assembly --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/C166MCAsmInfo.h"
#include "MCTargetDesc/C166MCTargetDesc.h"
#include "MCTargetDesc/C166TargetStreamer.h"
#include "TargetInfo/C166TargetInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MathExtras.h"

#include <optional>

using namespace llvm;

namespace {

static C166::Specifier getC166Specifier(StringRef Name) {
  if (Name.equals_insensitive("seg"))
    return C166::S_SEG;
  if (Name.equals_insensitive("sof"))
    return C166::S_SOF;
  if (Name.equals_insensitive("cof"))
    return C166::S_COF;
  if (Name.equals_insensitive("pag"))
    return C166::S_PAG;
  if (Name.equals_insensitive("pof"))
    return C166::S_POF;
  if (Name.equals_insensitive("dpp1"))
    return C166::S_DPP1;
  if (Name.equals_insensitive("dpp2"))
    return C166::S_DPP2;
  if (Name.equals_insensitive("paged"))
    return C166::S_PAGED32;
  return C166::S_None;
}

class C166Operand : public MCParsedAsmOperand {
  enum KindTy { Token, Register, Immediate } Kind;

  std::string TokenValue;
  MCRegister Reg;
  const MCExpr *Expr = nullptr;
  bool HasHash = false;
  SMLoc Start;
  SMLoc End;

  C166Operand(KindTy Kind, SMLoc Start, SMLoc End)
      : Kind(Kind), Start(Start), End(End) {}

public:
  static std::unique_ptr<C166Operand> createToken(StringRef Value, SMLoc Loc) {
    auto Op = std::unique_ptr<C166Operand>(new C166Operand(Token, Loc, Loc));
    Op->TokenValue = Value;
    return Op;
  }

  static std::unique_ptr<C166Operand> createRegister(MCRegister Reg,
                                                     SMLoc Start, SMLoc End) {
    auto Op =
        std::unique_ptr<C166Operand>(new C166Operand(Register, Start, End));
    Op->Reg = Reg;
    return Op;
  }

  static std::unique_ptr<C166Operand> createImmediate(const MCExpr *Expr,
                                                      SMLoc Start, SMLoc End,
                                                      bool HasHash = false) {
    auto Op =
        std::unique_ptr<C166Operand>(new C166Operand(Immediate, Start, End));
    Op->Expr = Expr;
    Op->HasHash = HasHash;
    return Op;
  }

  bool isToken() const override { return Kind == Token; }
  bool isReg() const override { return Kind == Register; }
  bool isImm() const override { return Kind == Immediate; }
  bool isMem() const override { return false; }

  StringRef getToken() const {
    assert(isToken());
    return TokenValue;
  }

  MCRegister getReg() const override {
    assert(isReg());
    return Reg;
  }

  bool isUImm(unsigned Bits) const {
    int64_t Value;
    return isImm() && Expr->evaluateAsAbsolute(Value) && Value >= 0 &&
           isUIntN(Bits, static_cast<uint64_t>(Value));
  }

  bool isUImm3() const { return isUImm(3); }
  bool isUImm4() const { return isUImm(4); }
  bool isUImm8() const { return isUImm(8); }
  bool isUImm16() const {
    if (!isImm())
      return false;
    int64_t Value;
    return !Expr->evaluateAsAbsolute(Value) ||
           (Value >= 0 && isUInt<16>(static_cast<uint64_t>(Value)));
  }
  bool isUImm16Large() const {
    if (!isImm() || !HasHash)
      return false;
    int64_t Value;
    // MOV reg,#data16 is the materialization form for every relocatable
    // 16-bit C166 address component.  Symbolic and modifier expressions can
    // only use this form; the compact four-bit MOV is for absolute 0..15.
    return !Expr->evaluateAsAbsolute(Value) ||
           (Value >= 16 && isUInt<16>(static_cast<uint64_t>(Value)));
  }
  bool isUImm16ALU() const {
    if (!isImm() || !HasHash)
      return false;
    int64_t Value;
    return !Expr->evaluateAsAbsolute(Value) ||
           (Value >= 8 && isUInt<16>(static_cast<uint64_t>(Value)));
  }
  bool isSequenceCount() const {
    int64_t Value;
    return isImm() && Expr->evaluateAsAbsolute(Value) && Value >= 1 &&
           Value <= 4;
  }
  bool isAtomicCount() const {
    int64_t Value;
    return isImm() && Expr->evaluateAsAbsolute(Value) && Value >= 1 &&
           Value <= 4;
  }
  bool isBitAddress() const { return isUImm(12); }

  bool isSpecifier(C166::Specifier Specifier) const {
    const auto *SpecifierExpr =
        isImm() ? dyn_cast<MCSpecifierExpr>(Expr) : nullptr;
    return SpecifierExpr && SpecifierExpr->getSpecifier() == Specifier;
  }

  bool isSeg8() const { return isUImm8() || isSpecifier(C166::S_SEG); }
  bool isSof16() const { return isUImm16() || isSpecifier(C166::S_SOF); }
  bool isCof16() const { return isUImm16() || isSpecifier(C166::S_COF); }
  bool isPag10() const { return isUImm(10) || isSpecifier(C166::S_PAG); }
  bool isPof14() const {
    return !HasHash && (isUImm(14) || isSpecifier(C166::S_POF));
  }
  bool isAbs16() const {
    if (!isImm() || HasHash)
      return false;
    // This operand exists for relocatable named register-bank storage.  Keep
    // numeric direct memory operands and explicit DPP wrappers on their
    // established POF14/DPP matcher paths, both for correct selector bits and
    // for their precise range diagnostics.
    int64_t Value;
    if (Expr->evaluateAsAbsolute(Value) || isa<MCSpecifierExpr>(Expr))
      return false;
    if (const auto *Symbol = dyn_cast<MCSymbolRefExpr>(Expr)) {
      StringRef Name = Symbol->getSymbol().getName();
      if (Name.size() > 1 && (Name.front() == 'r' || Name.front() == 'R')) {
        bool RegisterLike = true;
        for (char C : Name.drop_front())
          RegisterLike &= C >= '0' && C <= '9';
        if (RegisterLike)
          return false;
      }
    }
    return true;
  }
  bool isDPP1Address() const { return !HasHash && isSpecifier(C166::S_DPP1); }
  bool isDPP2Address() const { return !HasHash && isSpecifier(C166::S_DPP2); }
  bool isBrTarget() const {
    if (!isImm())
      return false;
    int64_t Value;
    // A symbolic target is range-checked after layout by the PC8 fixup.  A
    // numeric operand is the raw encoded displacement printed by the
    // disassembler and must fit the field instead of being silently truncated.
    return !Expr->evaluateAsAbsolute(Value) ||
           (Value >= 0 && isUInt<8>(static_cast<uint64_t>(Value)));
  }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && isReg());
    Inst.addOperand(MCOperand::createReg(Reg));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && isImm());
    int64_t Value;
    if (Expr->evaluateAsAbsolute(Value))
      Inst.addOperand(MCOperand::createImm(Value));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  SMLoc getStartLoc() const override { return Start; }
  SMLoc getEndLoc() const override { return End; }

  void print(raw_ostream &OS, const MCAsmInfo &MAI) const override {
    switch (Kind) {
    case Token:
      OS << "token " << TokenValue;
      break;
    case Register:
      OS << "register " << Reg.id();
      break;
    case Immediate:
      OS << "immediate ";
      MAI.printExpr(OS, *Expr);
      break;
    }
  }
};

class C166AsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;
  std::optional<CodeModel::Model> MemoryModel;
  DenseMap<const MCSymbol *, bool> FunctionClasses;
  DenseMap<const MCSymbol *, C166DataClass> DataClasses;

  AsmLexer &getLexer() const { return Parser.getLexer(); }

  bool parseOperand(OperandVector &Operands);

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;

  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool parsePrimaryExpr(const MCExpr *&Res, SMLoc &EndLoc) override;

  ParseStatus parseDirective(AsmToken DirectiveID) override;

public:
  enum C166MatchResultTy {
    Match_InvalidUImm3 = FIRST_TARGET_MATCH_RESULT_TY,
    Match_InvalidUImm4,
    Match_InvalidUImm8,
    Match_InvalidUImm16,
    Match_InvalidUImm16Large,
    Match_InvalidUImm16ALU,
    Match_InvalidSequenceCount,
    Match_InvalidAtomicCount,
    Match_InvalidBitAddress,
    Match_InvalidSeg8,
    Match_InvalidSof16,
    Match_InvalidCof16,
    Match_InvalidPag10,
    Match_InvalidPof14,
    Match_InvalidAbs16,
    Match_InvalidBrTarget,
  };

private:
#define GET_ASSEMBLER_HEADER
#include "C166GenAsmMatcher.inc"

public:
  C166AsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                const MCInstrInfo &MII)
      : MCTargetAsmParser(STI, MII), Parser(Parser) {
    MCAsmParserExtension::Initialize(Parser);
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
    StringRef Model = Parser.getContext().getTargetOptions().getABIName();
    if (Model == "large" || Model == "medium" || Model == "small") {
      MemoryModel = Model == "small"    ? CodeModel::Small
                    : Model == "medium" ? CodeModel::Medium
                                        : CodeModel::Large;
      auto *TS = static_cast<C166TargetStreamer *>(
          Parser.getStreamer().getTargetStreamer());
      if (TS)
        TS->emitMemoryModel(*MemoryModel);
    }
  }
};

} // namespace

bool C166AsmParser::parsePrimaryExpr(const MCExpr *&Res, SMLoc &EndLoc) {
  if (!getLexer().is(AsmToken::Identifier) ||
      !getLexer().peekTok().is(AsmToken::LParen))
    return MCTargetAsmParser::parsePrimaryExpr(Res, EndLoc);

  C166::Specifier Specifier =
      getC166Specifier(getLexer().getTok().getIdentifier());
  if (Specifier == C166::S_None)
    return MCTargetAsmParser::parsePrimaryExpr(Res, EndLoc);

  Parser.Lex();
  if (Parser.parseToken(AsmToken::LParen, "expected '(' after modifier") ||
      Parser.parseExpression(Res) ||
      Parser.parseToken(AsmToken::RParen, "expected ')' after expression"))
    return true;
  Res = MCSpecifierExpr::create(Res, Specifier, Parser.getContext());
  EndLoc = getLexer().getLoc();
  return false;
}

ParseStatus C166AsmParser::parseDirective(AsmToken DirectiveID) {
  StringRef Directive = DirectiveID.getIdentifier();
  if (Directive == ".c166_data") {
    SMLoc Loc = getLexer().getLoc();
    if (getLexer().isNot(AsmToken::Identifier))
      return Error(Loc, "expected C166 data class after .c166_data");
    StringRef Name = getLexer().getTok().getIdentifier();
    C166DataClass Class;
    if (Name.equals_insensitive("near"))
      Class = C166DataClass::Near;
    else if (Name.equals_insensitive("xnear"))
      Class = C166DataClass::XNear;
    else if (Name.equals_insensitive("far"))
      Class = C166DataClass::Far;
    else if (Name.equals_insensitive("huge"))
      Class = C166DataClass::Huge;
    else if (Name.equals_insensitive("shuge"))
      Class = C166DataClass::SHuge;
    else
      return Error(Loc, "unsupported C166 data class '" + Name + "'");
    Parser.Lex();
    if (Parser.parseToken(AsmToken::Comma,
                          "expected ',' after C166 data class") ||
        getLexer().isNot(AsmToken::Identifier))
      return Error(getLexer().getLoc(), "expected data symbol");
    MCSymbol *Symbol = Parser.getContext().getOrCreateSymbol(
        getLexer().getTok().getIdentifier());
    Parser.Lex();
    if (getLexer().isNot(AsmToken::EndOfStatement))
      return Error(getLexer().getLoc(), "unexpected token in .c166_data");
    Parser.Lex();
    auto [It, Inserted] = DataClasses.try_emplace(Symbol, Class);
    if (!Inserted && It->second != Class)
      return Error(Loc, "conflicting C166 data class directives for '" +
                            Symbol->getName() + "'");
    auto *TS = static_cast<C166TargetStreamer *>(
        Parser.getStreamer().getTargetStreamer());
    if (!TS)
      return Error(Loc, "C166 target streamer is not registered");
    TS->emitDataClass(*Symbol, Class);
    return ParseStatus::Success;
  }

  if (Directive == ".c166_function") {
    SMLoc Loc = getLexer().getLoc();
    if (getLexer().isNot(AsmToken::Identifier))
      return Error(Loc, "expected 'near' or 'huge' after .c166_function");
    StringRef Class = getLexer().getTok().getIdentifier();
    bool IsNear;
    if (Class.equals_insensitive("near"))
      IsNear = true;
    else if (Class.equals_insensitive("huge"))
      IsNear = false;
    else
      return Error(Loc, "unsupported C166 function class '" + Class + "'");
    Parser.Lex();
    if (Parser.parseToken(AsmToken::Comma,
                          "expected ',' after C166 function class") ||
        getLexer().isNot(AsmToken::Identifier))
      return Error(getLexer().getLoc(), "expected function symbol");
    MCSymbol *Symbol = Parser.getContext().getOrCreateSymbol(
        getLexer().getTok().getIdentifier());
    Parser.Lex();
    if (getLexer().isNot(AsmToken::EndOfStatement))
      return Error(getLexer().getLoc(), "unexpected token in .c166_function");
    Parser.Lex();
    auto [It, Inserted] = FunctionClasses.try_emplace(Symbol, IsNear);
    if (!Inserted && It->second != IsNear)
      return Error(Loc, "conflicting C166 function class directives for '" +
                            Symbol->getName() + "'");
    auto *TS = static_cast<C166TargetStreamer *>(
        Parser.getStreamer().getTargetStreamer());
    if (!TS)
      return Error(Loc, "C166 target streamer is not registered");
    TS->emitFunctionClass(*Symbol, IsNear);
    return ParseStatus::Success;
  }

  if (Directive != ".c166_model")
    return ParseStatus::NoMatch;

  SMLoc Loc = getLexer().getLoc();
  if (getLexer().isNot(AsmToken::Identifier))
    return Error(Loc,
                 "expected 'small', 'medium', or 'large' after .c166_model");

  StringRef Model = getLexer().getTok().getIdentifier();
  CodeModel::Model NewModel;
  if (Model.equals_insensitive("small"))
    NewModel = CodeModel::Small;
  else if (Model.equals_insensitive("medium"))
    NewModel = CodeModel::Medium;
  else if (Model.equals_insensitive("large"))
    NewModel = CodeModel::Large;
  else
    return Error(Loc, "unsupported C166 memory model '" + Model + "'");
  Parser.Lex();

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(getLexer().getLoc(), "unexpected token in .c166_model");
  Parser.Lex();

  if (MemoryModel && *MemoryModel != NewModel)
    return Error(Loc, "conflicting C166 memory model directives");
  MemoryModel = NewModel;

  auto *TS = static_cast<C166TargetStreamer *>(
      Parser.getStreamer().getTargetStreamer());
  if (!TS)
    return Error(Loc, "C166 target streamer is not registered");
  TS->emitMemoryModel(NewModel);
  return ParseStatus::Success;
}

static MCRegister MatchRegisterName(StringRef Name);

static std::optional<unsigned> getBitWordAddress(MCRegister Reg) {
  switch (Reg.id()) {
  case C166::R0:
    return 0xf0;
  case C166::R1:
    return 0xf1;
  case C166::R2:
    return 0xf2;
  case C166::R3:
    return 0xf3;
  case C166::R4:
    return 0xf4;
  case C166::R5:
    return 0xf5;
  case C166::R6:
    return 0xf6;
  case C166::R7:
    return 0xf7;
  case C166::R8:
    return 0xf8;
  case C166::R9:
    return 0xf9;
  case C166::R10:
    return 0xfa;
  case C166::R11:
    return 0xfb;
  case C166::R12:
    return 0xfc;
  case C166::R13:
    return 0xfd;
  case C166::R14:
    return 0xfe;
  case C166::R15:
    return 0xff;
  case C166::DPP0:
    return 0x00;
  case C166::DPP1:
    return 0x01;
  case C166::DPP2:
    return 0x02;
  case C166::DPP3:
    return 0x03;
  case C166::CSP:
    return 0x04;
  case C166::MDH:
    return 0x06;
  case C166::MDL:
    return 0x07;
  case C166::CP:
    return 0x08;
  case C166::SP:
    return 0x09;
  case C166::STKOV:
    return 0x0a;
  case C166::STKUN:
    return 0x0b;
  case C166::MDC:
    return 0x87;
  case C166::PSW:
    return 0x88;
  default:
    return std::nullopt;
  }
}

ParseStatus C166AsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                            SMLoc &EndLoc) {
  if (!getLexer().is(AsmToken::Identifier))
    return ParseStatus::NoMatch;

  StringRef Name = getLexer().getTok().getIdentifier();
  Reg = MatchRegisterName(Name.lower());
  if (!Reg)
    return ParseStatus::NoMatch;

  StartLoc = getLexer().getLoc();
  EndLoc = getLexer().getTok().getEndLoc();
  getLexer().Lex();
  return ParseStatus::Success;
}

bool C166AsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                  SMLoc &EndLoc) {
  ParseStatus Status = tryParseRegister(Reg, StartLoc, EndLoc);
  if (Status.isSuccess())
    return false;
  return Error(getLexer().getLoc(), "invalid register name");
}

bool C166AsmParser::parseOperand(OperandVector &Operands) {
  if (getLexer().is(AsmToken::LBrac)) {
    SMLoc Loc = getLexer().getLoc();
    Operands.push_back(C166Operand::createToken("[", Loc));
    Parser.Lex();

    if (getLexer().is(AsmToken::Minus)) {
      Loc = getLexer().getLoc();
      Operands.push_back(C166Operand::createToken("-", Loc));
      Parser.Lex();
    }

    MCRegister Base;
    SMLoc Start;
    SMLoc End;
    if (parseRegister(Base, Start, End))
      return true;
    Operands.push_back(C166Operand::createRegister(Base, Start, End));

    if (getLexer().is(AsmToken::Plus)) {
      Loc = getLexer().getLoc();
      Operands.push_back(C166Operand::createToken("+", Loc));
      Parser.Lex();

      // A trailing plus denotes the architectural post-increment form.
      // Otherwise the plus introduces an indexed displacement.
      if (getLexer().isNot(AsmToken::RBrac)) {
        Start = getLexer().getLoc();
        (void)parseOptionalToken(AsmToken::Hash);
        const MCExpr *Disp;
        if (Parser.parseExpression(Disp))
          return Error(Start, "expected displacement expression");
        End = getLexer().getLoc();
        Operands.push_back(C166Operand::createImmediate(Disp, Start, End));
      }
    }

    Loc = getLexer().getLoc();
    if (Parser.parseToken(AsmToken::RBrac, "expected ']'"))
      return true;
    Operands.push_back(C166Operand::createToken("]", Loc));
    return false;
  }

  // MC's lexer keeps the canonical `psw.11` spelling in one identifier
  // token.  Split it here before the ordinary register/expression paths.
  if (getLexer().is(AsmToken::Identifier)) {
    StringRef Spelling = getLexer().getTok().getIdentifier();
    auto [Base, BitSpelling] = Spelling.rsplit('.');
    uint64_t Bit;
    MCRegister BitReg = MatchRegisterName(Base.lower());
    std::optional<unsigned> Address = getBitWordAddress(BitReg);
    if (!Base.empty() && Address && !BitSpelling.empty() &&
        !BitSpelling.getAsInteger(10, Bit)) {
      SMLoc Start = getLexer().getLoc();
      SMLoc End = getLexer().getTok().getEndLoc();
      if (Bit > 15)
        return Error(Start, "bit number must be in the range 0..15");
      Parser.Lex();
      const MCExpr *Packed =
          MCConstantExpr::create((*Address << 4) | Bit, Parser.getContext());
      Operands.push_back(C166Operand::createImmediate(Packed, Start, End));
      return false;
    }
  }

  // Direct bit operands use the architectural `word-address.bit` spelling.
  // Named GPRs occupy F0h..FFh while named SFRs use their short address.
  if ((getLexer().is(AsmToken::Identifier) ||
       getLexer().is(AsmToken::Integer)) &&
      getLexer().peekTok().is(AsmToken::Dot)) {
    SMLoc Start = getLexer().getLoc();
    uint64_t WordAddress;
    if (getLexer().is(AsmToken::Identifier)) {
      MCRegister BitReg =
          MatchRegisterName(getLexer().getTok().getIdentifier().lower());
      std::optional<unsigned> Address = getBitWordAddress(BitReg);
      if (!Address)
        return Error(Start, "expected a bit-addressable direct register");
      WordAddress = *Address;
    } else {
      WordAddress = getLexer().getTok().getIntVal();
    }
    Parser.Lex();
    Parser.Lex(); // '.'
    if (WordAddress > 0xff || !getLexer().is(AsmToken::Integer))
      return Error(Start, "bit word address must be in the range 0..255");
    uint64_t Bit = getLexer().getTok().getIntVal();
    SMLoc End = getLexer().getTok().getEndLoc();
    if (Bit > 15)
      return Error(getLexer().getLoc(),
                   "bit number must be in the range 0..15");
    Parser.Lex();
    const MCExpr *Packed =
        MCConstantExpr::create((WordAddress << 4) | Bit, Parser.getContext());
    Operands.push_back(C166Operand::createImmediate(Packed, Start, End));
    return false;
  }

  // DPP1 and DPP2 are both register names and address modifiers.  A following
  // parenthesis unambiguously selects the modifier spelling, so do not consume
  // it as a register and leave the expression behind.
  bool IsModifierCall = false;
  if (getLexer().is(AsmToken::Identifier) &&
      getLexer().peekTok().is(AsmToken::LParen)) {
    StringRef Name = getLexer().getTok().getIdentifier();
    IsModifierCall =
        Name.equals_insensitive("dpp1") || Name.equals_insensitive("dpp2");
  }

  MCRegister Reg;
  SMLoc Start;
  SMLoc End;
  if (!IsModifierCall && tryParseRegister(Reg, Start, End).isSuccess()) {
    Operands.push_back(C166Operand::createRegister(Reg, Start, End));
    return false;
  }

  if (getLexer().is(AsmToken::Identifier) &&
      getLexer().getTok().getIdentifier().starts_with_insensitive("cc_")) {
    SMLoc Loc = getLexer().getLoc();
    StringRef Name = getLexer().getTok().getIdentifier();
    Operands.push_back(C166Operand::createToken(Name, Loc));
    Parser.Lex();
    return false;
  }

  Start = getLexer().getLoc();
  bool HasHash = parseOptionalToken(AsmToken::Hash);
  const MCExpr *Expr;
  if (getLexer().is(AsmToken::Identifier)) {
    StringRef Name = getLexer().getTok().getIdentifier();
    C166::Specifier Specifier = getC166Specifier(Name);

    if (Specifier != C166::S_None) {
      Parser.Lex();
      if (Parser.parseToken(AsmToken::LParen, "expected '(' after modifier") ||
          Parser.parseExpression(Expr) ||
          Parser.parseToken(AsmToken::RParen, "expected ')' after expression"))
        return true;
      Expr = MCSpecifierExpr::create(Expr, Specifier, Parser.getContext());
    } else if (Parser.parseExpression(Expr)) {
      return Error(Start, "expected register or immediate expression");
    }
  } else if (Parser.parseExpression(Expr)) {
    return Error(Start, "expected register or immediate expression");
  }
  End = getLexer().getLoc();
  Operands.push_back(C166Operand::createImmediate(Expr, Start, End, HasHash));
  return false;
}

bool C166AsmParser::parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                                     SMLoc NameLoc, OperandVector &Operands) {
  Operands.push_back(C166Operand::createToken(Name.lower(), NameLoc));
  if (getLexer().is(AsmToken::EndOfStatement)) {
    Parser.Lex();
    return false;
  }

  if (parseOperand(Operands))
    return true;
  while (parseOptionalToken(AsmToken::Comma))
    if (parseOperand(Operands))
      return true;

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    Parser.eatToEndOfStatement();
    return Error(Loc, "unexpected token in instruction");
  }
  Parser.Lex();
  return false;
}

bool C166AsmParser::matchAndEmitInstruction(SMLoc Loc, unsigned &Opcode,
                                            OperandVector &Operands,
                                            MCStreamer &Out,
                                            uint64_t &ErrorInfo,
                                            bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned Result =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);
  switch (Result) {
  case Match_Success:
    Inst.setLoc(Loc);
    Out.emitInstruction(Inst, *STI);
    return false;
  case Match_MnemonicFail:
    return Error(Loc, "invalid instruction mnemonic");
  case Match_InvalidUImm3:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "immediate must be in the range 0..7");
  case Match_InvalidUImm4:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "immediate must be in the range 0..15");
  case Match_InvalidUImm8:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "immediate must be in the range 0..255");
  case Match_InvalidUImm16:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "immediate must be in the range 0..65535");
  case Match_InvalidUImm16Large:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "immediate must be in the range 16..65535");
  case Match_InvalidUImm16ALU:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "immediate must be in the range 8..65535");
  case Match_InvalidSequenceCount:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "instruction count must be in the range 1..4");
  case Match_InvalidAtomicCount:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "atomic count must be in the range 1..4");
  case Match_InvalidBitAddress:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "expected an 8-bit word address and bit number 0..15");
  case Match_InvalidSeg8:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "expected an 8-bit segment or seg(expression)");
  case Match_InvalidSof16:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "expected a 16-bit offset or sof(expression)");
  case Match_InvalidCof16:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "expected a 16-bit code offset or cof(expression)");
  case Match_InvalidPag10:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "expected a 10-bit page or pag(expression)");
  case Match_InvalidPof14:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "expected a 14-bit page offset or pof(expression)");
  case Match_InvalidAbs16:
    return Error(Operands[ErrorInfo]->getStartLoc(),
                 "expected a 16-bit absolute address");
  case Match_InvalidBrTarget:
    return Error(
        Operands[ErrorInfo]->getStartLoc(),
        "expected a symbolic branch target or encoded 8-bit displacement");
  case Match_InvalidOperand: {
    SMLoc ErrorLoc = Loc;
    if (ErrorInfo != ~0ULL && ErrorInfo < Operands.size())
      ErrorLoc = Operands[ErrorInfo]->getStartLoc();
    return Error(ErrorLoc, ErrorInfo >= Operands.size()
                               ? "too few operands for instruction"
                               : "invalid operand for instruction");
  }
  default:
    return Error(Loc, "invalid instruction");
  }
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeC166AsmParser() {
  RegisterMCAsmParser<C166AsmParser> X(getTheC166Target());
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "C166GenAsmMatcher.inc"
