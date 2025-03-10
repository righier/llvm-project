//===-- CSKYInstrInfo.td - Target Description for CSKY -----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the CSKY instructions in TableGen format.
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// CSKY specific DAG Nodes.
//===----------------------------------------------------------------------===//

def SDT_CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>,
                                       SDTCisVT<1, i32>]>;

def SDT_CallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>,
                                     SDTCisVT<1, i32>]>;

def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_CallSeqStart,
                           [SDNPHasChain, SDNPOutGlue]>;

def callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_CallSeqEnd,
                           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

// Target-dependent nodes.
def CSKY_RET : SDNode<"CSKYISD::RET", SDTNone,
    [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

//===----------------------------------------------------------------------===//
// Operand and SDNode transformation definitions.
//===----------------------------------------------------------------------===//
class ImmAsmOperand<string prefix, int width, string suffix> : AsmOperandClass {
  let Name = prefix # "Imm" # width # suffix;
  let RenderMethod = "addImmOperands";
  let DiagnosticType = !strconcat("Invalid", Name);
}

class SImmAsmOperand<int width, string suffix = "">
    : ImmAsmOperand<"S", width, suffix> {
}

class UImmAsmOperand<int width, string suffix = "">
    : ImmAsmOperand<"U", width, suffix> {
}

class OImmAsmOperand<int width, string suffix = "">
    : ImmAsmOperand<"O", width, suffix> {
}

class oimm<int num> : Operand<i32>,
  ImmLeaf<i32, "return isUInt<"#num#">(Imm - 1);"> {
  let EncoderMethod = "getOImmOpValue";
  let ParserMatchClass = OImmAsmOperand<num>;
  let DecoderMethod = "decodeOImmOperand<"#num#">";
}

class uimm<int num, int shift = 0> : Operand<i32>,
  ImmLeaf<i32, "return isShiftedUInt<"#num#", "#shift#">(Imm);"> {
  let EncoderMethod = "getImmOpValue<"#shift#">";
  let ParserMatchClass =
    !if(!ne(shift, 0),
        UImmAsmOperand<num, "Shift"#shift>,
        UImmAsmOperand<num>);
  let DecoderMethod = "decodeUImmOperand<"#num#", "#shift#">";
}

class simm<int num, int shift = 0> : Operand<i32>,
  ImmLeaf<i32, "return isShiftedInt<"#num#", "#shift#">(Imm);"> {
  let EncoderMethod = "getImmOpValue<"#shift#">";
  let ParserMatchClass = SImmAsmOperand<num>;
  let DecoderMethod = "decodeSImmOperand<"#num#", "#shift#">";
}

def nimm_XFORM : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(~N->getSExtValue(), SDLoc(N), MVT::i32);
}]>;
class nimm<int num> : Operand<i32>,
  ImmLeaf<i32, "return isUInt<"#num#">(~Imm);", nimm_XFORM> {
  let ParserMatchClass = UImmAsmOperand<num>;
}

def uimm32_hi16 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant((N->getZExtValue() >> 16) & 0xFFFF,
    SDLoc(N), MVT::i32);
}]>;
def uimm32_lo16 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(N->getZExtValue()& 0xFFFF, SDLoc(N), MVT::i32);
}]>;
def uimm16_16_xform : Operand<i32>,
  ImmLeaf<i32, "return isShiftedUInt<16, 16>(Imm);", uimm32_hi16> {
  let ParserMatchClass = UImmAsmOperand<16>;
  let EncoderMethod = "getImmOpValue";
}

def uimm_shift : Operand<i32>, ImmLeaf<i32, "return isUInt<2>(Imm);"> {
  let EncoderMethod = "getImmShiftOpValue";
  let ParserMatchClass = UImmAsmOperand<2>;
  let DecoderMethod = "decodeImmShiftOpValue";
}

def CSKYSymbol : AsmOperandClass {
  let Name = "CSKYSymbol";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidCSKYSymbol";
  let ParserMethod = "parseCSKYSymbol";
}

def br_symbol : Operand<iPTR> {
  let EncoderMethod =
    "getBranchSymbolOpValue<CSKY::fixup_csky_pcrel_imm16_scale2>";
  let ParserMatchClass = CSKYSymbol;
  let DecoderMethod = "decodeSImmOperand<16, 1>";
  let PrintMethod = "printCSKYSymbolOperand";
  let OperandType = "OPERAND_PCREL";
}

def call_symbol : Operand<iPTR> {
  let ParserMatchClass = CSKYSymbol;
  let EncoderMethod = "getCallSymbolOpValue";
  let DecoderMethod = "decodeSImmOperand<26, 1>";
  let PrintMethod = "printCSKYSymbolOperand";
  let OperandType = "OPERAND_PCREL";
}

def Constpool : AsmOperandClass {
  let Name = "Constpool";
  let RenderMethod = "addConstpoolOperands";
  let DiagnosticType = "InvalidConstpool";
  let ParserMethod = "parseConstpoolSymbol";
}

def constpool_symbol : Operand<iPTR> {
  let ParserMatchClass = Constpool;
  let EncoderMethod =
    "getConstpoolSymbolOpValue<CSKY::fixup_csky_pcrel_uimm16_scale4>";
  let DecoderMethod = "decodeUImmOperand<16, 2>";
  let PrintMethod = "printConstpool";
  let OperandType = "OPERAND_PCREL";
}

def DataAsmClass : AsmOperandClass {
  let Name = "DataSymbol";
  let RenderMethod = "addConstpoolOperands";
  let DiagnosticType = "InvalidConstpool";
  let ParserMethod = "parseDataSymbol";
}

class data_symbol<string reloc, int shift> : Operand<iPTR> {
  let ParserMatchClass = Constpool;
  let EncoderMethod =
    "getDataSymbolOpValue<"#reloc#">";
  let DecoderMethod = "decodeUImmOperand<18, "#shift#">";
  let PrintMethod = "printDataSymbol";
}

def bare_symbol : Operand<iPTR> {
  let ParserMatchClass = CSKYSymbol;
  let EncoderMethod = "getBareSymbolOpValue";
  let PrintMethod = "printCSKYSymbolOperand";
  let DecoderMethod = "decodeSImmOperand<18, 1>";
  let OperandType = "OPERAND_PCREL";
}

def oimm3 : oimm<3>;
def oimm4 : oimm<4>;
def oimm5 : oimm<5>;
def oimm6 : oimm<6>;

def imm5_idly : Operand<i32>, ImmLeaf<i32,
  "return Imm <= 32 && Imm >= 0;"> {
  let EncoderMethod = "getImmOpValueIDLY";
  let DecoderMethod = "decodeOImmOperand<5>";
}

def oimm8 : oimm<8>;
def oimm12 : oimm<12>;
def oimm16 : oimm<16>;

def nimm12 : nimm<12>;

def uimm1 : uimm<1>;
def uimm2 : uimm<2>;


def uimm2_jmpix : Operand<i32>,
  ImmLeaf<i32, "return Imm == 16 || Imm == 24 || Imm == 32 || Imm == 40;"> {
  let EncoderMethod = "getImmJMPIX";
  let DecoderMethod = "decodeJMPIXImmOperand";
}

def uimm3 : uimm<3>;
def uimm4 : uimm<4>;
def uimm5 : uimm<5>;
def uimm5_msb_size : uimm<5> {
  let EncoderMethod = "getImmOpValueMSBSize";
}

def uimm5_1 : uimm<5, 1>;
def uimm5_2 : uimm<5, 2>;
def uimm6 : uimm<6>;
def uimm7 : uimm<7>;
def uimm7_1 : uimm<7, 1>;
def uimm7_2 : uimm<7, 2>;
def uimm7_3 : uimm<7, 3>;
def uimm8 : uimm<8>;
def uimm8_2 : uimm<8, 2>;
def uimm8_3 : uimm<8, 3>;
def uimm8_8 : uimm<8, 8>;
def uimm8_16 : uimm<8, 16>;
def uimm8_24 : uimm<8, 24>;
def uimm12 : uimm<12>;
def uimm12_1 : uimm<12, 1>;
def uimm12_2 : uimm<12, 2>;
def uimm16 : uimm<16>;
def uimm16_8 : uimm<16, 8>;
def uimm16_16 : uimm<16, 16>;
def uimm20 : uimm<20>;
def uimm24 : uimm<24>;
def uimm24_8 : uimm<24, 8>;

def simm8_2 : simm<8, 2>;

class RegSeqAsmOperand<string Suffix = ""> : AsmOperandClass {
  let Name = "RegSeq"#Suffix;
  let RenderMethod = "addRegSeqOperands";
  let DiagnosticType = "InvalidRegSeq";
  let ParserMethod = "parseRegSeq";
}

def regseq : Operand<iPTR> {
  let EncoderMethod = "getRegisterSeqOpValue";
  let ParserMatchClass = RegSeqAsmOperand<"">;
  let PrintMethod = "printRegisterSeq";
  let DecoderMethod = "DecodeRegSeqOperand";
  let MIOperandInfo = (ops GPR, uimm5);
}

def RegListAsmOperand : AsmOperandClass {
  let Name = "RegList";
  let RenderMethod = "addRegListOperands";
  let DiagnosticType = "InvalidRegList";
  let ParserMethod = "parseRegList";
}

def reglist : Operand<iPTR> {
  let ParserMatchClass = RegListAsmOperand;
  let PrintMethod = "printRegisterList";
}

def PSRFlag : AsmOperandClass {
  let Name = "PSRFlag";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidPSRFlag";
  let ParserMethod = "parsePSRFlag";
}

def psrflag : Operand<i32>, ImmLeaf<i32, "return isShiftedUInt<5, 0>(Imm);"> {
  let EncoderMethod = "getImmOpValue";
  let ParserMatchClass = PSRFlag;
  let PrintMethod = "printPSRFlag";
}

//===----------------------------------------------------------------------===//
// Instruction Formats
//===----------------------------------------------------------------------===//

include "CSKYInstrFormats.td"

//===----------------------------------------------------------------------===//
// Instruction definitions.
//===----------------------------------------------------------------------===//

class TriOpFrag<dag res> : PatFrag<(ops node: $LHS, node:$MHS, node:$RHS), res>;
class BinOpFrag<dag res> : PatFrag<(ops node:$LHS, node:$RHS), res>;
class UnOpFrag<dag res> : PatFrag<(ops node:$Src), res>;

def eqToAdd : PatFrag<(ops node:$lhs, node:$rhs), (or node:$lhs, node:$rhs), [{
  return isOrEquivalentToAdd(N);
}]>;

def BaseAddr : ComplexPattern<iPTR, 1, "SelectBaseAddr">;


//===----------------------------------------------------------------------===//
// CSKYPseudo
//===----------------------------------------------------------------------===//

// Pessimistically assume the stack pointer will be clobbered
let Defs = [R14], Uses = [R14] in {
def ADJCALLSTACKDOWN : CSKYPseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
  "!ADJCALLSTACKDOWN $amt1, $amt2", [(callseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP   : CSKYPseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
  "!ADJCALLSTACKUP $amt1, $amt2", [(callseq_end timm:$amt1, timm:$amt2)]>;
} // Defs = [R14], Uses = [R14]


//===----------------------------------------------------------------------===//
// Basic ALU instructions.
//===----------------------------------------------------------------------===//

let Predicates = [iHasE2] in {
  let isReMaterializable = 1, isAsCheapAsAMove = 1 in {
  let isAdd = 1 in
  def ADDI32 : I_12<0x0, "addi32", add, oimm12>;
  def SUBI32 : I_12<0x1, "subi32", sub, oimm12>;
  def ORI32 : I_16_ZX<"ori32", uimm16,
  [(set GPR:$rz, (or GPR:$rx, uimm16:$imm16))]>;
  def XORI32 : I_12<0x4, "xori32", xor, uimm12>;
  def ANDI32 : I_12<0x2, "andi32", and, uimm12>;
  def ANDNI32 : I_12<0x3, "andni32", and, nimm12>;
  def LSLI32 : I_5_XZ<0x12, 0x1, "lsli32",
    (outs GPR:$rz), (ins GPR:$rx, uimm5:$imm5),
    [(set GPR:$rz, (shl GPR:$rx, uimm5:$imm5))]>;
  def LSRI32 : I_5_XZ<0x12, 0x2, "lsri32",
    (outs GPR:$rz), (ins GPR:$rx, uimm5:$imm5),
    [(set GPR:$rz, (srl GPR:$rx, uimm5:$imm5))]>;
  def ASRI32 : I_5_XZ<0x12, 0x4, "asri32",
    (outs GPR:$rz), (ins GPR:$rx, uimm5:$imm5),
    [(set GPR:$rz, (sra GPR:$rx, uimm5:$imm5))]>;
  def ROTLI32 : I_5_XZ<0x12, 0x8, "rotli32",
    (outs GPR:$rz), (ins GPR:$rx, uimm5:$imm5),
    [(set GPR:$rz, (rotl GPR:$rx, uimm5:$imm5))]>;

  def ROTRI32 : CSKYPseudo<(outs GPR:$rz), (ins GPR:$rx, oimm5:$imm5),
                            "rotri32 $rz, $rx, $imm5", []>;
  }
  let isAdd = 1 in
  def ADDU32 : R_YXZ_SP_F1<0x0, 0x1,
    BinOpFrag<(add node:$LHS, node:$RHS)>, "addu32", 1>;
  def SUBU32 : R_YXZ_SP_F1<0x0, 0x4,
    BinOpFrag<(sub node:$LHS, node:$RHS)>, "subu32">;

  def MULT32 : R_YXZ_SP_F1<0x21, 0x1,
    BinOpFrag<(mul node:$LHS, node:$RHS)>, "mult32", 1>;
  def AND32 : R_YXZ_SP_F1<0x8, 0x1,
    BinOpFrag<(and node:$LHS, node:$RHS)>, "and32", 1>;
  def ANDN32 : R_YXZ_SP_F1<0x8, 0x2,
    BinOpFrag<(and node:$LHS, (not node:$RHS))>, "andn32">;
  def OR32: R_YXZ_SP_F1<0x9, 0x1,
    BinOpFrag<(or node:$LHS, node:$RHS)>, "or32", 1>;
  def XOR32 : R_YXZ_SP_F1<0x9, 0x2,
    BinOpFrag<(xor node:$LHS, node:$RHS)>, "xor32", 1>;
  def NOR32 : R_YXZ_SP_F1<0x9, 0x4,
    BinOpFrag<(not (or node:$LHS, node:$RHS))>, "nor32", 1>;
  let isCodeGenOnly = 1 in
  def NOT32 : R_XXZ<0b001001, 0b00100, (outs GPR:$rz), (ins GPR:$rx),
    "not32", [(set GPR:$rz, (not GPR:$rx))]>;

  let Size = 8 in
  def NEG32 : CSKYPseudo<(outs GPR:$rd), (ins GPR:$rx), "neg32 $rd, $rx", []>;

  let Size = 8 in
  def RSUBI32 : CSKYPseudo<(outs GPR:$rd), (ins GPR:$rx, uimm12:$imm12), "rsubi32 $rd, $rx, $imm12", []>;

  def LSL32 : R_YXZ_SP_F1<0x10, 0x1,
    BinOpFrag<(shl node:$LHS, node:$RHS)>, "lsl32">;
  def LSR32 : R_YXZ_SP_F1<0x10, 0x2,
    BinOpFrag<(srl node:$LHS, node:$RHS)>, "lsr32">;
  def ASR32 : R_YXZ_SP_F1<0x10, 0x4,
    BinOpFrag<(sra node:$LHS, node:$RHS)>, "asr32">;
  def ROTL32 : R_YXZ_SP_F1<0x10, 0x8,
    BinOpFrag<(rotl node:$LHS, (and node:$RHS, 0x1f))>, "rotl32">;

  def BMASKI32 : I_5_Z<0b010100, 0x1, "bmaski32", oimm5, []>;
  def LSLC32 : I_5_XZ<0x13, 0x1, "lslc32",
    (outs GPR:$rz, CARRY:$cout), (ins GPR:$rx, oimm5:$imm5), []>;
  def LSRC32 : I_5_XZ<0x13, 0x2, "lsrc32",
    (outs GPR:$rz, CARRY:$cout), (ins GPR:$rx, oimm5:$imm5), []>;
  def ASRC32 : I_5_XZ<0x13, 0x4, "asrc32",
    (outs GPR:$rz, CARRY:$cout), (ins GPR:$rx, oimm5:$imm5), []>;
  def XSR32 : I_5_XZ<0x13, 0x8, "xsr32",
    (outs GPR:$rz, CARRY:$cout), (ins GPR:$rx, oimm5:$imm5, CARRY:$cin), []>;

  def IXH32 : R_YXZ_SP_F1<0x2, 0x1,
    BinOpFrag<(add node:$LHS, (shl node:$RHS, (i32 1)))>, "ixh32">;
  def IXW32 : R_YXZ_SP_F1<0x2, 0x2,
    BinOpFrag<(add node:$LHS, (shl node:$RHS, (i32 2)))>, "ixw32">;
  let Predicates = [iHas2E3] in
  def IXD32 : R_YXZ_SP_F1<0x2, 0x4,
    BinOpFrag<(add node:$LHS, (shl node:$RHS, (i32 3)))>, "ixd32">;

  let isCommutable = 1, isAdd = 1 in
  def ADDC32 : R_YXZ<0x31, 0x0, 0x2, (outs GPR:$rz, CARRY:$cout),
    (ins GPR:$rx, GPR:$ry, CARRY:$cin), "addc32", []>;
  def SUBC32 : R_YXZ<0x31, 0x0, 0x8, (outs GPR:$rz, CARRY:$cout),
    (ins GPR:$rx, GPR:$ry, CARRY:$cin), "subc32", []>;

  def INCF32 : I_5_ZX<0x3, 0x1, "incf32", uimm5, []>;
  def INCT32 : I_5_ZX<0x3, 0x2, "inct32", uimm5, []>;
  def DECF32 : I_5_ZX<0x3, 0x4, "decf32", uimm5, []>;
  def DECT32 : I_5_ZX<0x3, 0x8, "dect32", uimm5, []>;
}

let Predicates = [iHas2E3] in {
  def DIVS32 : R_YXZ_SP_F1<0x20, 0x2,
    BinOpFrag<(sdiv node:$LHS, node:$RHS)>, "divs32">;
  def DIVU32 : R_YXZ_SP_F1<0x20, 0x1,
    BinOpFrag<(udiv node:$LHS, node:$RHS)>, "divu32">;

  def DECGT32 : I_5_XZ<0x4, 0x1, "decgt32",
    (outs GPR:$rz, CARRY:$cout), (ins GPR:$rx, uimm5:$imm5), []>;
  def DECLT32 : I_5_XZ<0x4, 0x2, "declt32",
    (outs GPR:$rz, CARRY:$cout), (ins GPR:$rx, uimm5:$imm5), []>;
  def DECNE32 : I_5_XZ<0x4, 0x4, "decne32",
    (outs GPR:$rz, CARRY:$cout), (ins GPR:$rx, uimm5:$imm5), []>;

  def SEXT32 : I_5_XZ_U<0x16, (outs GPR:$rz), (ins GPR:$rx, uimm5:$msb, uimm5:$lsb), "sext32", []>;
  let isCodeGenOnly = 1 in {
  def SEXTB32 : I_5_XZ_US<0x16, 0, 7, "sextb32", sext_inreg, i8>;
  def SEXTH32 : I_5_XZ_US<0x16, 0, 15, "sexth32", sext_inreg, i16>;
  def ZEXTB32 : I_5_XZ_UZ<0x15, 0, 7, "zextb32", 255>;
  def ZEXTH32 : I_5_XZ_UZ<0x15, 0, 15, "zexth32", 65535>;
  }
  def ZEXT32 : I_5_XZ_U<0x15, (outs GPR:$rz), (ins GPR:$rx, uimm5:$msb, uimm5:$lsb), "zext32",[]>;

  let Constraints = "$rZ = $rz" in
  def INS32 : I_5_XZ_INS<0b010111, (outs GPR:$rz), (ins GPR:$rZ, GPR:$rx, uimm5_msb_size:$msb, uimm5:$lsb), "ins32", []>;
}

let Predicates = [iHas3E3r1] in {
def MULTS32 : R_YXZ<0x3e, 0x20, 0x10, (outs GPRPair:$rz),
    (ins GPR:$rx, GPR:$ry), "mul.s32", []>;
def MULTU32 : R_YXZ<0x3e, 0x20, 0x00, (outs GPRPair:$rz),
    (ins GPR:$rx, GPR:$ry), "mul.u32", []>;

let Constraints = "$rZ = $rz" in {
def MULATS32 : R_YXZ<0x3e, 0x20, 0x14, (outs GPRPair:$rZ),
    (ins GPRPair:$rz, GPR:$rx, GPR:$ry), "mula.s32", []>;
def MULATU32 : R_YXZ<0x3e, 0x20, 0x04, (outs GPRPair:$rZ),
    (ins GPRPair:$rz, GPR:$rx, GPR:$ry), "mula.u32", []>;
}
}

def MULSH32 : R_YXZ<0x31, 0b100100, 0b00001, (outs GPR:$rz),
    (ins GPR:$rx, GPR:$ry), "mulsh32", []>;

//===----------------------------------------------------------------------===//
// Load & Store instructions.
//===----------------------------------------------------------------------===//

def LD32B : I_LD<AddrMode32B, 0x0, "ld32.b", uimm12>;
def LD32H : I_LD<AddrMode32H, 0x1, "ld32.h", uimm12_1>;
def LD32W : I_LD<AddrMode32WD, 0x2, "ld32.w", uimm12_2>;

let OutOperandList = (outs GPRPair:$rz) in
def LD32D : I_LD<AddrMode32WD, 0x3, "ld32.d", uimm12_2>;

let Predicates = [iHasE2] in {
  def LD32BS : I_LD<AddrMode32B, 0x4, "ld32.bs", uimm12>;
  def LD32HS : I_LD<AddrMode32H, 0x5, "ld32.hs", uimm12_1>;

  def LDM32 : I_5_YX<0b110100, 0b000111,
    (outs), (ins GPR:$rx, regseq:$regs, variable_ops), "ldm32\t$regs, (${rx})", []>;
  def STM32 : I_5_YX<0b110101, 0b000111,
    (outs), (ins GPR:$rx, regseq:$regs, variable_ops), "stm32\t$regs, (${rx})", []>;

  let Size = 4, isCodeGenOnly = 0 in {
  def LDQ32 : CSKYPseudo<(outs), (ins GPR:$rx, regseq:$regs, variable_ops),
                             "ldq32\t$regs, (${rx})", []>;
  def STQ32 : CSKYPseudo<(outs), (ins GPR:$rx, regseq:$regs, variable_ops),
                             "stq32\t$regs, (${rx})", []>;
  }

}

def ST32B : I_ST<AddrMode32B, 0x0, "st32.b", uimm12>;
def ST32H : I_ST<AddrMode32H, 0x1, "st32.h", uimm12_1>;
def ST32W : I_ST<AddrMode32WD, 0x2, "st32.w", uimm12_2>;

let InOperandList = (ins GPRPair:$rz, GPR:$rx, uimm12_2:$imm12 ) in
def ST32D : I_ST<AddrMode32WD, 0x3, "st32.d", uimm12_2>;

let Predicates = [iHas2E3] in {
  def LDR32B :  I_LDR<0x0, "ldr32.b">;
  def LDR32BS :  I_LDR<0x4, "ldr32.bs">;
  def LDR32H :  I_LDR<0x1, "ldr32.h">;
  def LDR32HS :  I_LDR<0x5, "ldr32.hs">;
  def LDR32W :  I_LDR<0x2, "ldr32.w">;
  def STR32B :  I_STR<0x0, "str32.b">;
  def STR32H :  I_STR<0x1, "str32.h">;
  def STR32W :  I_STR<0x2, "str32.w">;
}

// Indicate that we're dumping the CR register, so we'll need to
// scavenge a register for it.
let mayStore = 1 in {
def SPILL_CARRY : CSKYPseudo<(outs), (ins CARRY:$cond, GPR:$rx, uimm12_2:$imm),
                             "!SPILL_CARRY $cond, $rx, $imm", []>;
}

// Indicate that we're restoring the CR register (previously
// spilled), so we'll need to scavenge a register for it.
let mayLoad = 1 in {
def RESTORE_CARRY : CSKYPseudo<(outs CARRY:$cond), (ins GPR:$rx, uimm12_2:$imm),
                                "!RESTORE_CARRY $cond, $rx, $imm", []>;
}

let mayLoad = 1 in {
def STORE_PAIR : CSKYPseudo<(outs), (ins GPRPair:$rz, GPR:$rx, uimm12_2:$imm),
                            "!STORE_PAIR $rz, $rx, $imm", []>;
}

let mayLoad = 1 in {
def LOAD_PAIR : CSKYPseudo<(outs GPRPair:$rz), (ins GPR:$rx, uimm12_2:$imm),
                            "!LOAD_PAIR $rz, $rx, $imm", []>;
}

//===----------------------------------------------------------------------===//
// Compare instructions.
//===----------------------------------------------------------------------===//
let Predicates = [iHasE2] in {
  def CMPNEI32 : I_16_X<0x1A, "cmpnei32", uimm16>;
  def CMPHSI32 : I_16_X<0x18, "cmphsi32", oimm16>;
  def CMPLTI32 : I_16_X<0x19, "cmplti32", oimm16>;
  def CMPLEI32 : CSKYPseudo<(outs CARRY:$ca), (ins GPR:$rx, uimm16:$imm16),
    "cmplei32\t$rx, $imm16", []>;
}
let Predicates = [iHas2E3] in {
  def CMPNE32 : R_YX<0x1, 0x4, "cmpne32">;
  def CMPHS32 : R_YX<0x1, 0x1, "cmphs32">;
  def CMPLT32 : R_YX<0x1, 0x2, "cmplt32">;

  def SETC32 : CSKY32Inst<AddrModeNone, 0x31,
    (outs CARRY:$ca), (ins), "setc32", []> {
    let Inst{25 - 21} = 0; //rx
    let Inst{20 - 16} = 0; //ry
    let Inst{15 - 10} = 0x1;
    let Inst{9 - 5} = 0x1;
    let Inst{4 - 0} = 0;
    let isCompare = 1;
  }
  def CLRC32 : CSKY32Inst<AddrModeNone, 0x31,
    (outs CARRY:$ca), (ins), "clrc32", []> {
    let Inst{25 - 21} = 0; //rx
    let Inst{20 - 16} = 0; //ry
    let Inst{15 - 10} = 0x1;
    let Inst{9 - 5} = 0x4;
    let Inst{4 - 0} = 0;
    let isCompare = 1;
  }

  def TST32 : R_YX<0x8, 0x4, "tst32">;
  def TSTNBZ32 : R_X<0x8, 0x8,
    (outs CARRY:$ca), (ins GPR:$rx), "tstnbz32", []>;
}

//===----------------------------------------------------------------------===//
// Data move instructions.
//===----------------------------------------------------------------------===//

let Predicates= [iHasE2] in {
  let isCodeGenOnly = 1 in {
  def MOVT32 : R_ZX<0x3, 0x2, "movt32", []>;
  def MOVF32 : R_ZX<0x3, 0x1, "movf32", []>;
  }
  def MOVI32 : I_16_MOV<0x10, "movi32", uimm16>;
  let Size = 4, isCodeGenOnly = 0 in
  def BGENI : CSKYPseudo<(outs GPR:$dst), (ins uimm5:$imm), "bgeni\t$dst, $imm", []>;
  def : InstAlias<"bgeni16 $dst, $imm", (BGENI GPR:$dst, uimm5:$imm)>;
  def : InstAlias<"bgeni32 $dst, $imm", (BGENI GPR:$dst, uimm5:$imm)>;
  def MOVIH32 : I_16_MOV<0x11, "movih32", uimm16_16_xform>;
  def MVC32 : R_Z_1<0x1, 0x8, "mvc32">;
  let isCodeGenOnly = 1 in
  def MOV32 : R_XZ<0x12, 0x1, "mov32">;

  let usesCustomInserter = 1 in
  def ISEL32 : CSKYPseudo<(outs GPR:$dst), (ins CARRY:$cond, GPR:$src1, GPR:$src2),
    "!isel32\t$dst, $src1, src2", [(set GPR:$dst, (select CARRY:$cond, GPR:$src1, GPR:$src2))]>;
}

let Predicates = [iHas2E3] in {
  def MVCV32 : R_Z_1<0x1, 0x10, "mvcv32">;
  def CLRF32 : R_Z_2<0xB, 0x1, "clrf32", []>;
  def CLRT32 : R_Z_2<0xB, 0x2, "clrt32", []>;
}

//===----------------------------------------------------------------------===//
// Branch and call instructions.
//===----------------------------------------------------------------------===//

let isBranch = 1, isTerminator = 1 in {
  let isBarrier = 1, isPredicable = 1 in
    def BR32 : I_16_L<0x0, (outs), (ins br_symbol:$imm16), "br32\t$imm16",
                     [(br bb:$imm16)]>;

  def BT32 : I_16_L<0x3, (outs), (ins CARRY:$ca, br_symbol:$imm16),
    "bt32\t$imm16", [(brcond CARRY:$ca, bb:$imm16)]>, Requires<[iHasE2]>;
  def BF32 : I_16_L<0x2, (outs), (ins CARRY:$ca, br_symbol:$imm16),
    "bf32\t$imm16", []>, Requires<[iHasE2]>;
}

let Predicates = [iHas2E3] in {
  def BEZ32 : I_16_X_L<0x8, "bez32", br_symbol>;
  def BNEZ32 : I_16_X_L<0x9, "bnez32", br_symbol>;
  def BHZ32 : I_16_X_L<0xA, "bhz32", br_symbol>;
  def BLSZ32 : I_16_X_L<0xB, "blsz32", br_symbol>;
  def BLZ32 : I_16_X_L<0xC, "blz32", br_symbol>;
  def BHSZ32 : I_16_X_L<0xD, "bhsz32", br_symbol>;

  let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1 in {
    def JMP32 : I_16_JX<0x6, "jmp32", [(brind GPR:$rx)]>; // jmp to register
    def JMPI32 : I_16_L<0x16, (outs), (ins constpool_symbol:$imm16),
                   "jmpi32\t$imm16", []>;
  }

  let isCall = 1, Defs = [ R15 ] in
    def JSR32 : I_16_JX<0x7, "jsr32", []>;

  let isCall = 1, Defs = [ R15 ] , mayLoad = 1 in
    def JSRI32: I_16_L<0x17, (outs),
      (ins constpool_symbol:$imm16), "jsri32\t$imm16", []>;
}

def BNEZAD32 : CSKY32Inst<AddrModeNone, 0x3a,
  (outs GPR:$rx_u), (ins GPR:$rx, br_symbol:$imm16), "bnezad32\t$rx, $imm16", []> {
  bits<5> rx;
  bits<16> imm16;
  let Inst{25 - 21} = 0x1;
  let Inst{20 - 16} = rx;
  let Inst{15 - 0} = imm16;
  let isBranch = 1;
  let isTerminator = 1;
  let Constraints = "$rx_u = $rx";
  let Predicates = [iHas2E3, iHas10E60];
}

def BSR32 : J<0x38, (outs), (ins call_symbol:$offset), "bsr32", []>;

def : InstAlias<"bsr $dst", (BSR32 call_symbol:$dst)>;

def BSR32_BR : J<0x38, (outs), (ins call_symbol:$offset), "bsr32", []>{
  let isCodeGenOnly = 1;
  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let isPredicable = 1;
  let Defs = [ R15 ];
}

let Predicates = [iHasE2], isCodeGenOnly = 1 in {
  def RTS32 : I_16_RET<0x6, 0xF, "rts32", [(CSKY_RET)]>;
}


//===----------------------------------------------------------------------===//
// Symbol address instructions.
//===----------------------------------------------------------------------===//

def data_symbol_b : data_symbol<"CSKY::fixup_csky_doffset_imm18", 0>;
def data_symbol_h : data_symbol<"CSKY::fixup_csky_doffset_imm18_scale2", 1>;
def data_symbol_w : data_symbol<"CSKY::fixup_csky_doffset_imm18_scale4", 2> {
  let ParserMatchClass = DataAsmClass;
}

let Predicates = [iHas2E3] in {

def GRS32 : I_18_Z_L<0x3, "grs32\t$rz, $offset",
                    (outs GPR:$rz), (ins bare_symbol:$offset), []>;
def : InstAlias<"grs\t$rz, $offset", (GRS32 GPR:$rz, bare_symbol:$offset)>;

let Uses = [R28] in {
def LRS32B : I_18_Z_L<0x0, "lrs32.b\t$rz, $offset",
                    (outs GPR:$rz), (ins data_symbol_b:$offset), []>;
def LRS32H : I_18_Z_L<0x1, "lrs32.h\t$rz, $offset",
                    (outs GPR:$rz), (ins data_symbol_h:$offset), []>;
def LRS32W : I_18_Z_L<0x2, "lrs32.w\t$rz, $offset",
                    (outs GPR:$rz), (ins data_symbol_w:$offset), []>;
def SRS32B : I_18_Z_L<0x4, "srs32.b\t$rz, $offset",
                    (outs), (ins GPR:$rz, data_symbol_b:$offset), []>;
def SRS32H : I_18_Z_L<0x5, "srs32.h\t$rz, $offset",
                    (outs), (ins GPR:$rz, data_symbol_h:$offset), []>;
def SRS32W : I_18_Z_L<0x6, "srs32.w\t$rz, $offset",
                    (outs), (ins GPR:$rz, data_symbol_w:$offset), []>;
}

def PUSH32 : I_12_PP<0b11111, 0b00000, (outs), (ins reglist:$regs, variable_ops), "push32 $regs">;

let Uses = [R14, R15], isReturn = 1, isTerminator = 1, isBarrier = 1 in
def POP32 : I_12_PP<0b11110, 0b00000, (outs), (ins reglist:$regs, variable_ops), "pop32 $regs">;

}

let mayLoad = 1, mayStore = 0 in {
def LRW32 : I_16_Z_L<0x14, "lrw32", (ins constpool_symbol:$imm16), []>;
let isCodeGenOnly = 1 in
def LRW32_Gen : I_16_Z_L<0x14, "lrw32", (ins bare_symbol:$src1, constpool_symbol:$imm16), []>;
}

//===----------------------------------------------------------------------===//
// Atomic and fence instructions.
//===----------------------------------------------------------------------===//

let Predicates = [iHasMP1E2] in {
  def BRWARW : BAR<0b01111, "bar.brwarw", 0>;
  def BRWARWS : BAR<0b01111, "bar.brwarws", 1>;
  def BRARW : BAR<0b00111, "bar.brarw", 0>;
  def BRARWS : BAR<0b00111, "bar.brarws", 1>;
  def BRWAW : BAR<0b01110, "bar.brwaw", 0>;
  def BRWAWS : BAR<0b01110, "bar.brwaws", 1>;
  def BRAR : BAR<0b00101, "bar.brar", 0>;
  def BRARS : BAR<0b00101, "bar.brars", 1>;
  def BWAW : BAR<0b01010, "bar.bwaw", 0>;
  def BWAWS : BAR<0b01010, "bar.bwaws", 1>;

  def LDEX32W : I_LD<AddrMode32WD, 0x7, "ldex32.w", uimm12_2>;
  let Constraints = "$rd = $rz" in
    def STEX32W : I_LDST<AddrMode32WD, 0x37, 7,
      (outs GPR:$rd), (ins GPR:$rz, GPR:$rx, uimm12_2:$imm12), "stex32.w", []>;
}

//===----------------------------------------------------------------------===//
// Other operation instructions.
//===----------------------------------------------------------------------===//

let Predicates = [iHas2E3] in {
  def BREV32 : R_XZ<0x18, 0x10, "brev32">;
  def ABS32 : R_XZ<0x0, 0x10, "abs32">;
  def BGENR32 : R_XZ<0x14, 0x2, "bgenr32">;
}

let Predicates = [iHasE2] in {
  def REVB32 : R_XZ<0x18, 0x4, "revb32">;
  def REVH32 : R_XZ<0x18, 0x8, "revh32">;
  def FF0 : R_XZ<0x1F, 0x1, "ff0.32">;
  def FF1 : R_XZ<0x1F, 0x2, "ff1.32">;
  def XTRB0 : R_XZ<0x1C, 0x1, "xtrb0.32">;
  def XTRB1 : R_XZ<0x1C, 0x2, "xtrb1.32">;
  def XTRB2 : R_XZ<0x1C, 0x4, "xtrb2.32">;
  def XTRB3 : R_XZ<0x1C, 0x8, "xtrb3.32">;
  def BTSTI32 : I_5_X<0x0A, 0x4, "btsti32", uimm5, []>;
  def BCLRI32 : I_5_XZ<0xA, 0x1, "bclri32",
  (outs GPR:$rz), (ins GPR:$rx, uimm5:$imm5), []>;
  def BSETI32 : I_5_XZ<0xA, 0x2, "bseti32",
  (outs GPR:$rz), (ins GPR:$rx, uimm5:$imm5), []>;
}

//===----------------------------------------------------------------------===//
// Special instructions.
//===----------------------------------------------------------------------===//

def MFFCR : CSKY32Inst<AddrModeNone, 0x30,
  (outs GPR:$rx), (ins), "mfcr\t$rx, fcr", []> {
  bits<5> rx;

  let Inst{25 - 21} = 0b00010;
  let Inst{20 - 16} = 0b00001;
  let Inst{15 - 10} = 0b011000;
  let Inst{9 - 5} = 0b00001;
  let Inst{4 - 0} = rx;
  let hasSideEffects = 1;
  let isCodeGenOnly = 1;
}

def MTFCR : CSKY32Inst<AddrModeNone, 0x30,
  (outs), (ins GPR:$rx), "mtcr\t$rx, fcr", []> {
  bits<5> rx;

  let Inst{25 - 21} = 0b00010;
  let Inst{20 - 16} = rx;
  let Inst{15 - 10} = 0b011001;
  let Inst{9 - 5} = 0b00001;
  let Inst{4 - 0} = 0b00001;
  let hasSideEffects = 1;
  let isCodeGenOnly = 1;
}

def SYNC32 : I_5_IMM5<0x30, 0b000001, 0b00001, "sync32", uimm5, []>;

def SYNC0_32 : CSKY32Inst<AddrModeNone, 0x30, (outs), (ins),
                 "sync32", []> {
  let Inst{25 - 21} = 0;
  let Inst{20 - 16} = 0;
  let Inst{15 - 10} = 0b000001;
  let Inst{9 - 5} = 0b00001;
  let Inst{4 - 0} = 0;
}

def SYNC_32_I : CSKY32Inst<AddrModeNone, 0x30, (outs), (ins),
                 "sync32.i", []> {
  let Inst{25 - 21} = 1;
  let Inst{20 - 16} = 0;
  let Inst{15 - 10} = 0b000001;
  let Inst{9 - 5} = 0b00001;
  let Inst{4 - 0} = 0;
}

def SYNC_32_S : CSKY32Inst<AddrModeNone, 0x30, (outs), (ins),
                 "sync32.s", []> {
  let Inst{25 - 21} = 0b10000;
  let Inst{20 - 16} = 0;
  let Inst{15 - 10} = 0b000001;
  let Inst{9 - 5} = 0b00001;
  let Inst{4 - 0} = 0;
}

def SYNC_32_IS : CSKY32Inst<AddrModeNone, 0x30, (outs), (ins),
                 "sync32.is", []> {
  let Inst{25 - 21} = 0b10001;
  let Inst{20 - 16} = 0;
  let Inst{15 - 10} = 0b000001;
  let Inst{9 - 5} = 0b00001;
  let Inst{4 - 0} = 0;
}

let Predicates = [iHas2E3] in {
  def RFI32 : I_5_XZ_PRIVI<0x11, 0x1, "rfi32">;
  def SCE32 : I_5_IMM5<0x30, 0b000110, 0b00001, "sce32", uimm4, []>;
}
let Predicates = [HasExtendLrw] in
def IDLY32 : I_5_IMM5<0x30, 0b000111, 0b00001, "idly32", imm5_idly, []>;
def STOP32 : I_5_XZ_PRIVI<0x12, 0x1, "stop32">;
def WAIT32 : I_5_XZ_PRIVI<0x13, 0x1, "wait32">;
def DOZE32 : I_5_XZ_PRIVI<0x14, 0x1, "doze32">;
def WE32 : I_5_XZ_PRIVI<0b010101, 0x1, "we32">;
def SE32 : I_5_XZ_PRIVI<0b010110, 0x1, "se32">;
def WSC32 : I_5_XZ_PRIVI<0b001111, 0x1, "wsc32">;

def CPOP32 : I_CPOP<(outs), (ins uimm5:$cpid, uimm20:$usdef), "cpop32 <$cpid, ${usdef}>">;
def CPRC32 : I_CP<0b0100, (outs CARRY:$ca), (ins uimm5:$cpid, uimm12:$usdef), "cprc32 <$cpid, ${usdef}>">;
def CPRCR32 : I_CP_Z<0b0010, (outs GPR:$rz), (ins uimm5:$cpid, uimm12:$usdef), "cprcr32 $rz, <$cpid, ${usdef}>">;
def CPRGR32 : I_CP_Z<0b0000, (outs GPR:$rz), (ins uimm5:$cpid, uimm12:$usdef), "cprgr32 $rz, <$cpid, ${usdef}>">;
def CPWCR32 : I_CP_Z<0b0011, (outs), (ins GPR:$rz, uimm5:$cpid, uimm12:$usdef), "cpwcr32 $rz, <$cpid, ${usdef}>">;
def CPWGR32 : I_CP_Z<0b0001, (outs), (ins GPR:$rz, uimm5:$cpid, uimm12:$usdef), "cpwgr32 $rz, <$cpid, ${usdef}>">;

let Predicates = [iHas3r2E3r3] in {
def DCACHE_IALL32 : I_5_CACHE<0b100101, 0b01000, "dcache32.iall">;
def DCACHE_CALL32 : I_5_CACHE<0b100101, 0b00100, "dcache32.call">;
def DCACHE_CIALL32 : I_5_CACHE<0b100101, 0b01100, "dcache32.ciall">;
def DCACHE_IVA32 : I_5_X_CACHE<0b100101, 0b01011, "dcache32.iva">;
def DCACHE_ISW32: I_5_X_CACHE<0b100101, 0b01010, "dcache32.isw">;
def DCACHE_CVA32 : I_5_X_CACHE<0b100101, 0b00111, "dcache32.cva">;
def DCACHE_CVAL32 : I_5_X_CACHE<0b100101, 0b10111, "dcache32.cval1">;
def DCACHE_CSW32 : I_5_X_CACHE<0b100101, 0b00110, "dcache32.csw">;
def DCACHE_CIVA32 : I_5_X_CACHE<0b100101, 0b01111, "dcache32.civa">;
def DCACHE_CISW32 : I_5_X_CACHE<0b100101, 0b01110, "dcache32.cisw">;

def ICACHE_IALL32 : I_5_CACHE<0b100100, 0b01000, "icache32.iall">;
def ICACHE_IALLS32 : I_5_CACHE<0b100100, 0b11000, "icache32.ialls">;
def ICACHE_IVA32 : I_5_X_CACHE<0b100100, 0b01011, "icache32.iva">;

def TLBI_VAA32 : I_5_X_CACHE<0b100010, 0b00010, "tlbi32.vaa">;
def TLBI_VAAS32 : I_5_X_CACHE<0b100010, 0b10010, "tlbi32.vaas">;
def TLBI_ASID32 : I_5_X_CACHE<0b100010, 0b00001, "tlbi32.asid">;
def TLBI_ASIDS32 : I_5_X_CACHE<0b100010, 0b10001, "tlbi32.asids">;
def TLBI_VA32 : I_5_X_CACHE<0b100010, 0b00011, "tlbi32.va">;
def TLBI_VAS32 : I_5_X_CACHE<0b100010, 0b10011, "tlbi32.vas">;
def TLBI_ALL32 : I_5_CACHE<0b100010, 0b00000, "tlbi32.all">;
def TLBI_ALLS32 : I_5_CACHE<0b100010, 0b10000, "tlbi32.alls">;

def L2CACHE_IALL : I_5_CACHE<0b100110, 0b01000, "l2cache.iall">;
def L2CACHE_CALL : I_5_CACHE<0b100110, 0b00100, "l2cache.call">;
def L2CACHE_CIALL : I_5_CACHE<0b100110, 0b01100, "l2cache.ciall">;
}

def PLDR32 :I_PLDR<AddrMode32WD, 0x36, 0b0110, (outs), (ins GPR:$rx, uimm12_2:$imm12), "pldr32", []>;
def PLDW32 :I_PLDR<AddrMode32WD, 0x37, 0b0110, (outs), (ins GPR:$rx, uimm12_2:$imm12), "pldw32", []>;

def TRAP32 : CSKY32Inst<AddrModeNone, 0x30, (outs), (ins uimm2:$imm2), "trap32 ${imm2}", []> {
  bits<2> imm2;

  let Inst{25 - 21} = 0;
  let Inst{20 - 16} = 0;
  let Inst{15 - 12} = 0b0010;
  let Inst{11 - 10} = imm2;
  let Inst{9 - 5} = 0b00001;
  let Inst{4 - 0} = 0;

}


//===----------------------------------------------------------------------===//
// Pseudo for assembly
//===----------------------------------------------------------------------===//

let isCall = 1, Defs = [ R15 ], mayLoad = 1, Size = 4, isCodeGenOnly = 0 in
def JBSR32 : CSKYPseudo<(outs), (ins call_symbol:$src1), "jbsr32\t$src1", []>;

def : InstAlias<"jbsr\t$src1", (JBSR32 call_symbol:$src1)>;

def JBR32 : CSKYPseudo<(outs), (ins br_symbol:$src1), "jbr32\t$src1", []> {
  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let isIndirectBranch = 1;
  let mayLoad = 1;
  let Size = 4;
}

def JBT32 : CSKYPseudo<(outs), (ins CARRY:$ca, br_symbol:$src1), "jbt32\t$src1", []> {
  let isBranch = 1;
  let isTerminator = 1;
  let isIndirectBranch = 1;
  let mayLoad = 1;
  let Size = 4;
}

def JBF32 : CSKYPseudo<(outs), (ins CARRY:$ca, br_symbol:$src1), "jbf32\t$src1", []> {
  let isBranch = 1;
  let isTerminator = 1;
  let isIndirectBranch = 1;
  let mayLoad = 1;
  let Size = 4;
}

def JBT_E : CSKYPseudo<(outs), (ins CARRY:$ca, br_symbol:$src1), "!jbt_e\t$src1", []> {
  let isBranch = 1;
  let isTerminator = 1;
  let isIndirectBranch = 1;
  let mayLoad = 1;
  let Size = 6;
}

def JBF_E : CSKYPseudo<(outs), (ins CARRY:$ca, br_symbol:$src1), "!jbf_e\t$src1", []> {
  let isBranch = 1;
  let isTerminator = 1;
  let isIndirectBranch = 1;
  let mayLoad = 1;
  let Size = 6;
}

let mayLoad = 1, Size = 2, isCodeGenOnly = 0 in
def PseudoLRW32 : CSKYPseudo<(outs GPR:$rz), (ins bare_symbol:$src), "lrw32 $rz, $src", []>;


def : InstAlias<"lrw $rz, $src", (PseudoLRW32 GPR:$rz, bare_symbol:$src)>;
def : InstAlias<"lrw $rz, $src", (LRW32 GPR:$rz, constpool_symbol:$src)>;

let mayLoad = 1, Size = 4, isCodeGenOnly = 0 in
def PseudoJSRI32 : CSKYPseudo<(outs), (ins call_symbol:$src), "jsri32 $src", []>;
def : InstAlias<"jsri $dst", (PseudoJSRI32 call_symbol:$dst)>;
def : InstAlias<"jsri $dst", (JSRI32 constpool_symbol:$dst)>;

let mayLoad = 1, Size = 4, isCodeGenOnly = 0 in
def PseudoJMPI32 : CSKYPseudo<(outs), (ins br_symbol:$src), "jmpi32 $src", []>;
def : InstAlias<"jmpi $dst", (PseudoJMPI32 br_symbol:$dst)>;
def : InstAlias<"jmpi $dst", (JMPI32 constpool_symbol:$dst)>;

let isNotDuplicable = 1, mayLoad = 1, mayStore = 0, Size = 8 in
def PseudoTLSLA32 : CSKYPseudo<(outs GPR:$dst1, GPR:$dst2),
  (ins constpool_symbol:$src, i32imm:$label), "!tlslrw32\t$dst1, $dst2, $src, $label", []>;

let hasSideEffects = 0, isNotDuplicable = 1 in
def CONSTPOOL_ENTRY : CSKYPseudo<(outs),
  (ins i32imm:$instid, i32imm:$cpidx, i32imm:$size), "", []>;