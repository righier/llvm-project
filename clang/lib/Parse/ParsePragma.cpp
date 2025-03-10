//===--- ParsePragma.cpp - Language specific pragma parsing ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the language specific #pragma handlers.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/Basic/PragmaKinds.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Token.h"
#include "clang/Parse/LoopHint.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Parse/Parser.h"
#include "clang/Parse/RAIIObjectsForParser.h"
#include "clang/Sema/Scope.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringSwitch.h"
using namespace clang;

namespace {

struct PragmaAlignHandler : public PragmaHandler {
  explicit PragmaAlignHandler() : PragmaHandler("align") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaGCCVisibilityHandler : public PragmaHandler {
  explicit PragmaGCCVisibilityHandler() : PragmaHandler("visibility") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaOptionsHandler : public PragmaHandler {
  explicit PragmaOptionsHandler() : PragmaHandler("options") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaPackHandler : public PragmaHandler {
  explicit PragmaPackHandler() : PragmaHandler("pack") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaClangSectionHandler : public PragmaHandler {
  explicit PragmaClangSectionHandler(Sema &S)
             : PragmaHandler("section"), Actions(S) {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;

private:
  Sema &Actions;
};

struct PragmaMSStructHandler : public PragmaHandler {
  explicit PragmaMSStructHandler() : PragmaHandler("ms_struct") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaUnusedHandler : public PragmaHandler {
  PragmaUnusedHandler() : PragmaHandler("unused") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaWeakHandler : public PragmaHandler {
  explicit PragmaWeakHandler() : PragmaHandler("weak") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaRedefineExtnameHandler : public PragmaHandler {
  explicit PragmaRedefineExtnameHandler() : PragmaHandler("redefine_extname") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaOpenCLExtensionHandler : public PragmaHandler {
  PragmaOpenCLExtensionHandler() : PragmaHandler("EXTENSION") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};


struct PragmaFPContractHandler : public PragmaHandler {
  PragmaFPContractHandler() : PragmaHandler("FP_CONTRACT") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

// Pragma STDC implementations.

/// PragmaSTDC_FENV_ACCESSHandler - "\#pragma STDC FENV_ACCESS ...".
struct PragmaSTDC_FENV_ACCESSHandler : public PragmaHandler {
  PragmaSTDC_FENV_ACCESSHandler() : PragmaHandler("FENV_ACCESS") {}

  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &Tok) override {
    Token PragmaName = Tok;
    if (!PP.getTargetInfo().hasStrictFP() && !PP.getLangOpts().ExpStrictFP) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_fp_ignored)
          << PragmaName.getIdentifierInfo()->getName();
      return;
    }
    tok::OnOffSwitch OOS;
    if (PP.LexOnOffSwitch(OOS))
     return;

    MutableArrayRef<Token> Toks(PP.getPreprocessorAllocator().Allocate<Token>(1),
                                1);
    Toks[0].startToken();
    Toks[0].setKind(tok::annot_pragma_fenv_access);
    Toks[0].setLocation(Tok.getLocation());
    Toks[0].setAnnotationEndLoc(Tok.getLocation());
    Toks[0].setAnnotationValue(reinterpret_cast<void*>(
                               static_cast<uintptr_t>(OOS)));
    PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                        /*IsReinject=*/false);
  }
};

/// PragmaSTDC_CX_LIMITED_RANGEHandler - "\#pragma STDC CX_LIMITED_RANGE ...".
struct PragmaSTDC_CX_LIMITED_RANGEHandler : public PragmaHandler {
  PragmaSTDC_CX_LIMITED_RANGEHandler() : PragmaHandler("CX_LIMITED_RANGE") {}

  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &Tok) override {
    tok::OnOffSwitch OOS;
    PP.LexOnOffSwitch(OOS);
  }
};

/// Handler for "\#pragma STDC FENV_ROUND ...".
struct PragmaSTDC_FENV_ROUNDHandler : public PragmaHandler {
  PragmaSTDC_FENV_ROUNDHandler() : PragmaHandler("FENV_ROUND") {}

  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &Tok) override;
};

/// PragmaSTDC_UnknownHandler - "\#pragma STDC ...".
struct PragmaSTDC_UnknownHandler : public PragmaHandler {
  PragmaSTDC_UnknownHandler() = default;

  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &UnknownTok) override {
    // C99 6.10.6p2, unknown forms are not allowed.
    PP.Diag(UnknownTok, diag::ext_stdc_pragma_ignored);
  }
};

struct PragmaFPHandler : public PragmaHandler {
  PragmaFPHandler() : PragmaHandler("fp") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaNoOpenMPHandler : public PragmaHandler {
  PragmaNoOpenMPHandler() : PragmaHandler("omp") { }
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaOpenMPHandler : public PragmaHandler {
  PragmaOpenMPHandler() : PragmaHandler("omp") { }
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

/// PragmaCommentHandler - "\#pragma comment ...".
struct PragmaCommentHandler : public PragmaHandler {
  PragmaCommentHandler(Sema &Actions)
    : PragmaHandler("comment"), Actions(Actions) {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;

private:
  Sema &Actions;
};

struct PragmaDetectMismatchHandler : public PragmaHandler {
  PragmaDetectMismatchHandler(Sema &Actions)
    : PragmaHandler("detect_mismatch"), Actions(Actions) {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;

private:
  Sema &Actions;
};

struct PragmaFloatControlHandler : public PragmaHandler {
  PragmaFloatControlHandler(Sema &Actions)
      : PragmaHandler("float_control") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaMSPointersToMembers : public PragmaHandler {
  explicit PragmaMSPointersToMembers() : PragmaHandler("pointers_to_members") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaMSVtorDisp : public PragmaHandler {
  explicit PragmaMSVtorDisp() : PragmaHandler("vtordisp") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaMSPragma : public PragmaHandler {
  explicit PragmaMSPragma(const char *name) : PragmaHandler(name) {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

/// PragmaOptimizeHandler - "\#pragma clang optimize on/off".
struct PragmaOptimizeHandler : public PragmaHandler {
  PragmaOptimizeHandler(Sema &S)
    : PragmaHandler("optimize"), Actions(S) {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;

private:
  Sema &Actions;
};

struct PragmaLoopHintHandler : public PragmaHandler {
  PragmaLoopHintHandler() : PragmaHandler("loop") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;

  void HandleLegacySyntax(Preprocessor &PP, PragmaIntroducer Introducer,
                          Token &FirstToken);
  void HandleOmpSyntax(Preprocessor &PP, PragmaIntroducer Introducer,
                       Token &FirstToken);
};

struct PragmaUnrollHintHandler : public PragmaHandler {
  PragmaUnrollHintHandler(const char *name) : PragmaHandler(name) {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaMSRuntimeChecksHandler : public EmptyPragmaHandler {
  PragmaMSRuntimeChecksHandler() : EmptyPragmaHandler("runtime_checks") {}
};

struct PragmaMSIntrinsicHandler : public PragmaHandler {
  PragmaMSIntrinsicHandler() : PragmaHandler("intrinsic") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaMSOptimizeHandler : public PragmaHandler {
  PragmaMSOptimizeHandler() : PragmaHandler("optimize") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

// "\#pragma fenv_access (on)".
struct PragmaMSFenvAccessHandler : public PragmaHandler {
  PragmaMSFenvAccessHandler() : PragmaHandler("fenv_access") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override {
    StringRef PragmaName = FirstToken.getIdentifierInfo()->getName();
    if (!PP.getTargetInfo().hasStrictFP() && !PP.getLangOpts().ExpStrictFP) {
      PP.Diag(FirstToken.getLocation(), diag::warn_pragma_fp_ignored)
          << PragmaName;
      return;
    }

    Token Tok;
    PP.Lex(Tok);
    if (Tok.isNot(tok::l_paren)) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_lparen)
          << PragmaName;
      return;
    }
    PP.Lex(Tok); // Consume the l_paren.
    if (Tok.isNot(tok::identifier)) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_ms_fenv_access);
      return;
    }
    const IdentifierInfo *II = Tok.getIdentifierInfo();
    tok::OnOffSwitch OOS;
    if (II->isStr("on")) {
      OOS = tok::OOS_ON;
      PP.Lex(Tok);
    } else if (II->isStr("off")) {
      OOS = tok::OOS_OFF;
      PP.Lex(Tok);
    } else {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_ms_fenv_access);
      return;
    }
    if (Tok.isNot(tok::r_paren)) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_rparen)
          << PragmaName;
      return;
    }
    PP.Lex(Tok); // Consume the r_paren.

    if (Tok.isNot(tok::eod)) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
          << PragmaName;
      return;
    }

    MutableArrayRef<Token> Toks(
        PP.getPreprocessorAllocator().Allocate<Token>(1), 1);
    Toks[0].startToken();
    Toks[0].setKind(tok::annot_pragma_fenv_access_ms);
    Toks[0].setLocation(FirstToken.getLocation());
    Toks[0].setAnnotationEndLoc(Tok.getLocation());
    Toks[0].setAnnotationValue(
        reinterpret_cast<void*>(static_cast<uintptr_t>(OOS)));
    PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                        /*IsReinject=*/false);
  }
};

struct PragmaForceCUDAHostDeviceHandler : public PragmaHandler {
  PragmaForceCUDAHostDeviceHandler(Sema &Actions)
      : PragmaHandler("force_cuda_host_device"), Actions(Actions) {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;

private:
  Sema &Actions;
};

/// PragmaAttributeHandler - "\#pragma clang attribute ...".
struct PragmaAttributeHandler : public PragmaHandler {
  PragmaAttributeHandler(AttributeFactory &AttrFactory)
      : PragmaHandler("attribute"), AttributesForPragmaAttribute(AttrFactory) {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;

  /// A pool of attributes that were parsed in \#pragma clang attribute.
  ParsedAttributes AttributesForPragmaAttribute;
};

struct PragmaMaxTokensHereHandler : public PragmaHandler {
  PragmaMaxTokensHereHandler() : PragmaHandler("max_tokens_here") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

struct PragmaMaxTokensTotalHandler : public PragmaHandler {
  PragmaMaxTokensTotalHandler() : PragmaHandler("max_tokens_total") {}
  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &FirstToken) override;
};

void markAsReinjectedForRelexing(llvm::MutableArrayRef<clang::Token> Toks) {
  for (auto &T : Toks)
    T.setFlag(clang::Token::IsReinjected);
}
}  // end namespace

void Parser::initializePragmaHandlers() {
  AlignHandler = std::make_unique<PragmaAlignHandler>();
  PP.AddPragmaHandler(AlignHandler.get());

  GCCVisibilityHandler = std::make_unique<PragmaGCCVisibilityHandler>();
  PP.AddPragmaHandler("GCC", GCCVisibilityHandler.get());

  OptionsHandler = std::make_unique<PragmaOptionsHandler>();
  PP.AddPragmaHandler(OptionsHandler.get());

  PackHandler = std::make_unique<PragmaPackHandler>();
  PP.AddPragmaHandler(PackHandler.get());

  MSStructHandler = std::make_unique<PragmaMSStructHandler>();
  PP.AddPragmaHandler(MSStructHandler.get());

  UnusedHandler = std::make_unique<PragmaUnusedHandler>();
  PP.AddPragmaHandler(UnusedHandler.get());

  WeakHandler = std::make_unique<PragmaWeakHandler>();
  PP.AddPragmaHandler(WeakHandler.get());

  RedefineExtnameHandler = std::make_unique<PragmaRedefineExtnameHandler>();
  PP.AddPragmaHandler(RedefineExtnameHandler.get());

  FPContractHandler = std::make_unique<PragmaFPContractHandler>();
  PP.AddPragmaHandler("STDC", FPContractHandler.get());

  STDCFenvAccessHandler = std::make_unique<PragmaSTDC_FENV_ACCESSHandler>();
  PP.AddPragmaHandler("STDC", STDCFenvAccessHandler.get());

  STDCFenvRoundHandler = std::make_unique<PragmaSTDC_FENV_ROUNDHandler>();
  PP.AddPragmaHandler("STDC", STDCFenvRoundHandler.get());

  STDCCXLIMITHandler = std::make_unique<PragmaSTDC_CX_LIMITED_RANGEHandler>();
  PP.AddPragmaHandler("STDC", STDCCXLIMITHandler.get());

  STDCUnknownHandler = std::make_unique<PragmaSTDC_UnknownHandler>();
  PP.AddPragmaHandler("STDC", STDCUnknownHandler.get());

  PCSectionHandler = std::make_unique<PragmaClangSectionHandler>(Actions);
  PP.AddPragmaHandler("clang", PCSectionHandler.get());

  if (getLangOpts().OpenCL) {
    OpenCLExtensionHandler = std::make_unique<PragmaOpenCLExtensionHandler>();
    PP.AddPragmaHandler("OPENCL", OpenCLExtensionHandler.get());

    PP.AddPragmaHandler("OPENCL", FPContractHandler.get());
  }
  if (getLangOpts().OpenMP)
    OpenMPHandler = std::make_unique<PragmaOpenMPHandler>();
  else
    OpenMPHandler = std::make_unique<PragmaNoOpenMPHandler>();
  PP.AddPragmaHandler(OpenMPHandler.get());

  if (getLangOpts().MicrosoftExt ||
      getTargetInfo().getTriple().isOSBinFormatELF()) {
    MSCommentHandler = std::make_unique<PragmaCommentHandler>(Actions);
    PP.AddPragmaHandler(MSCommentHandler.get());
  }

  FloatControlHandler = std::make_unique<PragmaFloatControlHandler>(Actions);
  PP.AddPragmaHandler(FloatControlHandler.get());
  if (getLangOpts().MicrosoftExt) {
    MSDetectMismatchHandler =
        std::make_unique<PragmaDetectMismatchHandler>(Actions);
    PP.AddPragmaHandler(MSDetectMismatchHandler.get());
    MSPointersToMembers = std::make_unique<PragmaMSPointersToMembers>();
    PP.AddPragmaHandler(MSPointersToMembers.get());
    MSVtorDisp = std::make_unique<PragmaMSVtorDisp>();
    PP.AddPragmaHandler(MSVtorDisp.get());
    MSInitSeg = std::make_unique<PragmaMSPragma>("init_seg");
    PP.AddPragmaHandler(MSInitSeg.get());
    MSDataSeg = std::make_unique<PragmaMSPragma>("data_seg");
    PP.AddPragmaHandler(MSDataSeg.get());
    MSBSSSeg = std::make_unique<PragmaMSPragma>("bss_seg");
    PP.AddPragmaHandler(MSBSSSeg.get());
    MSConstSeg = std::make_unique<PragmaMSPragma>("const_seg");
    PP.AddPragmaHandler(MSConstSeg.get());
    MSCodeSeg = std::make_unique<PragmaMSPragma>("code_seg");
    PP.AddPragmaHandler(MSCodeSeg.get());
    MSSection = std::make_unique<PragmaMSPragma>("section");
    PP.AddPragmaHandler(MSSection.get());
    MSRuntimeChecks = std::make_unique<PragmaMSRuntimeChecksHandler>();
    PP.AddPragmaHandler(MSRuntimeChecks.get());
    MSIntrinsic = std::make_unique<PragmaMSIntrinsicHandler>();
    PP.AddPragmaHandler(MSIntrinsic.get());
    MSOptimize = std::make_unique<PragmaMSOptimizeHandler>();
    PP.AddPragmaHandler(MSOptimize.get());
    MSFenvAccess = std::make_unique<PragmaMSFenvAccessHandler>();
    PP.AddPragmaHandler(MSFenvAccess.get());
  }

  if (getLangOpts().CUDA) {
    CUDAForceHostDeviceHandler =
        std::make_unique<PragmaForceCUDAHostDeviceHandler>(Actions);
    PP.AddPragmaHandler("clang", CUDAForceHostDeviceHandler.get());
  }

  OptimizeHandler = std::make_unique<PragmaOptimizeHandler>(Actions);
  PP.AddPragmaHandler("clang", OptimizeHandler.get());

  LoopHintHandler = std::make_unique<PragmaLoopHintHandler>();
  PP.AddPragmaHandler("clang", LoopHintHandler.get());

  UnrollHintHandler = std::make_unique<PragmaUnrollHintHandler>("unroll");
  PP.AddPragmaHandler(UnrollHintHandler.get());
  PP.AddPragmaHandler("GCC", UnrollHintHandler.get());

  NoUnrollHintHandler = std::make_unique<PragmaUnrollHintHandler>("nounroll");
  PP.AddPragmaHandler(NoUnrollHintHandler.get());
  PP.AddPragmaHandler("GCC", NoUnrollHintHandler.get());

  UnrollAndJamHintHandler =
      std::make_unique<PragmaUnrollHintHandler>("unroll_and_jam");
  PP.AddPragmaHandler(UnrollAndJamHintHandler.get());

  NoUnrollAndJamHintHandler =
      std::make_unique<PragmaUnrollHintHandler>("nounroll_and_jam");
  PP.AddPragmaHandler(NoUnrollAndJamHintHandler.get());

  FPHandler = std::make_unique<PragmaFPHandler>();
  PP.AddPragmaHandler("clang", FPHandler.get());

  AttributePragmaHandler =
      std::make_unique<PragmaAttributeHandler>(AttrFactory);
  PP.AddPragmaHandler("clang", AttributePragmaHandler.get());

  MaxTokensHerePragmaHandler = std::make_unique<PragmaMaxTokensHereHandler>();
  PP.AddPragmaHandler("clang", MaxTokensHerePragmaHandler.get());

  MaxTokensTotalPragmaHandler = std::make_unique<PragmaMaxTokensTotalHandler>();
  PP.AddPragmaHandler("clang", MaxTokensTotalPragmaHandler.get());
}

void Parser::resetPragmaHandlers() {
  // Remove the pragma handlers we installed.
  PP.RemovePragmaHandler(AlignHandler.get());
  AlignHandler.reset();
  PP.RemovePragmaHandler("GCC", GCCVisibilityHandler.get());
  GCCVisibilityHandler.reset();
  PP.RemovePragmaHandler(OptionsHandler.get());
  OptionsHandler.reset();
  PP.RemovePragmaHandler(PackHandler.get());
  PackHandler.reset();
  PP.RemovePragmaHandler(MSStructHandler.get());
  MSStructHandler.reset();
  PP.RemovePragmaHandler(UnusedHandler.get());
  UnusedHandler.reset();
  PP.RemovePragmaHandler(WeakHandler.get());
  WeakHandler.reset();
  PP.RemovePragmaHandler(RedefineExtnameHandler.get());
  RedefineExtnameHandler.reset();

  if (getLangOpts().OpenCL) {
    PP.RemovePragmaHandler("OPENCL", OpenCLExtensionHandler.get());
    OpenCLExtensionHandler.reset();
    PP.RemovePragmaHandler("OPENCL", FPContractHandler.get());
  }
  PP.RemovePragmaHandler(OpenMPHandler.get());
  OpenMPHandler.reset();

  if (getLangOpts().MicrosoftExt ||
      getTargetInfo().getTriple().isOSBinFormatELF()) {
    PP.RemovePragmaHandler(MSCommentHandler.get());
    MSCommentHandler.reset();
  }

  PP.RemovePragmaHandler("clang", PCSectionHandler.get());
  PCSectionHandler.reset();

  PP.RemovePragmaHandler(FloatControlHandler.get());
  FloatControlHandler.reset();
  if (getLangOpts().MicrosoftExt) {
    PP.RemovePragmaHandler(MSDetectMismatchHandler.get());
    MSDetectMismatchHandler.reset();
    PP.RemovePragmaHandler(MSPointersToMembers.get());
    MSPointersToMembers.reset();
    PP.RemovePragmaHandler(MSVtorDisp.get());
    MSVtorDisp.reset();
    PP.RemovePragmaHandler(MSInitSeg.get());
    MSInitSeg.reset();
    PP.RemovePragmaHandler(MSDataSeg.get());
    MSDataSeg.reset();
    PP.RemovePragmaHandler(MSBSSSeg.get());
    MSBSSSeg.reset();
    PP.RemovePragmaHandler(MSConstSeg.get());
    MSConstSeg.reset();
    PP.RemovePragmaHandler(MSCodeSeg.get());
    MSCodeSeg.reset();
    PP.RemovePragmaHandler(MSSection.get());
    MSSection.reset();
    PP.RemovePragmaHandler(MSRuntimeChecks.get());
    MSRuntimeChecks.reset();
    PP.RemovePragmaHandler(MSIntrinsic.get());
    MSIntrinsic.reset();
    PP.RemovePragmaHandler(MSOptimize.get());
    MSOptimize.reset();
    PP.RemovePragmaHandler(MSFenvAccess.get());
    MSFenvAccess.reset();
  }

  if (getLangOpts().CUDA) {
    PP.RemovePragmaHandler("clang", CUDAForceHostDeviceHandler.get());
    CUDAForceHostDeviceHandler.reset();
  }

  PP.RemovePragmaHandler("STDC", FPContractHandler.get());
  FPContractHandler.reset();

  PP.RemovePragmaHandler("STDC", STDCFenvAccessHandler.get());
  STDCFenvAccessHandler.reset();

  PP.RemovePragmaHandler("STDC", STDCFenvRoundHandler.get());
  STDCFenvRoundHandler.reset();

  PP.RemovePragmaHandler("STDC", STDCCXLIMITHandler.get());
  STDCCXLIMITHandler.reset();

  PP.RemovePragmaHandler("STDC", STDCUnknownHandler.get());
  STDCUnknownHandler.reset();

  PP.RemovePragmaHandler("clang", OptimizeHandler.get());
  OptimizeHandler.reset();

  PP.RemovePragmaHandler("clang", LoopHintHandler.get());
  LoopHintHandler.reset();

  PP.RemovePragmaHandler(UnrollHintHandler.get());
  PP.RemovePragmaHandler("GCC", UnrollHintHandler.get());
  UnrollHintHandler.reset();

  PP.RemovePragmaHandler(NoUnrollHintHandler.get());
  PP.RemovePragmaHandler("GCC", NoUnrollHintHandler.get());
  NoUnrollHintHandler.reset();

  PP.RemovePragmaHandler(UnrollAndJamHintHandler.get());
  UnrollAndJamHintHandler.reset();

  PP.RemovePragmaHandler(NoUnrollAndJamHintHandler.get());
  NoUnrollAndJamHintHandler.reset();

  PP.RemovePragmaHandler("clang", FPHandler.get());
  FPHandler.reset();

  PP.RemovePragmaHandler("clang", AttributePragmaHandler.get());
  AttributePragmaHandler.reset();

  PP.RemovePragmaHandler("clang", MaxTokensHerePragmaHandler.get());
  MaxTokensHerePragmaHandler.reset();

  PP.RemovePragmaHandler("clang", MaxTokensTotalPragmaHandler.get());
  MaxTokensTotalPragmaHandler.reset();
}

/// Handle the annotation token produced for #pragma unused(...)
///
/// Each annot_pragma_unused is followed by the argument token so e.g.
/// "#pragma unused(x,y)" becomes:
/// annot_pragma_unused 'x' annot_pragma_unused 'y'
void Parser::HandlePragmaUnused() {
  assert(Tok.is(tok::annot_pragma_unused));
  SourceLocation UnusedLoc = ConsumeAnnotationToken();
  Actions.ActOnPragmaUnused(Tok, getCurScope(), UnusedLoc);
  ConsumeToken(); // The argument token.
}

void Parser::HandlePragmaVisibility() {
  assert(Tok.is(tok::annot_pragma_vis));
  const IdentifierInfo *VisType =
    static_cast<IdentifierInfo *>(Tok.getAnnotationValue());
  SourceLocation VisLoc = ConsumeAnnotationToken();
  Actions.ActOnPragmaVisibility(VisType, VisLoc);
}

namespace {
struct PragmaPackInfo {
  Sema::PragmaMsStackAction Action;
  StringRef SlotLabel;
  Token Alignment;
};
} // end anonymous namespace

void Parser::HandlePragmaPack() {
  assert(Tok.is(tok::annot_pragma_pack));
  PragmaPackInfo *Info =
    static_cast<PragmaPackInfo *>(Tok.getAnnotationValue());
  SourceLocation PragmaLoc = Tok.getLocation();
  ExprResult Alignment;
  if (Info->Alignment.is(tok::numeric_constant)) {
    Alignment = Actions.ActOnNumericConstant(Info->Alignment);
    if (Alignment.isInvalid()) {
      ConsumeAnnotationToken();
      return;
    }
  }
  Actions.ActOnPragmaPack(PragmaLoc, Info->Action, Info->SlotLabel,
                          Alignment.get());
  // Consume the token after processing the pragma to enable pragma-specific
  // #include warnings.
  ConsumeAnnotationToken();
}

void Parser::HandlePragmaMSStruct() {
  assert(Tok.is(tok::annot_pragma_msstruct));
  PragmaMSStructKind Kind = static_cast<PragmaMSStructKind>(
      reinterpret_cast<uintptr_t>(Tok.getAnnotationValue()));
  Actions.ActOnPragmaMSStruct(Kind);
  ConsumeAnnotationToken();
}

void Parser::HandlePragmaAlign() {
  assert(Tok.is(tok::annot_pragma_align));
  Sema::PragmaOptionsAlignKind Kind =
    static_cast<Sema::PragmaOptionsAlignKind>(
    reinterpret_cast<uintptr_t>(Tok.getAnnotationValue()));
  Actions.ActOnPragmaOptionsAlign(Kind, Tok.getLocation());
  // Consume the token after processing the pragma to enable pragma-specific
  // #include warnings.
  ConsumeAnnotationToken();
}

void Parser::HandlePragmaDump() {
  assert(Tok.is(tok::annot_pragma_dump));
  IdentifierInfo *II =
      reinterpret_cast<IdentifierInfo *>(Tok.getAnnotationValue());
  Actions.ActOnPragmaDump(getCurScope(), Tok.getLocation(), II);
  ConsumeAnnotationToken();
}

void Parser::HandlePragmaWeak() {
  assert(Tok.is(tok::annot_pragma_weak));
  SourceLocation PragmaLoc = ConsumeAnnotationToken();
  Actions.ActOnPragmaWeakID(Tok.getIdentifierInfo(), PragmaLoc,
                            Tok.getLocation());
  ConsumeToken(); // The weak name.
}

void Parser::HandlePragmaWeakAlias() {
  assert(Tok.is(tok::annot_pragma_weakalias));
  SourceLocation PragmaLoc = ConsumeAnnotationToken();
  IdentifierInfo *WeakName = Tok.getIdentifierInfo();
  SourceLocation WeakNameLoc = Tok.getLocation();
  ConsumeToken();
  IdentifierInfo *AliasName = Tok.getIdentifierInfo();
  SourceLocation AliasNameLoc = Tok.getLocation();
  ConsumeToken();
  Actions.ActOnPragmaWeakAlias(WeakName, AliasName, PragmaLoc,
                               WeakNameLoc, AliasNameLoc);

}

void Parser::HandlePragmaRedefineExtname() {
  assert(Tok.is(tok::annot_pragma_redefine_extname));
  SourceLocation RedefLoc = ConsumeAnnotationToken();
  IdentifierInfo *RedefName = Tok.getIdentifierInfo();
  SourceLocation RedefNameLoc = Tok.getLocation();
  ConsumeToken();
  IdentifierInfo *AliasName = Tok.getIdentifierInfo();
  SourceLocation AliasNameLoc = Tok.getLocation();
  ConsumeToken();
  Actions.ActOnPragmaRedefineExtname(RedefName, AliasName, RedefLoc,
                                     RedefNameLoc, AliasNameLoc);
}

void Parser::HandlePragmaFPContract() {
  assert(Tok.is(tok::annot_pragma_fp_contract));
  tok::OnOffSwitch OOS =
    static_cast<tok::OnOffSwitch>(
    reinterpret_cast<uintptr_t>(Tok.getAnnotationValue()));

  LangOptions::FPModeKind FPC;
  switch (OOS) {
  case tok::OOS_ON:
    FPC = LangOptions::FPM_On;
    break;
  case tok::OOS_OFF:
    FPC = LangOptions::FPM_Off;
    break;
  case tok::OOS_DEFAULT:
    FPC = getLangOpts().getDefaultFPContractMode();
    break;
  }

  SourceLocation PragmaLoc = ConsumeAnnotationToken();
  Actions.ActOnPragmaFPContract(PragmaLoc, FPC);
}

void Parser::HandlePragmaFloatControl() {
  assert(Tok.is(tok::annot_pragma_float_control));

  // The value that is held on the PragmaFloatControlStack encodes
  // the PragmaFloatControl kind and the MSStackAction kind
  // into a single 32-bit word. The MsStackAction is the high 16 bits
  // and the FloatControl is the lower 16 bits. Use shift and bit-and
  // to decode the parts.
  uintptr_t Value = reinterpret_cast<uintptr_t>(Tok.getAnnotationValue());
  Sema::PragmaMsStackAction Action =
      static_cast<Sema::PragmaMsStackAction>((Value >> 16) & 0xFFFF);
  PragmaFloatControlKind Kind = PragmaFloatControlKind(Value & 0xFFFF);
  SourceLocation PragmaLoc = ConsumeAnnotationToken();
  Actions.ActOnPragmaFloatControl(PragmaLoc, Action, Kind);
}

void Parser::HandlePragmaFEnvAccess() {
  assert(Tok.is(tok::annot_pragma_fenv_access) ||
         Tok.is(tok::annot_pragma_fenv_access_ms));
  tok::OnOffSwitch OOS =
    static_cast<tok::OnOffSwitch>(
    reinterpret_cast<uintptr_t>(Tok.getAnnotationValue()));

  bool IsEnabled;
  switch (OOS) {
  case tok::OOS_ON:
    IsEnabled = true;
    break;
  case tok::OOS_OFF:
    IsEnabled = false;
    break;
  case tok::OOS_DEFAULT: // FIXME: Add this cli option when it makes sense.
    IsEnabled = false;
    break;
  }

  SourceLocation PragmaLoc = ConsumeAnnotationToken();
  Actions.ActOnPragmaFEnvAccess(PragmaLoc, IsEnabled);
}

void Parser::HandlePragmaFEnvRound() {
  assert(Tok.is(tok::annot_pragma_fenv_round));
  auto RM = static_cast<llvm::RoundingMode>(
      reinterpret_cast<uintptr_t>(Tok.getAnnotationValue()));

  SourceLocation PragmaLoc = ConsumeAnnotationToken();
  Actions.setRoundingMode(PragmaLoc, RM);
}

StmtResult Parser::HandlePragmaCaptured()
{
  assert(Tok.is(tok::annot_pragma_captured));
  ConsumeAnnotationToken();

  if (Tok.isNot(tok::l_brace)) {
    PP.Diag(Tok, diag::err_expected) << tok::l_brace;
    return StmtError();
  }

  SourceLocation Loc = Tok.getLocation();

  ParseScope CapturedRegionScope(this, Scope::FnScope | Scope::DeclScope |
                                           Scope::CompoundStmtScope);
  Actions.ActOnCapturedRegionStart(Loc, getCurScope(), CR_Default,
                                   /*NumParams=*/1);

  StmtResult R = ParseCompoundStatement();
  CapturedRegionScope.Exit();

  if (R.isInvalid()) {
    Actions.ActOnCapturedRegionError();
    return StmtError();
  }

  return Actions.ActOnCapturedRegionEnd(R.get());
}

namespace {
  enum OpenCLExtState : char {
    Disable, Enable, Begin, End
  };
  typedef std::pair<const IdentifierInfo *, OpenCLExtState> OpenCLExtData;
}

void Parser::HandlePragmaOpenCLExtension() {
  assert(Tok.is(tok::annot_pragma_opencl_extension));
  OpenCLExtData *Data = static_cast<OpenCLExtData*>(Tok.getAnnotationValue());
  auto State = Data->second;
  auto Ident = Data->first;
  SourceLocation NameLoc = Tok.getLocation();
  ConsumeAnnotationToken();

  auto &Opt = Actions.getOpenCLOptions();
  auto Name = Ident->getName();
  // OpenCL 1.1 9.1: "The all variant sets the behavior for all extensions,
  // overriding all previously issued extension directives, but only if the
  // behavior is set to disable."
  if (Name == "all") {
    if (State == Disable)
      Opt.disableAll();
    else
      PP.Diag(NameLoc, diag::warn_pragma_expected_predicate) << 1;
  } else if (State == Begin) {
    if (!Opt.isKnown(Name) || !Opt.isSupported(Name, getLangOpts())) {
      Opt.support(Name);
      // FIXME: Default behavior of the extension pragma is not defined.
      // Therefore, it should never be added by default.
      Opt.acceptsPragma(Name);
    }
  } else if (State == End) {
    // There is no behavior for this directive. We only accept this for
    // backward compatibility.
  } else if (!Opt.isKnown(Name) || !Opt.isWithPragma(Name))
    PP.Diag(NameLoc, diag::warn_pragma_unknown_extension) << Ident;
  else if (Opt.isSupportedExtension(Name, getLangOpts()))
    Opt.enable(Name, State == Enable);
  else if (Opt.isSupportedCoreOrOptionalCore(Name, getLangOpts()))
    PP.Diag(NameLoc, diag::warn_pragma_extension_is_core) << Ident;
  else
    PP.Diag(NameLoc, diag::warn_pragma_unsupported_extension) << Ident;
}

void Parser::HandlePragmaMSPointersToMembers() {
  assert(Tok.is(tok::annot_pragma_ms_pointers_to_members));
  LangOptions::PragmaMSPointersToMembersKind RepresentationMethod =
      static_cast<LangOptions::PragmaMSPointersToMembersKind>(
          reinterpret_cast<uintptr_t>(Tok.getAnnotationValue()));
  SourceLocation PragmaLoc = ConsumeAnnotationToken();
  Actions.ActOnPragmaMSPointersToMembers(RepresentationMethod, PragmaLoc);
}

void Parser::HandlePragmaMSVtorDisp() {
  assert(Tok.is(tok::annot_pragma_ms_vtordisp));
  uintptr_t Value = reinterpret_cast<uintptr_t>(Tok.getAnnotationValue());
  Sema::PragmaMsStackAction Action =
      static_cast<Sema::PragmaMsStackAction>((Value >> 16) & 0xFFFF);
  MSVtorDispMode Mode = MSVtorDispMode(Value & 0xFFFF);
  SourceLocation PragmaLoc = ConsumeAnnotationToken();
  Actions.ActOnPragmaMSVtorDisp(Action, PragmaLoc, Mode);
}

void Parser::HandlePragmaMSPragma() {
  assert(Tok.is(tok::annot_pragma_ms_pragma));
  // Grab the tokens out of the annotation and enter them into the stream.
  auto TheTokens =
      (std::pair<std::unique_ptr<Token[]>, size_t> *)Tok.getAnnotationValue();
  PP.EnterTokenStream(std::move(TheTokens->first), TheTokens->second, true,
                      /*IsReinject=*/true);
  SourceLocation PragmaLocation = ConsumeAnnotationToken();
  assert(Tok.isAnyIdentifier());
  StringRef PragmaName = Tok.getIdentifierInfo()->getName();
  PP.Lex(Tok); // pragma kind

  // Figure out which #pragma we're dealing with.  The switch has no default
  // because lex shouldn't emit the annotation token for unrecognized pragmas.
  typedef bool (Parser::*PragmaHandler)(StringRef, SourceLocation);
  PragmaHandler Handler = llvm::StringSwitch<PragmaHandler>(PragmaName)
    .Case("data_seg", &Parser::HandlePragmaMSSegment)
    .Case("bss_seg", &Parser::HandlePragmaMSSegment)
    .Case("const_seg", &Parser::HandlePragmaMSSegment)
    .Case("code_seg", &Parser::HandlePragmaMSSegment)
    .Case("section", &Parser::HandlePragmaMSSection)
    .Case("init_seg", &Parser::HandlePragmaMSInitSeg);

  if (!(this->*Handler)(PragmaName, PragmaLocation)) {
    // Pragma handling failed, and has been diagnosed.  Slurp up the tokens
    // until eof (really end of line) to prevent follow-on errors.
    while (Tok.isNot(tok::eof))
      PP.Lex(Tok);
    PP.Lex(Tok);
  }
}

bool Parser::HandlePragmaMSSection(StringRef PragmaName,
                                   SourceLocation PragmaLocation) {
  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(PragmaLocation, diag::warn_pragma_expected_lparen) << PragmaName;
    return false;
  }
  PP.Lex(Tok); // (
  // Parsing code for pragma section
  if (Tok.isNot(tok::string_literal)) {
    PP.Diag(PragmaLocation, diag::warn_pragma_expected_section_name)
        << PragmaName;
    return false;
  }
  ExprResult StringResult = ParseStringLiteralExpression();
  if (StringResult.isInvalid())
    return false; // Already diagnosed.
  StringLiteral *SegmentName = cast<StringLiteral>(StringResult.get());
  if (SegmentName->getCharByteWidth() != 1) {
    PP.Diag(PragmaLocation, diag::warn_pragma_expected_non_wide_string)
        << PragmaName;
    return false;
  }
  int SectionFlags = ASTContext::PSF_Read;
  bool SectionFlagsAreDefault = true;
  while (Tok.is(tok::comma)) {
    PP.Lex(Tok); // ,
    // Ignore "long" and "short".
    // They are undocumented, but widely used, section attributes which appear
    // to do nothing.
    if (Tok.is(tok::kw_long) || Tok.is(tok::kw_short)) {
      PP.Lex(Tok); // long/short
      continue;
    }

    if (!Tok.isAnyIdentifier()) {
      PP.Diag(PragmaLocation, diag::warn_pragma_expected_action_or_r_paren)
          << PragmaName;
      return false;
    }
    ASTContext::PragmaSectionFlag Flag =
      llvm::StringSwitch<ASTContext::PragmaSectionFlag>(
      Tok.getIdentifierInfo()->getName())
      .Case("read", ASTContext::PSF_Read)
      .Case("write", ASTContext::PSF_Write)
      .Case("execute", ASTContext::PSF_Execute)
      .Case("shared", ASTContext::PSF_Invalid)
      .Case("nopage", ASTContext::PSF_Invalid)
      .Case("nocache", ASTContext::PSF_Invalid)
      .Case("discard", ASTContext::PSF_Invalid)
      .Case("remove", ASTContext::PSF_Invalid)
      .Default(ASTContext::PSF_None);
    if (Flag == ASTContext::PSF_None || Flag == ASTContext::PSF_Invalid) {
      PP.Diag(PragmaLocation, Flag == ASTContext::PSF_None
                                  ? diag::warn_pragma_invalid_specific_action
                                  : diag::warn_pragma_unsupported_action)
          << PragmaName << Tok.getIdentifierInfo()->getName();
      return false;
    }
    SectionFlags |= Flag;
    SectionFlagsAreDefault = false;
    PP.Lex(Tok); // Identifier
  }
  // If no section attributes are specified, the section will be marked as
  // read/write.
  if (SectionFlagsAreDefault)
    SectionFlags |= ASTContext::PSF_Write;
  if (Tok.isNot(tok::r_paren)) {
    PP.Diag(PragmaLocation, diag::warn_pragma_expected_rparen) << PragmaName;
    return false;
  }
  PP.Lex(Tok); // )
  if (Tok.isNot(tok::eof)) {
    PP.Diag(PragmaLocation, diag::warn_pragma_extra_tokens_at_eol)
        << PragmaName;
    return false;
  }
  PP.Lex(Tok); // eof
  Actions.ActOnPragmaMSSection(PragmaLocation, SectionFlags, SegmentName);
  return true;
}

bool Parser::HandlePragmaMSSegment(StringRef PragmaName,
                                   SourceLocation PragmaLocation) {
  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(PragmaLocation, diag::warn_pragma_expected_lparen) << PragmaName;
    return false;
  }
  PP.Lex(Tok); // (
  Sema::PragmaMsStackAction Action = Sema::PSK_Reset;
  StringRef SlotLabel;
  if (Tok.isAnyIdentifier()) {
    StringRef PushPop = Tok.getIdentifierInfo()->getName();
    if (PushPop == "push")
      Action = Sema::PSK_Push;
    else if (PushPop == "pop")
      Action = Sema::PSK_Pop;
    else {
      PP.Diag(PragmaLocation,
              diag::warn_pragma_expected_section_push_pop_or_name)
          << PragmaName;
      return false;
    }
    if (Action != Sema::PSK_Reset) {
      PP.Lex(Tok); // push | pop
      if (Tok.is(tok::comma)) {
        PP.Lex(Tok); // ,
        // If we've got a comma, we either need a label or a string.
        if (Tok.isAnyIdentifier()) {
          SlotLabel = Tok.getIdentifierInfo()->getName();
          PP.Lex(Tok); // identifier
          if (Tok.is(tok::comma))
            PP.Lex(Tok);
          else if (Tok.isNot(tok::r_paren)) {
            PP.Diag(PragmaLocation, diag::warn_pragma_expected_punc)
                << PragmaName;
            return false;
          }
        }
      } else if (Tok.isNot(tok::r_paren)) {
        PP.Diag(PragmaLocation, diag::warn_pragma_expected_punc) << PragmaName;
        return false;
      }
    }
  }
  // Grab the string literal for our section name.
  StringLiteral *SegmentName = nullptr;
  if (Tok.isNot(tok::r_paren)) {
    if (Tok.isNot(tok::string_literal)) {
      unsigned DiagID = Action != Sema::PSK_Reset ? !SlotLabel.empty() ?
          diag::warn_pragma_expected_section_name :
          diag::warn_pragma_expected_section_label_or_name :
          diag::warn_pragma_expected_section_push_pop_or_name;
      PP.Diag(PragmaLocation, DiagID) << PragmaName;
      return false;
    }
    ExprResult StringResult = ParseStringLiteralExpression();
    if (StringResult.isInvalid())
      return false; // Already diagnosed.
    SegmentName = cast<StringLiteral>(StringResult.get());
    if (SegmentName->getCharByteWidth() != 1) {
      PP.Diag(PragmaLocation, diag::warn_pragma_expected_non_wide_string)
          << PragmaName;
      return false;
    }
    // Setting section "" has no effect
    if (SegmentName->getLength())
      Action = (Sema::PragmaMsStackAction)(Action | Sema::PSK_Set);
  }
  if (Tok.isNot(tok::r_paren)) {
    PP.Diag(PragmaLocation, diag::warn_pragma_expected_rparen) << PragmaName;
    return false;
  }
  PP.Lex(Tok); // )
  if (Tok.isNot(tok::eof)) {
    PP.Diag(PragmaLocation, diag::warn_pragma_extra_tokens_at_eol)
        << PragmaName;
    return false;
  }
  PP.Lex(Tok); // eof
  Actions.ActOnPragmaMSSeg(PragmaLocation, Action, SlotLabel,
                           SegmentName, PragmaName);
  return true;
}

// #pragma init_seg({ compiler | lib | user | "section-name" [, func-name]} )
bool Parser::HandlePragmaMSInitSeg(StringRef PragmaName,
                                   SourceLocation PragmaLocation) {
  if (getTargetInfo().getTriple().getEnvironment() != llvm::Triple::MSVC) {
    PP.Diag(PragmaLocation, diag::warn_pragma_init_seg_unsupported_target);
    return false;
  }

  if (ExpectAndConsume(tok::l_paren, diag::warn_pragma_expected_lparen,
                       PragmaName))
    return false;

  // Parse either the known section names or the string section name.
  StringLiteral *SegmentName = nullptr;
  if (Tok.isAnyIdentifier()) {
    auto *II = Tok.getIdentifierInfo();
    StringRef Section = llvm::StringSwitch<StringRef>(II->getName())
                            .Case("compiler", "\".CRT$XCC\"")
                            .Case("lib", "\".CRT$XCL\"")
                            .Case("user", "\".CRT$XCU\"")
                            .Default("");

    if (!Section.empty()) {
      // Pretend the user wrote the appropriate string literal here.
      Token Toks[1];
      Toks[0].startToken();
      Toks[0].setKind(tok::string_literal);
      Toks[0].setLocation(Tok.getLocation());
      Toks[0].setLiteralData(Section.data());
      Toks[0].setLength(Section.size());
      SegmentName =
          cast<StringLiteral>(Actions.ActOnStringLiteral(Toks, nullptr).get());
      PP.Lex(Tok);
    }
  } else if (Tok.is(tok::string_literal)) {
    ExprResult StringResult = ParseStringLiteralExpression();
    if (StringResult.isInvalid())
      return false;
    SegmentName = cast<StringLiteral>(StringResult.get());
    if (SegmentName->getCharByteWidth() != 1) {
      PP.Diag(PragmaLocation, diag::warn_pragma_expected_non_wide_string)
          << PragmaName;
      return false;
    }
    // FIXME: Add support for the '[, func-name]' part of the pragma.
  }

  if (!SegmentName) {
    PP.Diag(PragmaLocation, diag::warn_pragma_expected_init_seg) << PragmaName;
    return false;
  }

  if (ExpectAndConsume(tok::r_paren, diag::warn_pragma_expected_rparen,
                       PragmaName) ||
      ExpectAndConsume(tok::eof, diag::warn_pragma_extra_tokens_at_eol,
                       PragmaName))
    return false;

  Actions.ActOnPragmaMSInitSeg(PragmaLocation, SegmentName);
  return true;
}

namespace {
struct PragmaLoopHintInfo {
  Token PragmaName;
  Token Option;
  ArrayRef<Token> Toks;
};
} // end anonymous namespace

static std::string PragmaLoopHintString(Token PragmaName, Token Option) {
  StringRef Str = PragmaName.getIdentifierInfo()->getName();
  std::string ClangLoopStr = (llvm::Twine("clang loop ") + Str).str();
  return std::string(llvm::StringSwitch<StringRef>(Str)
                         .Case("loop", ClangLoopStr)
                         .Case("unroll_and_jam", Str)
                         .Case("unroll", Str)
                         .Default(""));
}

bool Parser::HandlePragmaLoopHint(LoopHint &Hint) {
  assert(Tok.is(tok::annot_pragma_loop_hint));
  PragmaLoopHintInfo *Info =
      static_cast<PragmaLoopHintInfo *>(Tok.getAnnotationValue());

  IdentifierInfo *PragmaNameInfo = Info->PragmaName.getIdentifierInfo();
  Hint.PragmaNameLoc = IdentifierLoc::create(
      Actions.Context, Info->PragmaName.getLocation(), PragmaNameInfo);

  // It is possible that the loop hint has no option identifier, such as
  // #pragma unroll(4).
  IdentifierInfo *OptionInfo = Info->Option.is(tok::identifier)
                                   ? Info->Option.getIdentifierInfo()
                                   : nullptr;
  Hint.OptionLoc = IdentifierLoc::create(
      Actions.Context, Info->Option.getLocation(), OptionInfo);

  llvm::ArrayRef<Token> Toks = Info->Toks;

  // Return a valid hint if pragma unroll or nounroll were specified
  // without an argument.
  auto IsLoopHint = llvm::StringSwitch<bool>(PragmaNameInfo->getName())
                        .Cases("unroll", "nounroll", "unroll_and_jam",
                               "nounroll_and_jam", true)
                        .Default(false);

  if (Toks.empty() && IsLoopHint) {
    ConsumeAnnotationToken();
    Hint.Range = Info->PragmaName.getLocation();
    return true;
  }

  // The constant expression is always followed by an eof token, which increases
  // the TokSize by 1.
  assert(!Toks.empty() &&
         "PragmaLoopHintInfo::Toks must contain at least one token.");

  // If no option is specified the argument is assumed to be a constant expr.
  bool OptionUnroll = false;
  bool OptionUnrollAndJam = false;
  bool OptionDistribute = false;
  bool OptionPipelineDisabled = false;
  bool StateOption = false;
  if (OptionInfo) { // Pragma Unroll does not specify an option.
    OptionUnroll = OptionInfo->isStr("unroll");
    OptionUnrollAndJam = OptionInfo->isStr("unroll_and_jam");
    OptionDistribute = OptionInfo->isStr("distribute");
    OptionPipelineDisabled = OptionInfo->isStr("pipeline");
    StateOption = llvm::StringSwitch<bool>(OptionInfo->getName())
                      .Case("vectorize", true)
                      .Case("interleave", true)
                      .Case("vectorize_predicate", true)
                      .Default(false) ||
                  OptionUnroll || OptionUnrollAndJam || OptionDistribute ||
                  OptionPipelineDisabled;
  }

  bool AssumeSafetyArg = !OptionUnroll && !OptionUnrollAndJam &&
                         !OptionDistribute && !OptionPipelineDisabled;
  // Verify loop hint has an argument.
  if (Toks[0].is(tok::eof)) {
    ConsumeAnnotationToken();
    Diag(Toks[0].getLocation(), diag::err_pragma_loop_missing_argument)
        << /*StateArgument=*/StateOption
        << /*FullKeyword=*/(OptionUnroll || OptionUnrollAndJam)
        << /*AssumeSafetyKeyword=*/AssumeSafetyArg;
    return false;
  }

  // Validate the argument.
  if (StateOption) {
    ConsumeAnnotationToken();
    SourceLocation StateLoc = Toks[0].getLocation();
    IdentifierInfo *StateInfo = Toks[0].getIdentifierInfo();

    bool Valid = StateInfo &&
                 llvm::StringSwitch<bool>(StateInfo->getName())
                     .Case("disable", true)
                     .Case("enable", !OptionPipelineDisabled)
                     .Case("full", OptionUnroll || OptionUnrollAndJam)
                     .Case("assume_safety", AssumeSafetyArg)
                     .Default(false);
    if (!Valid) {
      if (OptionPipelineDisabled) {
        Diag(Toks[0].getLocation(), diag::err_pragma_pipeline_invalid_keyword);
      } else {
        Diag(Toks[0].getLocation(), diag::err_pragma_invalid_keyword)
            << /*FullKeyword=*/(OptionUnroll || OptionUnrollAndJam)
            << /*AssumeSafetyKeyword=*/AssumeSafetyArg;
      }
      return false;
    }
    if (Toks.size() > 2)
      Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
          << PragmaLoopHintString(Info->PragmaName, Info->Option);
    Hint.StateLoc = IdentifierLoc::create(Actions.Context, StateLoc, StateInfo);
  } else if (OptionInfo && OptionInfo->getName() == "vectorize_width") {
    PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/false,
                        /*IsReinject=*/false);
    ConsumeAnnotationToken();

    SourceLocation StateLoc = Toks[0].getLocation();
    IdentifierInfo *StateInfo = Toks[0].getIdentifierInfo();
    StringRef IsScalableStr = StateInfo ? StateInfo->getName() : "";

    // Look for vectorize_width(fixed|scalable)
    if (IsScalableStr == "scalable" || IsScalableStr == "fixed") {
      PP.Lex(Tok); // Identifier

      if (Toks.size() > 2) {
        Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
            << PragmaLoopHintString(Info->PragmaName, Info->Option);
        while (Tok.isNot(tok::eof))
          ConsumeAnyToken();
      }

      Hint.StateLoc =
          IdentifierLoc::create(Actions.Context, StateLoc, StateInfo);

      ConsumeToken(); // Consume the constant expression eof terminator.
    } else {
      // Enter constant expression including eof terminator into token stream.
      ExprResult R = ParseConstantExpression();

      if (R.isInvalid() && !Tok.is(tok::comma))
        Diag(Toks[0].getLocation(),
             diag::note_pragma_loop_invalid_vectorize_option);

      bool Arg2Error = false;
      if (Tok.is(tok::comma)) {
        PP.Lex(Tok); // ,

        StateInfo = Tok.getIdentifierInfo();
        IsScalableStr = StateInfo->getName();

        if (IsScalableStr != "scalable" && IsScalableStr != "fixed") {
          Diag(Tok.getLocation(),
               diag::err_pragma_loop_invalid_vectorize_option);
          Arg2Error = true;
        } else
          Hint.StateLoc =
              IdentifierLoc::create(Actions.Context, StateLoc, StateInfo);

        PP.Lex(Tok); // Identifier
      }

      // Tokens following an error in an ill-formed constant expression will
      // remain in the token stream and must be removed.
      if (Tok.isNot(tok::eof)) {
        Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
            << PragmaLoopHintString(Info->PragmaName, Info->Option);
        while (Tok.isNot(tok::eof))
          ConsumeAnyToken();
      }

      ConsumeToken(); // Consume the constant expression eof terminator.

      if (Arg2Error || R.isInvalid() ||
          Actions.CheckLoopHintExpr(R.get(), Toks[0].getLocation()))
        return false;

      // Argument is a constant expression with an integer type.
      Hint.ValueExpr = R.get();
    }
  } else {
    // Enter constant expression including eof terminator into token stream.
    PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/false,
                        /*IsReinject=*/false);
    ConsumeAnnotationToken();
    ExprResult R = ParseConstantExpression();

    // Tokens following an error in an ill-formed constant expression will
    // remain in the token stream and must be removed.
    if (Tok.isNot(tok::eof)) {
      Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
          << PragmaLoopHintString(Info->PragmaName, Info->Option);
      while (Tok.isNot(tok::eof))
        ConsumeAnyToken();
    }

    ConsumeToken(); // Consume the constant expression eof terminator.

    if (R.isInvalid() ||
        Actions.CheckLoopHintExpr(R.get(), Toks[0].getLocation()))
      return false;

    // Argument is a constant expression with an integer type.
    Hint.ValueExpr = R.get();
  }

  Hint.Range = SourceRange(Info->PragmaName.getLocation(),
                           Info->Toks.back().getLocation());
  return true;
}

static ExprResult parseExpression(Preprocessor &PP, Parser &Parse, Token &Tok,
                                  ArrayRef<Token> Toks, int &Count,
                                  bool ExpectComma) {
  // TODO: Use BalancedDelimiterTracker
  auto NumOpenParens = 0;
  int i = 0;
  while (true) {
    if (i >= Toks.size())
      break;
    auto &Tok = Toks[i];
    if (Tok.is(tok::l_paren))
      NumOpenParens += 1;
    else if (Tok.is(tok::r_paren)) {
      if (NumOpenParens <= 0)
        break;
      NumOpenParens -= 1;
    } else if (ExpectComma && Tok.is(tok::comma))
      break;
    i += 1;
  }
  Count += i;

  auto ClauseValue = Toks.slice(0, i);

  // Token Annotation;
  // PP.Lex(Annotation);

#if 0
	Token MyToks[4];
	for (int j = 0; j < 4; j+=1)
		PP.Lex(MyToks[j]);
	PP.EnterTokenStream(MyToks, true);

	for (int j = 0; j < 4; j+=1)
		PP.Lex(MyToks[j]);
	PP.EnterTokenStream(MyToks, true);
#endif

  SourceLocation AfterEndLoc;
  if (i < Toks.size()) {
    AfterEndLoc = Toks[i].getLocation();
  } else {
    Token AfterAnnotation;
    PP.Lex(AfterAnnotation);
    AfterEndLoc = AfterAnnotation.getLocation();
    PP.EnterTokenStream(AfterAnnotation, true, /*IsReinject=*/true);
  }

  // Push back end marker that does not get accidentally consumed.
  Token Eof;
  Eof.startToken();
  Eof.setKind(tok::eod);
  Eof.setLocation(AfterEndLoc);
  PP.EnterTokenStream(Eof, true, /*IsReinject=*/false);

  // Push back the tokens on the stack so we can parse them.
  PP.EnterTokenStream(ClauseValue, /*DisableMacroExpansion=*/true,
                      /*IsReinject*/ true);

  // Save current Parser.Tok to restore later.
  Token Annotation = Tok;

  // ParseConstantExpression() takes Parser.Tok as first token.
  PP.Lex(Tok);

  ExprResult R = Parse.ParseConstantExpression();

  // Restore state
  if (Tok.isNot(tok::eod))
    PP.DiscardUntilEndOfDirective();
  Tok = Annotation;
#if 0
	Token MyToks[4];
	for (int j = 0; j < 4; j+=1)
		PP.Lex(MyToks[j]);
	PP.EnterTokenStream(MyToks, true);
#endif
  return R;
}

enum class TransformClauseKind {
  None,
  ReversedId,    // reverse
  Sizes,         // tile
  Permutation,   // interchange
  PermutedIds,   // interchange
  Array,         // pack
  FloorIds,      // tile
  TileIds,       // tile
  Peel,          // tile
  Allocate,      // pack
  IslSize,       // pack
  IslRedirect,   // pack
  Factor,        // unrolling, unrollingandjam
  Full,          // unrolling, unrollingandjam
  UnrolledIds,   // unrollingandjam
  Autofission,   // fission
  SplitAt,       // fission
  FissionendIds, // fission
  FusedId,       // fusion
};

// TODO: Introduce enum for clause names
static TransformClauseKind parseNextClause(Preprocessor &PP, Parser &Parse,
                                           Token &Tok, ArrayRef<Token> Toks,
                                           int &i,
                                           SmallVectorImpl<ArgsUnion> &Args) {
  auto &ClauseTok = Toks[i];
  if (ClauseTok.is(tok::eof))
    return TransformClauseKind::None;

  assert(ClauseTok.is(tok::identifier));
  auto ClauseName = ClauseTok.getIdentifierInfo()->getName();

  auto Kind = llvm::StringSwitch<TransformClauseKind>(ClauseName)
                  .Case("reversed_id", TransformClauseKind::ReversedId)
                  .Case("sizes", TransformClauseKind::Sizes)
                  .Case("permutation", TransformClauseKind::Permutation)
                  .Case("permuted_ids", TransformClauseKind::PermutedIds)
                  .Case("array", TransformClauseKind::Array)
                  .Case("floor_ids", TransformClauseKind::FloorIds)
                  .Case("tile_ids", TransformClauseKind::TileIds)
                  .Case("peel", TransformClauseKind::Peel)
                  .Case("allocate", TransformClauseKind::Allocate)
                  .Case("isl_size", TransformClauseKind::IslSize)
                  .Case("isl_redirect", TransformClauseKind::IslRedirect)
                  .Case("factor", TransformClauseKind::Factor)
                  .Case("full", TransformClauseKind::Full)
                  .Case("unrolled_ids", TransformClauseKind::UnrolledIds)
                  .Case("autofission", TransformClauseKind::Autofission)
                  .Case("split_at", TransformClauseKind::SplitAt)
                  .Case("fissioned_ids", TransformClauseKind::FissionendIds)
                  .Case("fused_id", TransformClauseKind::FusedId)
                  .Default(TransformClauseKind::None);

  switch (Kind) {
  case TransformClauseKind::ReversedId:
  case TransformClauseKind::FusedId: {
    i += 1;

    assert(Toks[i].is(tok::l_paren));
    i += 1;

    auto LoopIdInfo = Toks[i].getIdentifierInfo();
    auto LoopIdStr = LoopIdInfo->getName();
    Args.push_back(IdentifierLoc::create(Parse.getActions().getASTContext(),
                                         Toks[i].getLocation(), LoopIdInfo));
    i += 1;

    assert(Toks[i].is(tok::r_paren));
    i += 1;
    return Kind;
  } break;
  case TransformClauseKind::Sizes:
  case TransformClauseKind::SplitAt: {
    assert(Toks[i + 1].is(tok::l_paren));
    i += 2;

    // Get option value
    // TODO: Use BalancedDelimiterTracker
    auto NumOpenParens = 1;
    auto StartInner = i;
    while (NumOpenParens > 0) {
      auto &Tok = Toks[i];
      assert(Tok.isNot(tok::eof));
      if (Tok.is(tok::l_paren))
        NumOpenParens += 1;
      else if (Tok.is(tok::r_paren))
        NumOpenParens -= 1;
      i += 1;
    }
    auto ClauseParens = Toks.slice(StartInner - 1, i - StartInner + 1);
    auto ClauseValue = Toks.slice(StartInner, i - StartInner - 1);

    // Push back the tokens on the stack so we can parse them
    PP.EnterTokenStream(ClauseParens.slice(1), /*DisableMacroExpansion=*/true,
                        /*IsReinject=*/true);

    // Update token stream; current token could be an annotation token or a
    // closing paren.
    PP.Lex(Tok);

    while (true) {
      ExprResult R = Parse.ParseConstantExpression();
      assert(!R.isInvalid());
      Args.push_back(R.get());

      if (Tok.is(tok::comma)) {
        PP.Lex(Tok);
        continue;
      }
      if (Tok.is(tok::r_paren)) // FIXME: Maybe use eod token to be sure the we
                                // don't hit a nested rparen
        break;
      llvm_unreachable("Unexpected token");
    }
    return Kind;
  } break;

  case TransformClauseKind::FloorIds:
  case TransformClauseKind::TileIds:
  case TransformClauseKind::Permutation:
  case TransformClauseKind::PermutedIds:
  case TransformClauseKind::FissionendIds:
  case TransformClauseKind::UnrolledIds: {
    assert(Toks[i + 1].is(tok::l_paren));
    i += 2;
    while (true) {
      assert(Toks[i].is(tok::identifier));
      auto LoopIdInfo = Toks[i].getIdentifierInfo();
      auto LoopIdStr = LoopIdInfo->getName();
      Args.push_back(IdentifierLoc::create(Parse.getActions().getASTContext(),
                                           Toks[i].getLocation(), LoopIdInfo));

      i += 1;
      if (Toks[i].is(tok::comma)) {
        i += 1;
        continue;
      } else if (Toks[i].is(tok::r_paren)) {
        i += 1;
        break;
      }
      llvm_unreachable("unexpected token");
    }
    return Kind;
  } break;

  case TransformClauseKind::Array: {
    assert(Toks[i + 1].is(tok::l_paren));
    assert(Toks[i + 2].is(tok::identifier));
    assert(Toks[i + 3].is(tok::r_paren));
    auto ClauseSlice = Toks.slice(i, 4);
    i += 4;

    // Push identifier on main stack to be parsed
    PP.EnterTokenStream(ClauseSlice.slice(2), /*DisableMacroExpansion=*/true,
                        /*IsReinject=*/true);

    // Push an end marker to the token stream
    // Token EndMarker;
    //  EndMarker.startToken();
    // EndMarker.setKind(tok::eod);
    //  PP.EnterTokenStream(EndMarker,false);

    // Update token stream; current token could be an annotation token from when
    // the #pragma started or a closing paren from the previous clause
    PP.Lex(Tok);

    ExprResult VarExpr = Parse.getActions().CorrectDelayedTyposInExpr(
        Parse.ParseAssignmentExpression());
    assert(VarExpr.isUsable());
    auto V = cast<DeclRefExpr>(VarExpr.get());

    Args.push_back(V);
    return TransformClauseKind::Array;
  } break;

  case TransformClauseKind::Allocate:
  case TransformClauseKind::Peel: {
    assert(Toks[i + 1].is(tok::l_paren));
    assert(Toks[i + 2].is(tok::identifier));
    assert(Toks[i + 3].is(tok::r_paren));

    auto OptionInfo = Toks[i + 2].getIdentifierInfo();
    auto OptionStr = OptionInfo->getName();
    // assert(OptionStr == "malloc");
    Args.push_back(IdentifierLoc::create(Parse.getActions().getASTContext(),
                                         Toks[i].getLocation(), OptionInfo));

    i += 4;
    return Kind;
  } break;

  case TransformClauseKind::IslSize:
  case TransformClauseKind::IslRedirect: {
    i += 1;
    assert(Toks[i].is(tok::l_paren));
    i += 1;
    int Count = 0;
    auto Expr = parseExpression(PP, Parse, Tok, Toks.slice(i), Count, false);
    assert(Expr.isUsable());
    // assert(Expr.get()->getType().isConstant());

    i += Count;
    assert(Toks[i].is(tok::r_paren));

    Args.push_back(Expr.get());

    i += 1;
    return Kind;
  } break;

  case TransformClauseKind::Factor: {
    assert(Toks[i + 1].is(tok::l_paren));
    i += 2;

    // Get option value
    auto NumOpenParens = 1;
    auto StartInner = i;
    while (NumOpenParens > 0) {
      auto &Tok = Toks[i];
      assert(Tok.isNot(tok::eof));
      if (Tok.is(tok::l_paren))
        NumOpenParens += 1;
      else if (Tok.is(tok::r_paren))
        NumOpenParens -= 1;
      i += 1;
    }
    auto ClauseParens = Toks.slice(StartInner - 1, i - StartInner + 1);
    auto ClauseValue = Toks.slice(StartInner, i - StartInner - 1);

    // Push back the tokens on the stack so we can parse them
    PP.EnterTokenStream(ClauseParens.slice(1), /*DisableMacroExpansion=*/true,
                        /*IsReinject=*/true);

    // Update token stream; current token could be an annotation token or a
    // closing paren.
    PP.Lex(Tok);

    ExprResult R = Parse.ParseConstantExpression();
    assert(!R.isInvalid());
    assert(Tok.is(tok::r_paren)); // Closing paren of factor(
    Args.push_back(R.get());      // The factor

    return TransformClauseKind::Factor;
  } break;

  case TransformClauseKind::Full:
  case TransformClauseKind::Autofission: {
    assert(!Toks[i + 1].is(tok::l_paren)); // No arguments
    auto OptionInfo = Toks[i].getIdentifierInfo();
    auto OptionStr = OptionInfo->getName();

    // Use the keyword itself as "argument".
    Args.push_back(IdentifierLoc::create(Parse.getActions().getASTContext(),
                                         Toks[i].getLocation(), OptionInfo));

    i += 1;
    return Kind;
  } break;

  case TransformClauseKind::None:
    llvm_unreachable("Unknown clause");
  }
  llvm_unreachable("Unknown clause");
}

bool Parser::HandlePragmaLoopTransform(IdentifierLoc *&PragmaNameLoc,
                                       SourceRange &Range,
                                       SmallVectorImpl<ArgsUnion> &ArgHints) {
  assert(Tok.is(tok::annot_pragma_loop_transform));
  assert(ArgHints.size() == 0);
  PragmaLoopHintInfo *Info =
      static_cast<PragmaLoopHintInfo *>(Tok.getAnnotationValue());

  auto &Toks = Info->Toks;

  int i = 0;
  auto &LoopTok = Toks[i];
  assert(LoopTok.is(tok::identifier));
  assert(LoopTok.getIdentifierInfo()->getName() == "loop");
  i += 1;

  // IdentifierLoc *ApplyOnLoc=nullptr;

  // Parse loop name this applies to.
  // SmallVector<StringRef,4> ApplyOns;
  SmallVector<IdentifierLoc *, 4> ApplyOnLocs;
  Expr *ApplyOnFollowing = nullptr;
  // StringRef ApplyOn;
  if (Toks[i].is(tok::l_paren)) {
    i += 1;

    auto &LoopCountTok = Toks[i];
    if (LoopCountTok.is(
            tok::numeric_constant)) { // TODO: Allow arbitrary expressions.
      int Count = 0;
      auto R = parseExpression(PP, *this, Tok, Toks.slice(i), Count, false);
      assert(R.isUsable());
      ApplyOnFollowing = R.get();

      i += Count;
      assert(Toks[i].is(tok::r_paren));
      i += 1;
    } else {
      while (true) {
        auto &LoopNameTok = Toks[i];
        assert(LoopNameTok.is(tok::identifier));
        auto ApplyOnLoc =
            IdentifierLoc::create(Actions.Context, LoopNameTok.getLocation(),
                                  LoopNameTok.getIdentifierInfo());
        // auto ApplyOn = LoopNameTok.getIdentifierInfo()->getName();

        ApplyOnLocs.push_back(ApplyOnLoc);

        auto &RParTok = Toks[i + 1];
        if (RParTok.is(tok::r_paren)) {
          i += 2;
          break;
        }

        if (RParTok.is(tok::comma)) {
          i += 2;
          continue;
        }

        llvm_unreachable("unexpected token");
      }
    }
  }

  auto &IdTok = Toks[i];
  assert(IdTok.is(tok::identifier));
  PragmaNameLoc = IdentifierLoc::create(Actions.Context, IdTok.getLocation(),
                                        IdTok.getIdentifierInfo());
  i += 1;
  Range = SourceRange(IdTok.getLocation(), IdTok.getLocation());

  if (IdTok.getIdentifierInfo()->getName() == "id") {
    assert(ApplyOnLocs.empty() && "No id on already named loop");
    assert(!ApplyOnFollowing && "Id always applies to nest loop only");
    auto &LParTok = Toks[i];
    auto &NameTok = Toks[i + 1];
    auto &RParTok = Toks[i + 2];
    auto &EofTok = Toks[i + 3];

    assert(LParTok.is(tok::l_paren));
    assert(NameTok.is(tok::identifier));
    auto LoopId = NameTok.getIdentifierInfo()->getName();
    assert(RParTok.is(tok::r_paren));
    assert(EofTok.is(tok::eof));

    Range = SourceRange(IdTok.getLocation(), RParTok.getLocation());
    // ArgHints.push_back(OptionLoc);
    ArgHints.push_back(IdentifierLoc::create(
        Actions.Context, NameTok.getLocation(), NameTok.getIdentifierInfo()));

    i += 4;
    assert(Toks.size() == i);
    ConsumeAnnotationToken();
    return true;
  }

  if (IdTok.getIdentifierInfo()->getName() == "reverse") {
    // TODO: With ApplyOn, could appear anywhere (in the function?)
    // Use Sema::ActOnXYZ instead of adding a token
    assert(ApplyOnLocs.size() <= 1 && "only single loop supported for reverse");
    assert(!ApplyOnFollowing && "Reverse applies on only one next loop");

    Range = SourceRange(IdTok.getLocation(), IdTok.getLocation());

    assert(ApplyOnLocs.size() <= 1);
    if (ApplyOnLocs.empty())
      // Apply on following loop
      ArgHints.push_back((IdentifierLoc *)nullptr);
    else
      ArgHints.push_back(ApplyOnLocs[0]);

    ArgsUnion ReverseId = (IdentifierLoc *)nullptr;
    while (true) {
      SmallVector<ArgsUnion, 4> ClauseArgs;
      auto Kind = parseNextClause(PP, *this, Tok, Toks, i, ClauseArgs);
      if (Kind == TransformClauseKind::None)
        break;
      switch (Kind) {
      default:
        llvm_unreachable("wrong clause for tile");
      case TransformClauseKind::ReversedId:
        assert(!ReverseId);
        assert(ClauseArgs.size() == 1);
        ReverseId = ClauseArgs[0];
        break;
      }
    }

    ArgHints.push_back(ReverseId);

    auto &EofTok = Toks[i];
    assert(EofTok.is(tok::eof));
    i += 1;

    assert(Toks.size() == i); // Nothing following
    ConsumeAnnotationToken();
    return true;
  }

  if (IdTok.getIdentifierInfo()->getName() == "tile") {
    assert(!ApplyOnFollowing || ApplyOnLocs.empty());
    if (ApplyOnFollowing)
      ArgHints.push_back(ApplyOnFollowing);
    for (auto NameLoc : ApplyOnLocs)
      ArgHints.push_back(NameLoc);
    ArgHints.push_back((IdentifierLoc *)nullptr);

    SmallVector<ArgsUnion, 4> TileSizes;
    SmallVector<ArgsUnion, 4> FloorIds;
    SmallVector<ArgsUnion, 4> TileIds;
    ArgsUnion Peel = (IdentifierLoc *)nullptr;
    while (true) {
      SmallVector<ArgsUnion, 4> ClauseArgs;
      auto Kind = parseNextClause(PP, *this, Tok, Toks, i, ClauseArgs);
      if (Kind == TransformClauseKind::None)
        break;
      switch (Kind) {
      default:
        llvm_unreachable("wrong clause for tile");
      case TransformClauseKind::Sizes:
        assert(!ClauseArgs.empty());
        assert(TileSizes.empty());
        TileSizes = std::move(ClauseArgs);
        break;
      case TransformClauseKind::FloorIds:
        assert(!ClauseArgs.empty());
        assert(FloorIds.empty());
        FloorIds = std::move(ClauseArgs);
        break;
      case TransformClauseKind::TileIds:
        assert(!ClauseArgs.empty());
        assert(TileIds.empty());
        TileIds = std::move(ClauseArgs);
        break;
      case TransformClauseKind::Peel:
        assert(ClauseArgs.size() == 1);
        Peel = ClauseArgs[0];
        break;
      }
    }

    for (auto TileSize : TileSizes)
      ArgHints.push_back(TileSize);
    ArgHints.push_back((Expr *)nullptr);
    for (auto PitId : FloorIds)
      ArgHints.push_back(PitId);
    ArgHints.push_back((IdentifierLoc *)nullptr);
    for (auto TileId : TileIds)
      ArgHints.push_back(TileId);
    ArgHints.push_back((IdentifierLoc *)nullptr);
    ArgHints.push_back(Peel);

    auto &EofTok = Toks[i];
    assert(EofTok.is(tok::eof));
    i += 1;

    assert(Toks.size() == i); // Nothing following
    PP.Lex(Tok);
    return true;
  }

  if (IdTok.getIdentifierInfo()->getName() == "interchange") {
    assert(!ApplyOnFollowing || ApplyOnLocs.empty());
    if (ApplyOnFollowing)
      ArgHints.push_back(ApplyOnFollowing);
    for (auto NameLoc : ApplyOnLocs)
      ArgHints.push_back(NameLoc);
    ArgHints.push_back((IdentifierLoc *)nullptr);

    SmallVector<ArgsUnion, 4> Permutation;
    SmallVector<ArgsUnion, 4> PermutedIds;
    while (true) {
      SmallVector<ArgsUnion, 4> ClauseArgs;
      auto Kind = parseNextClause(PP, *this, Tok, Toks, i, ClauseArgs);
      if (Kind == TransformClauseKind::None)
        break;
      switch (Kind) {
      default:
        llvm_unreachable("unsupported clause for interchange");
      case TransformClauseKind::Permutation:
        assert(!ClauseArgs.empty());
        assert(Permutation.empty());
        Permutation = std::move(ClauseArgs);
        break;
      case TransformClauseKind::PermutedIds:
        assert(!ClauseArgs.empty());
        assert(PermutedIds.empty());
        PermutedIds = std::move(ClauseArgs);
        break;
      }
    }

    for (auto PermuteId : Permutation)
      ArgHints.push_back(PermuteId);
    ArgHints.push_back((IdentifierLoc *)nullptr); // NULL terminator

    for (auto PermutedId : PermutedIds)
      ArgHints.push_back(PermutedId);
    ArgHints.push_back((IdentifierLoc *)nullptr); // NULL terminator

    auto &EofTok = Toks[i];
    assert(EofTok.is(tok::eof));
    i += 1;

    assert(Toks.size() == i); // Nothing following
    ConsumeAnnotationToken();
    return true;
  }

  if (IdTok.getIdentifierInfo()->getName() == "pack") {
    assert(ApplyOnLocs.size() <= 1 && "only single loop supported for pack");
    assert(!ApplyOnFollowing && "pack applies to single loop only");
    if (ApplyOnLocs.empty())
      // Apply on following loop
      ArgHints.push_back((IdentifierLoc *)nullptr);
    else
      ArgHints.push_back(ApplyOnLocs[0]);

    DeclRefExpr *Array = nullptr;
    ArgsUnion OnHeap = (IdentifierLoc *)nullptr;
    Expr *IslSize = nullptr;
    Expr *IslRedirect = nullptr;
    while (true) {
      SmallVector<ArgsUnion, 4> ClauseArgs;
      auto Kind = parseNextClause(PP, *this, Tok, Toks, i, ClauseArgs);
      if (Kind == TransformClauseKind::None)
        break;
      switch (Kind) {
      default:
        llvm_unreachable("unsupported clause for pack");
      case TransformClauseKind::Allocate:
        assert(ClauseArgs.size() == 1);
        assert(!OnHeap);
        OnHeap = ClauseArgs[0];
        break;
      case TransformClauseKind::Array:
        assert(ClauseArgs.size() == 1);
        assert(!Array);
        Array = cast<DeclRefExpr>(ClauseArgs[0].get<Expr *>());
        break;
      case TransformClauseKind::IslSize:
        assert(ClauseArgs.size() == 1);
        assert(!IslSize);
        IslSize = ClauseArgs[0].get<Expr *>();
        break;
      case TransformClauseKind::IslRedirect:
        assert(ClauseArgs.size() == 1);
        assert(!IslRedirect);
        IslRedirect = ClauseArgs[0].get<Expr *>();
        break;
      }
    }

    ArgHints.push_back(Array);
    ArgHints.push_back(OnHeap);
    ArgHints.push_back(IslSize);
    ArgHints.push_back(IslRedirect);

    auto &EofTok = Toks[i];
    assert(EofTok.is(tok::eof));
    i += 1;

    assert(Toks.size() == i); // Nothing following
    PP.Lex(Tok);              // ConsumeAnnotationToken(); or rparen
    return true;
  }

  if (IdTok.getIdentifierInfo()->getName() == "unrolling") {
    assert(ApplyOnLocs.size() <= 1 &&
           "only single loop supported for unrolling");
    assert(!ApplyOnFollowing && "unrolling applies to single loop only");
    if (ApplyOnLocs.empty())
      // Apply on following loop
      ArgHints.push_back((IdentifierLoc *)nullptr);
    else
      ArgHints.push_back(ApplyOnLocs[0]);

    ArgsUnion Factor{(Expr *)nullptr};
    ArgsUnion Full{(IdentifierLoc *)nullptr}; // Only presence matters
    while (true) {
      SmallVector<ArgsUnion, 4> ClauseArgs;
      auto Kind = parseNextClause(PP, *this, Tok, Toks, i, ClauseArgs);
      if (Kind == TransformClauseKind::None)
        break;
      switch (Kind) {
      default:
        llvm_unreachable("unsupported clause for unrolling");
      case TransformClauseKind::Factor:
        assert(ClauseArgs.size() == 1);
        Factor = ClauseArgs[0];
        break;
      case TransformClauseKind::Full:
        assert(ClauseArgs.size() == 1);
        Full = ClauseArgs[0];
        break;
      }
    }

    assert((!Factor || !Full) && "factor(n) and full contradicting");
    ArgHints.push_back(Factor);
    ArgHints.push_back(Full);

    auto &EofTok = Toks[i];
    assert(EofTok.is(tok::eof));
    i += 1;

    assert(Toks.size() == i); // Nothing following
    PP.Lex(Tok);              // ConsumeAnnotationToken(); or rparen
    return true;
  }

  if (IdTok.getIdentifierInfo()->getName() == "unrollingandjam") {
    assert(!ApplyOnFollowing && "following loop not supported by unrollandjam");
    // assert(ApplyOnLocs.size()==2&&  "specify outer (loop too unroll) and
    // inner (loop to jam) as loops to apply on");
    for (auto NameLoc : ApplyOnLocs)
      ArgHints.push_back(NameLoc);
    ArgHints.push_back((IdentifierLoc *)nullptr);

    ArgsUnion Factor{(Expr *)nullptr};
    ArgsUnion Full{(IdentifierLoc *)nullptr}; // Only presence matters
    SmallVector<ArgsUnion, 4> PermutedIds;
    while (true) {
      // TODO: Unroll-and-Jam not necessarily jams the innermost loop, might
      // also jam some loop in-between. Add clause for this option. Maybe by
      // having two loops in the "on" clause?
      SmallVector<ArgsUnion, 4> ClauseArgs;
      auto Kind = parseNextClause(PP, *this, Tok, Toks, i, ClauseArgs);
      if (Kind == TransformClauseKind::None)
        break;
      switch (Kind) {
      default:
        llvm_unreachable("unsupported clause for unrollingandjam");
      case TransformClauseKind::Factor:
        assert(ClauseArgs.size() == 1);
        Factor = ClauseArgs[0];
        break;
      case TransformClauseKind::Full:
        assert(ClauseArgs.size() == 1);
        Full = ClauseArgs[0];
        break;
      case TransformClauseKind::UnrolledIds:
        assert(ClauseArgs.size() >= 2);
        assert(PermutedIds.empty());
        llvm::append_range(PermutedIds, ClauseArgs);
        break;
      }
    }

    assert((!Factor || !Full) && "factor(n) and full contradicting");
    ArgHints.push_back(Factor);
    ArgHints.push_back(Full);

    llvm::append_range(ArgHints, PermutedIds);
    ArgHints.push_back((IdentifierLoc *)nullptr);

    auto &EofTok = Toks[i];
    assert(EofTok.is(tok::eof));
    i += 1;

    assert(Toks.size() == i); // Nothing following
    PP.Lex(Tok);              // ConsumeAnnotationToken(); or rparen
    return true;
  }

  if (IdTok.getIdentifierInfo()->getName() == "parallelize_thread") {
    assert(ApplyOnLocs.size() <= 1 &&
           "only single loop supported for thread-parallelism; use collapse "
           "before to parallelize multiple loops");
    assert(!ApplyOnFollowing &&
           "parallelize_thread applies to single loop only");
    if (ApplyOnLocs.empty())
      // Apply on following loop
      ArgHints.push_back((IdentifierLoc *)nullptr);
    else
      ArgHints.push_back(ApplyOnLocs[0]);

    while (true) {
      SmallVector<ArgsUnion, 4> ClauseArgs;
      auto Kind = parseNextClause(PP, *this, Tok, Toks, i, ClauseArgs);
      if (Kind == TransformClauseKind::None)
        break;
      llvm_unreachable("unsupported clause for thread-parallelism");
    }

    auto &EofTok = Toks[i];
    assert(EofTok.is(tok::eof));
    i += 1;

    assert(Toks.size() == i); // Nothing following
    PP.Lex(Tok);              // ConsumeAnnotationToken(); or rparen
    return true;
  }

  if (IdTok.getIdentifierInfo()->getName() == "fission") {
    assert(ApplyOnLocs.size() <= 1 &&
           "only single loop supported for loop fission/distribution");
    assert(!ApplyOnFollowing && "fission applies to single loop only");
    if (ApplyOnLocs.empty())
      // Apply to following loop
      ArgHints.push_back((IdentifierLoc *)nullptr);
    else
      ArgHints.push_back(ApplyOnLocs[0]);

    ArgsUnion Autofission{(IdentifierLoc *)nullptr}; // Only presence matters
    SmallVector<ArgsUnion, 4> SplitAt;
    SmallVector<ArgsUnion, 4> FissionedIds;
    while (true) {
      SmallVector<ArgsUnion, 4> ClauseArgs;
      auto Kind = parseNextClause(PP, *this, Tok, Toks, i, ClauseArgs);
      if (Kind == TransformClauseKind::None)
        break;
      switch (Kind) {
      case TransformClauseKind::Autofission:
        assert(ClauseArgs.size() == 1);
        Autofission = ClauseArgs[0];
        break;
      case TransformClauseKind::SplitAt:
        assert(!ClauseArgs.empty());
        assert(SplitAt.empty());
        SplitAt = ClauseArgs;
        break;
      case TransformClauseKind::FissionendIds:
        assert(!ClauseArgs.empty());
        assert(FissionedIds.empty());
        FissionedIds = std::move(ClauseArgs);
        break;
      default:
        llvm_unreachable("unsupported clause for fission");
      }
    }

    auto &EofTok = Toks[i];
    assert(EofTok.is(tok::eof));
    i += 1;

    ArgHints.push_back(Autofission);
    for (auto SplitPos : SplitAt)
      ArgHints.push_back(SplitPos);
    ArgHints.push_back((Expr *)nullptr);
    for (auto PitId : FissionedIds)
      ArgHints.push_back(PitId);
    ArgHints.push_back((IdentifierLoc *)nullptr);

    assert(Toks.size() == i && "must have parsed all clauses");
    PP.Lex(Tok);
    return true;
  }

  if (IdTok.getIdentifierInfo()->getName() == "fuse") {
    assert(ApplyOnLocs.size() > 1 && "must fuse at least two loops");
    assert(!ApplyOnFollowing && "fusion requires loop ids");
    for (auto NameLoc : ApplyOnLocs)
      ArgHints.push_back(NameLoc);
    ArgHints.push_back((IdentifierLoc *)nullptr);

    ArgsUnion FusedId{(IdentifierLoc *)nullptr};
    while (true) {
      SmallVector<ArgsUnion, 4> ClauseArgs;
      auto Kind = parseNextClause(PP, *this, Tok, Toks, i, ClauseArgs);
      if (Kind == TransformClauseKind::None)
        break;
      switch (Kind) {
      case TransformClauseKind::FusedId:
        assert(ClauseArgs.size() == 1);
        assert(!FusedId);
        FusedId = ClauseArgs[0];
        break;
      default:
        llvm_unreachable("unsupported clause for fuse");
      }
    }
    auto &EofTok = Toks[i];
    assert(EofTok.is(tok::eof));
    i += 1;

    ArgHints.push_back(FusedId);

    assert(Toks.size() == i && "must have parsed all clauses");
    PP.Lex(Tok);
    return true;
  }

  llvm_unreachable("Unrecognized transformation");
}

namespace {
struct PragmaAttributeInfo {
  enum ActionType { Push, Pop, Attribute };
  ParsedAttributes &Attributes;
  ActionType Action;
  const IdentifierInfo *Namespace = nullptr;
  ArrayRef<Token> Tokens;

  PragmaAttributeInfo(ParsedAttributes &Attributes) : Attributes(Attributes) {}
};

#include "clang/Parse/AttrSubMatchRulesParserStringSwitches.inc"

} // end anonymous namespace

static StringRef getIdentifier(const Token &Tok) {
  if (Tok.is(tok::identifier))
    return Tok.getIdentifierInfo()->getName();
  const char *S = tok::getKeywordSpelling(Tok.getKind());
  if (!S)
    return "";
  return S;
}

static bool isAbstractAttrMatcherRule(attr::SubjectMatchRule Rule) {
  using namespace attr;
  switch (Rule) {
#define ATTR_MATCH_RULE(Value, Spelling, IsAbstract)                           \
  case Value:                                                                  \
    return IsAbstract;
#include "clang/Basic/AttrSubMatchRulesList.inc"
  }
  llvm_unreachable("Invalid attribute subject match rule");
  return false;
}

static void diagnoseExpectedAttributeSubjectSubRule(
    Parser &PRef, attr::SubjectMatchRule PrimaryRule, StringRef PrimaryRuleName,
    SourceLocation SubRuleLoc) {
  auto Diagnostic =
      PRef.Diag(SubRuleLoc,
                diag::err_pragma_attribute_expected_subject_sub_identifier)
      << PrimaryRuleName;
  if (const char *SubRules = validAttributeSubjectMatchSubRules(PrimaryRule))
    Diagnostic << /*SubRulesSupported=*/1 << SubRules;
  else
    Diagnostic << /*SubRulesSupported=*/0;
}

static void diagnoseUnknownAttributeSubjectSubRule(
    Parser &PRef, attr::SubjectMatchRule PrimaryRule, StringRef PrimaryRuleName,
    StringRef SubRuleName, SourceLocation SubRuleLoc) {

  auto Diagnostic =
      PRef.Diag(SubRuleLoc, diag::err_pragma_attribute_unknown_subject_sub_rule)
      << SubRuleName << PrimaryRuleName;
  if (const char *SubRules = validAttributeSubjectMatchSubRules(PrimaryRule))
    Diagnostic << /*SubRulesSupported=*/1 << SubRules;
  else
    Diagnostic << /*SubRulesSupported=*/0;
}

bool Parser::ParsePragmaAttributeSubjectMatchRuleSet(
    attr::ParsedSubjectMatchRuleSet &SubjectMatchRules, SourceLocation &AnyLoc,
    SourceLocation &LastMatchRuleEndLoc) {
  bool IsAny = false;
  BalancedDelimiterTracker AnyParens(*this, tok::l_paren);
  if (getIdentifier(Tok) == "any") {
    AnyLoc = ConsumeToken();
    IsAny = true;
    if (AnyParens.expectAndConsume())
      return true;
  }

  do {
    // Parse the subject matcher rule.
    StringRef Name = getIdentifier(Tok);
    if (Name.empty()) {
      Diag(Tok, diag::err_pragma_attribute_expected_subject_identifier);
      return true;
    }
    std::pair<Optional<attr::SubjectMatchRule>,
              Optional<attr::SubjectMatchRule> (*)(StringRef, bool)>
        Rule = isAttributeSubjectMatchRule(Name);
    if (!Rule.first) {
      Diag(Tok, diag::err_pragma_attribute_unknown_subject_rule) << Name;
      return true;
    }
    attr::SubjectMatchRule PrimaryRule = *Rule.first;
    SourceLocation RuleLoc = ConsumeToken();

    BalancedDelimiterTracker Parens(*this, tok::l_paren);
    if (isAbstractAttrMatcherRule(PrimaryRule)) {
      if (Parens.expectAndConsume())
        return true;
    } else if (Parens.consumeOpen()) {
      if (!SubjectMatchRules
               .insert(
                   std::make_pair(PrimaryRule, SourceRange(RuleLoc, RuleLoc)))
               .second)
        Diag(RuleLoc, diag::err_pragma_attribute_duplicate_subject)
            << Name
            << FixItHint::CreateRemoval(SourceRange(
                   RuleLoc, Tok.is(tok::comma) ? Tok.getLocation() : RuleLoc));
      LastMatchRuleEndLoc = RuleLoc;
      continue;
    }

    // Parse the sub-rules.
    StringRef SubRuleName = getIdentifier(Tok);
    if (SubRuleName.empty()) {
      diagnoseExpectedAttributeSubjectSubRule(*this, PrimaryRule, Name,
                                              Tok.getLocation());
      return true;
    }
    attr::SubjectMatchRule SubRule;
    if (SubRuleName == "unless") {
      SourceLocation SubRuleLoc = ConsumeToken();
      BalancedDelimiterTracker Parens(*this, tok::l_paren);
      if (Parens.expectAndConsume())
        return true;
      SubRuleName = getIdentifier(Tok);
      if (SubRuleName.empty()) {
        diagnoseExpectedAttributeSubjectSubRule(*this, PrimaryRule, Name,
                                                SubRuleLoc);
        return true;
      }
      auto SubRuleOrNone = Rule.second(SubRuleName, /*IsUnless=*/true);
      if (!SubRuleOrNone) {
        std::string SubRuleUnlessName = "unless(" + SubRuleName.str() + ")";
        diagnoseUnknownAttributeSubjectSubRule(*this, PrimaryRule, Name,
                                               SubRuleUnlessName, SubRuleLoc);
        return true;
      }
      SubRule = *SubRuleOrNone;
      ConsumeToken();
      if (Parens.consumeClose())
        return true;
    } else {
      auto SubRuleOrNone = Rule.second(SubRuleName, /*IsUnless=*/false);
      if (!SubRuleOrNone) {
        diagnoseUnknownAttributeSubjectSubRule(*this, PrimaryRule, Name,
                                               SubRuleName, Tok.getLocation());
        return true;
      }
      SubRule = *SubRuleOrNone;
      ConsumeToken();
    }
    SourceLocation RuleEndLoc = Tok.getLocation();
    LastMatchRuleEndLoc = RuleEndLoc;
    if (Parens.consumeClose())
      return true;
    if (!SubjectMatchRules
             .insert(std::make_pair(SubRule, SourceRange(RuleLoc, RuleEndLoc)))
             .second) {
      Diag(RuleLoc, diag::err_pragma_attribute_duplicate_subject)
          << attr::getSubjectMatchRuleSpelling(SubRule)
          << FixItHint::CreateRemoval(SourceRange(
                 RuleLoc, Tok.is(tok::comma) ? Tok.getLocation() : RuleEndLoc));
      continue;
    }
  } while (IsAny && TryConsumeToken(tok::comma));

  if (IsAny)
    if (AnyParens.consumeClose())
      return true;

  return false;
}

namespace {

/// Describes the stage at which attribute subject rule parsing was interrupted.
enum class MissingAttributeSubjectRulesRecoveryPoint {
  Comma,
  ApplyTo,
  Equals,
  Any,
  None,
};

MissingAttributeSubjectRulesRecoveryPoint
getAttributeSubjectRulesRecoveryPointForToken(const Token &Tok) {
  if (const auto *II = Tok.getIdentifierInfo()) {
    if (II->isStr("apply_to"))
      return MissingAttributeSubjectRulesRecoveryPoint::ApplyTo;
    if (II->isStr("any"))
      return MissingAttributeSubjectRulesRecoveryPoint::Any;
  }
  if (Tok.is(tok::equal))
    return MissingAttributeSubjectRulesRecoveryPoint::Equals;
  return MissingAttributeSubjectRulesRecoveryPoint::None;
}

/// Creates a diagnostic for the attribute subject rule parsing diagnostic that
/// suggests the possible attribute subject rules in a fix-it together with
/// any other missing tokens.
DiagnosticBuilder createExpectedAttributeSubjectRulesTokenDiagnostic(
    unsigned DiagID, ParsedAttr &Attribute,
    MissingAttributeSubjectRulesRecoveryPoint Point, Parser &PRef) {
  SourceLocation Loc = PRef.getEndOfPreviousToken();
  if (Loc.isInvalid())
    Loc = PRef.getCurToken().getLocation();
  auto Diagnostic = PRef.Diag(Loc, DiagID);
  std::string FixIt;
  MissingAttributeSubjectRulesRecoveryPoint EndPoint =
      getAttributeSubjectRulesRecoveryPointForToken(PRef.getCurToken());
  if (Point == MissingAttributeSubjectRulesRecoveryPoint::Comma)
    FixIt = ", ";
  if (Point <= MissingAttributeSubjectRulesRecoveryPoint::ApplyTo &&
      EndPoint > MissingAttributeSubjectRulesRecoveryPoint::ApplyTo)
    FixIt += "apply_to";
  if (Point <= MissingAttributeSubjectRulesRecoveryPoint::Equals &&
      EndPoint > MissingAttributeSubjectRulesRecoveryPoint::Equals)
    FixIt += " = ";
  SourceRange FixItRange(Loc);
  if (EndPoint == MissingAttributeSubjectRulesRecoveryPoint::None) {
    // Gather the subject match rules that are supported by the attribute.
    SmallVector<std::pair<attr::SubjectMatchRule, bool>, 4> SubjectMatchRuleSet;
    Attribute.getMatchRules(PRef.getLangOpts(), SubjectMatchRuleSet);
    if (SubjectMatchRuleSet.empty()) {
      // FIXME: We can emit a "fix-it" with a subject list placeholder when
      // placeholders will be supported by the fix-its.
      return Diagnostic;
    }
    FixIt += "any(";
    bool NeedsComma = false;
    for (const auto &I : SubjectMatchRuleSet) {
      // Ensure that the missing rule is reported in the fix-it only when it's
      // supported in the current language mode.
      if (!I.second)
        continue;
      if (NeedsComma)
        FixIt += ", ";
      else
        NeedsComma = true;
      FixIt += attr::getSubjectMatchRuleSpelling(I.first);
    }
    FixIt += ")";
    // Check if we need to remove the range
    PRef.SkipUntil(tok::eof, Parser::StopBeforeMatch);
    FixItRange.setEnd(PRef.getCurToken().getLocation());
  }
  if (FixItRange.getBegin() == FixItRange.getEnd())
    Diagnostic << FixItHint::CreateInsertion(FixItRange.getBegin(), FixIt);
  else
    Diagnostic << FixItHint::CreateReplacement(
        CharSourceRange::getCharRange(FixItRange), FixIt);
  return Diagnostic;
}

} // end anonymous namespace

void Parser::HandlePragmaAttribute() {
  assert(Tok.is(tok::annot_pragma_attribute) &&
         "Expected #pragma attribute annotation token");
  SourceLocation PragmaLoc = Tok.getLocation();
  auto *Info = static_cast<PragmaAttributeInfo *>(Tok.getAnnotationValue());
  if (Info->Action == PragmaAttributeInfo::Pop) {
    ConsumeAnnotationToken();
    Actions.ActOnPragmaAttributePop(PragmaLoc, Info->Namespace);
    return;
  }
  // Parse the actual attribute with its arguments.
  assert((Info->Action == PragmaAttributeInfo::Push ||
          Info->Action == PragmaAttributeInfo::Attribute) &&
         "Unexpected #pragma attribute command");

  if (Info->Action == PragmaAttributeInfo::Push && Info->Tokens.empty()) {
    ConsumeAnnotationToken();
    Actions.ActOnPragmaAttributeEmptyPush(PragmaLoc, Info->Namespace);
    return;
  }

  PP.EnterTokenStream(Info->Tokens, /*DisableMacroExpansion=*/false,
                      /*IsReinject=*/false);
  ConsumeAnnotationToken();

  ParsedAttributes &Attrs = Info->Attributes;
  Attrs.clearListOnly();

  auto SkipToEnd = [this]() {
    SkipUntil(tok::eof, StopBeforeMatch);
    ConsumeToken();
  };

  if (Tok.is(tok::l_square) && NextToken().is(tok::l_square)) {
    // Parse the CXX11 style attribute.
    ParseCXX11AttributeSpecifier(Attrs);
  } else if (Tok.is(tok::kw___attribute)) {
    ConsumeToken();
    if (ExpectAndConsume(tok::l_paren, diag::err_expected_lparen_after,
                         "attribute"))
      return SkipToEnd();
    if (ExpectAndConsume(tok::l_paren, diag::err_expected_lparen_after, "("))
      return SkipToEnd();

    // FIXME: The practical usefulness of completion here is limited because
    // we only get here if the line has balanced parens.
    if (Tok.is(tok::code_completion)) {
      cutOffParsing();
      // FIXME: suppress completion of unsupported attributes?
      Actions.CodeCompleteAttribute(AttributeCommonInfo::Syntax::AS_GNU);
      return SkipToEnd();
    }

    if (Tok.isNot(tok::identifier)) {
      Diag(Tok, diag::err_pragma_attribute_expected_attribute_name);
      SkipToEnd();
      return;
    }
    IdentifierInfo *AttrName = Tok.getIdentifierInfo();
    SourceLocation AttrNameLoc = ConsumeToken();

    if (Tok.isNot(tok::l_paren))
      Attrs.addNew(AttrName, AttrNameLoc, nullptr, AttrNameLoc, nullptr, 0,
                   ParsedAttr::AS_GNU);
    else
      ParseGNUAttributeArgs(AttrName, AttrNameLoc, Attrs, /*EndLoc=*/nullptr,
                            /*ScopeName=*/nullptr,
                            /*ScopeLoc=*/SourceLocation(), ParsedAttr::AS_GNU,
                            /*Declarator=*/nullptr);

    if (ExpectAndConsume(tok::r_paren))
      return SkipToEnd();
    if (ExpectAndConsume(tok::r_paren))
      return SkipToEnd();
  } else if (Tok.is(tok::kw___declspec)) {
    ParseMicrosoftDeclSpecs(Attrs);
  } else {
    Diag(Tok, diag::err_pragma_attribute_expected_attribute_syntax);
    if (Tok.getIdentifierInfo()) {
      // If we suspect that this is an attribute suggest the use of
      // '__attribute__'.
      if (ParsedAttr::getParsedKind(
              Tok.getIdentifierInfo(), /*ScopeName=*/nullptr,
              ParsedAttr::AS_GNU) != ParsedAttr::UnknownAttribute) {
        SourceLocation InsertStartLoc = Tok.getLocation();
        ConsumeToken();
        if (Tok.is(tok::l_paren)) {
          ConsumeAnyToken();
          SkipUntil(tok::r_paren, StopBeforeMatch);
          if (Tok.isNot(tok::r_paren))
            return SkipToEnd();
        }
        Diag(Tok, diag::note_pragma_attribute_use_attribute_kw)
            << FixItHint::CreateInsertion(InsertStartLoc, "__attribute__((")
            << FixItHint::CreateInsertion(Tok.getEndLoc(), "))");
      }
    }
    SkipToEnd();
    return;
  }

  if (Attrs.empty() || Attrs.begin()->isInvalid()) {
    SkipToEnd();
    return;
  }

  // Ensure that we don't have more than one attribute.
  if (Attrs.size() > 1) {
    SourceLocation Loc = Attrs[1].getLoc();
    Diag(Loc, diag::err_pragma_attribute_multiple_attributes);
    SkipToEnd();
    return;
  }

  ParsedAttr &Attribute = *Attrs.begin();
  if (!Attribute.isSupportedByPragmaAttribute()) {
    Diag(PragmaLoc, diag::err_pragma_attribute_unsupported_attribute)
        << Attribute;
    SkipToEnd();
    return;
  }

  // Parse the subject-list.
  if (!TryConsumeToken(tok::comma)) {
    createExpectedAttributeSubjectRulesTokenDiagnostic(
        diag::err_expected, Attribute,
        MissingAttributeSubjectRulesRecoveryPoint::Comma, *this)
        << tok::comma;
    SkipToEnd();
    return;
  }

  if (Tok.isNot(tok::identifier)) {
    createExpectedAttributeSubjectRulesTokenDiagnostic(
        diag::err_pragma_attribute_invalid_subject_set_specifier, Attribute,
        MissingAttributeSubjectRulesRecoveryPoint::ApplyTo, *this);
    SkipToEnd();
    return;
  }
  const IdentifierInfo *II = Tok.getIdentifierInfo();
  if (!II->isStr("apply_to")) {
    createExpectedAttributeSubjectRulesTokenDiagnostic(
        diag::err_pragma_attribute_invalid_subject_set_specifier, Attribute,
        MissingAttributeSubjectRulesRecoveryPoint::ApplyTo, *this);
    SkipToEnd();
    return;
  }
  ConsumeToken();

  if (!TryConsumeToken(tok::equal)) {
    createExpectedAttributeSubjectRulesTokenDiagnostic(
        diag::err_expected, Attribute,
        MissingAttributeSubjectRulesRecoveryPoint::Equals, *this)
        << tok::equal;
    SkipToEnd();
    return;
  }

  attr::ParsedSubjectMatchRuleSet SubjectMatchRules;
  SourceLocation AnyLoc, LastMatchRuleEndLoc;
  if (ParsePragmaAttributeSubjectMatchRuleSet(SubjectMatchRules, AnyLoc,
                                              LastMatchRuleEndLoc)) {
    SkipToEnd();
    return;
  }

  // Tokens following an ill-formed attribute will remain in the token stream
  // and must be removed.
  if (Tok.isNot(tok::eof)) {
    Diag(Tok, diag::err_pragma_attribute_extra_tokens_after_attribute);
    SkipToEnd();
    return;
  }

  // Consume the eof terminator token.
  ConsumeToken();

  // Handle a mixed push/attribute by desurging to a push, then an attribute.
  if (Info->Action == PragmaAttributeInfo::Push)
    Actions.ActOnPragmaAttributeEmptyPush(PragmaLoc, Info->Namespace);

  Actions.ActOnPragmaAttributeAttribute(Attribute, PragmaLoc,
                                        std::move(SubjectMatchRules));
}

// #pragma GCC visibility comes in two variants:
//   'push' '(' [visibility] ')'
//   'pop'
void PragmaGCCVisibilityHandler::HandlePragma(Preprocessor &PP,
                                              PragmaIntroducer Introducer,
                                              Token &VisTok) {
  SourceLocation VisLoc = VisTok.getLocation();

  Token Tok;
  PP.LexUnexpandedToken(Tok);

  const IdentifierInfo *PushPop = Tok.getIdentifierInfo();

  const IdentifierInfo *VisType;
  if (PushPop && PushPop->isStr("pop")) {
    VisType = nullptr;
  } else if (PushPop && PushPop->isStr("push")) {
    PP.LexUnexpandedToken(Tok);
    if (Tok.isNot(tok::l_paren)) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_lparen)
        << "visibility";
      return;
    }
    PP.LexUnexpandedToken(Tok);
    VisType = Tok.getIdentifierInfo();
    if (!VisType) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_identifier)
        << "visibility";
      return;
    }
    PP.LexUnexpandedToken(Tok);
    if (Tok.isNot(tok::r_paren)) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_rparen)
        << "visibility";
      return;
    }
  } else {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_identifier)
      << "visibility";
    return;
  }
  SourceLocation EndLoc = Tok.getLocation();
  PP.LexUnexpandedToken(Tok);
  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
      << "visibility";
    return;
  }

  auto Toks = std::make_unique<Token[]>(1);
  Toks[0].startToken();
  Toks[0].setKind(tok::annot_pragma_vis);
  Toks[0].setLocation(VisLoc);
  Toks[0].setAnnotationEndLoc(EndLoc);
  Toks[0].setAnnotationValue(
      const_cast<void *>(static_cast<const void *>(VisType)));
  PP.EnterTokenStream(std::move(Toks), 1, /*DisableMacroExpansion=*/true,
                      /*IsReinject=*/false);
}

// #pragma pack(...) comes in the following delicious flavors:
//   pack '(' [integer] ')'
//   pack '(' 'show' ')'
//   pack '(' ('push' | 'pop') [',' identifier] [, integer] ')'
void PragmaPackHandler::HandlePragma(Preprocessor &PP,
                                     PragmaIntroducer Introducer,
                                     Token &PackTok) {
  SourceLocation PackLoc = PackTok.getLocation();

  Token Tok;
  PP.Lex(Tok);
  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_lparen) << "pack";
    return;
  }

  Sema::PragmaMsStackAction Action = Sema::PSK_Reset;
  StringRef SlotLabel;
  Token Alignment;
  Alignment.startToken();
  PP.Lex(Tok);
  if (Tok.is(tok::numeric_constant)) {
    Alignment = Tok;

    PP.Lex(Tok);

    // In MSVC/gcc, #pragma pack(4) sets the alignment without affecting
    // the push/pop stack.
    // In Apple gcc/XL, #pragma pack(4) is equivalent to #pragma pack(push, 4)
    Action = (PP.getLangOpts().ApplePragmaPack || PP.getLangOpts().XLPragmaPack)
                 ? Sema::PSK_Push_Set
                 : Sema::PSK_Set;
  } else if (Tok.is(tok::identifier)) {
    const IdentifierInfo *II = Tok.getIdentifierInfo();
    if (II->isStr("show")) {
      Action = Sema::PSK_Show;
      PP.Lex(Tok);
    } else {
      if (II->isStr("push")) {
        Action = Sema::PSK_Push;
      } else if (II->isStr("pop")) {
        Action = Sema::PSK_Pop;
      } else {
        PP.Diag(Tok.getLocation(), diag::warn_pragma_invalid_action) << "pack";
        return;
      }
      PP.Lex(Tok);

      if (Tok.is(tok::comma)) {
        PP.Lex(Tok);

        if (Tok.is(tok::numeric_constant)) {
          Action = (Sema::PragmaMsStackAction)(Action | Sema::PSK_Set);
          Alignment = Tok;

          PP.Lex(Tok);
        } else if (Tok.is(tok::identifier)) {
          SlotLabel = Tok.getIdentifierInfo()->getName();
          PP.Lex(Tok);

          if (Tok.is(tok::comma)) {
            PP.Lex(Tok);

            if (Tok.isNot(tok::numeric_constant)) {
              PP.Diag(Tok.getLocation(), diag::warn_pragma_pack_malformed);
              return;
            }

            Action = (Sema::PragmaMsStackAction)(Action | Sema::PSK_Set);
            Alignment = Tok;

            PP.Lex(Tok);
          }
        } else {
          PP.Diag(Tok.getLocation(), diag::warn_pragma_pack_malformed);
          return;
        }
      }
    }
  } else if (PP.getLangOpts().ApplePragmaPack ||
             PP.getLangOpts().XLPragmaPack) {
    // In MSVC/gcc, #pragma pack() resets the alignment without affecting
    // the push/pop stack.
    // In Apple gcc and IBM XL, #pragma pack() is equivalent to #pragma
    // pack(pop).
    Action = Sema::PSK_Pop;
  }

  if (Tok.isNot(tok::r_paren)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_rparen) << "pack";
    return;
  }

  SourceLocation RParenLoc = Tok.getLocation();
  PP.Lex(Tok);
  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol) << "pack";
    return;
  }

  PragmaPackInfo *Info =
      PP.getPreprocessorAllocator().Allocate<PragmaPackInfo>(1);
  Info->Action = Action;
  Info->SlotLabel = SlotLabel;
  Info->Alignment = Alignment;

  MutableArrayRef<Token> Toks(PP.getPreprocessorAllocator().Allocate<Token>(1),
                              1);
  Toks[0].startToken();
  Toks[0].setKind(tok::annot_pragma_pack);
  Toks[0].setLocation(PackLoc);
  Toks[0].setAnnotationEndLoc(RParenLoc);
  Toks[0].setAnnotationValue(static_cast<void*>(Info));
  PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                      /*IsReinject=*/false);
}

// #pragma ms_struct on
// #pragma ms_struct off
void PragmaMSStructHandler::HandlePragma(Preprocessor &PP,
                                         PragmaIntroducer Introducer,
                                         Token &MSStructTok) {
  PragmaMSStructKind Kind = PMSST_OFF;

  Token Tok;
  PP.Lex(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_ms_struct);
    return;
  }
  SourceLocation EndLoc = Tok.getLocation();
  const IdentifierInfo *II = Tok.getIdentifierInfo();
  if (II->isStr("on")) {
    Kind = PMSST_ON;
    PP.Lex(Tok);
  }
  else if (II->isStr("off") || II->isStr("reset"))
    PP.Lex(Tok);
  else {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_ms_struct);
    return;
  }

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
      << "ms_struct";
    return;
  }

  MutableArrayRef<Token> Toks(PP.getPreprocessorAllocator().Allocate<Token>(1),
                              1);
  Toks[0].startToken();
  Toks[0].setKind(tok::annot_pragma_msstruct);
  Toks[0].setLocation(MSStructTok.getLocation());
  Toks[0].setAnnotationEndLoc(EndLoc);
  Toks[0].setAnnotationValue(reinterpret_cast<void*>(
                             static_cast<uintptr_t>(Kind)));
  PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                      /*IsReinject=*/false);
}

// #pragma clang section bss="abc" data="" rodata="def" text="" relro=""
void PragmaClangSectionHandler::HandlePragma(Preprocessor &PP,
                                             PragmaIntroducer Introducer,
                                             Token &FirstToken) {

  Token Tok;
  auto SecKind = Sema::PragmaClangSectionKind::PCSK_Invalid;

  PP.Lex(Tok); // eat 'section'
  while (Tok.isNot(tok::eod)) {
    if (Tok.isNot(tok::identifier)) {
      PP.Diag(Tok.getLocation(), diag::err_pragma_expected_clang_section_name) << "clang section";
      return;
    }

    const IdentifierInfo *SecType = Tok.getIdentifierInfo();
    if (SecType->isStr("bss"))
      SecKind = Sema::PragmaClangSectionKind::PCSK_BSS;
    else if (SecType->isStr("data"))
      SecKind = Sema::PragmaClangSectionKind::PCSK_Data;
    else if (SecType->isStr("rodata"))
      SecKind = Sema::PragmaClangSectionKind::PCSK_Rodata;
    else if (SecType->isStr("relro"))
      SecKind = Sema::PragmaClangSectionKind::PCSK_Relro;
    else if (SecType->isStr("text"))
      SecKind = Sema::PragmaClangSectionKind::PCSK_Text;
    else {
      PP.Diag(Tok.getLocation(), diag::err_pragma_expected_clang_section_name) << "clang section";
      return;
    }

    SourceLocation PragmaLocation = Tok.getLocation();
    PP.Lex(Tok); // eat ['bss'|'data'|'rodata'|'text']
    if (Tok.isNot(tok::equal)) {
      PP.Diag(Tok.getLocation(), diag::err_pragma_clang_section_expected_equal) << SecKind;
      return;
    }

    std::string SecName;
    if (!PP.LexStringLiteral(Tok, SecName, "pragma clang section", false))
      return;

    Actions.ActOnPragmaClangSection(
        PragmaLocation,
        (SecName.size() ? Sema::PragmaClangSectionAction::PCSA_Set
                        : Sema::PragmaClangSectionAction::PCSA_Clear),
        SecKind, SecName);
  }
}

// #pragma 'align' '=' {'native','natural','mac68k','power','reset'}
// #pragma 'options 'align' '=' {'native','natural','mac68k','power','reset'}
// #pragma 'align' '(' {'native','natural','mac68k','power','reset'} ')'
static void ParseAlignPragma(Preprocessor &PP, Token &FirstTok,
                             bool IsOptions) {
  Token Tok;

  if (IsOptions) {
    PP.Lex(Tok);
    if (Tok.isNot(tok::identifier) ||
        !Tok.getIdentifierInfo()->isStr("align")) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_options_expected_align);
      return;
    }
  }

  PP.Lex(Tok);
  if (PP.getLangOpts().XLPragmaPack) {
    if (Tok.isNot(tok::l_paren)) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_lparen) << "align";
      return;
    }
  } else if (Tok.isNot(tok::equal)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_align_expected_equal)
      << IsOptions;
    return;
  }

  PP.Lex(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_identifier)
      << (IsOptions ? "options" : "align");
    return;
  }

  Sema::PragmaOptionsAlignKind Kind = Sema::POAK_Natural;
  const IdentifierInfo *II = Tok.getIdentifierInfo();
  if (II->isStr("native"))
    Kind = Sema::POAK_Native;
  else if (II->isStr("natural"))
    Kind = Sema::POAK_Natural;
  else if (II->isStr("packed"))
    Kind = Sema::POAK_Packed;
  else if (II->isStr("power"))
    Kind = Sema::POAK_Power;
  else if (II->isStr("mac68k"))
    Kind = Sema::POAK_Mac68k;
  else if (II->isStr("reset"))
    Kind = Sema::POAK_Reset;
  else {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_align_invalid_option)
      << IsOptions;
    return;
  }

  if (PP.getLangOpts().XLPragmaPack) {
    PP.Lex(Tok);
    if (Tok.isNot(tok::r_paren)) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_rparen) << "align";
      return;
    }
  }

  SourceLocation EndLoc = Tok.getLocation();
  PP.Lex(Tok);
  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
      << (IsOptions ? "options" : "align");
    return;
  }

  MutableArrayRef<Token> Toks(PP.getPreprocessorAllocator().Allocate<Token>(1),
                              1);
  Toks[0].startToken();
  Toks[0].setKind(tok::annot_pragma_align);
  Toks[0].setLocation(FirstTok.getLocation());
  Toks[0].setAnnotationEndLoc(EndLoc);
  Toks[0].setAnnotationValue(reinterpret_cast<void*>(
                             static_cast<uintptr_t>(Kind)));
  PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                      /*IsReinject=*/false);
}

void PragmaAlignHandler::HandlePragma(Preprocessor &PP,
                                      PragmaIntroducer Introducer,
                                      Token &AlignTok) {
  ParseAlignPragma(PP, AlignTok, /*IsOptions=*/false);
}

void PragmaOptionsHandler::HandlePragma(Preprocessor &PP,
                                        PragmaIntroducer Introducer,
                                        Token &OptionsTok) {
  ParseAlignPragma(PP, OptionsTok, /*IsOptions=*/true);
}

// #pragma unused(identifier)
void PragmaUnusedHandler::HandlePragma(Preprocessor &PP,
                                       PragmaIntroducer Introducer,
                                       Token &UnusedTok) {
  // FIXME: Should we be expanding macros here? My guess is no.
  SourceLocation UnusedLoc = UnusedTok.getLocation();

  // Lex the left '('.
  Token Tok;
  PP.Lex(Tok);
  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_lparen) << "unused";
    return;
  }

  // Lex the declaration reference(s).
  SmallVector<Token, 5> Identifiers;
  SourceLocation RParenLoc;
  bool LexID = true;

  while (true) {
    PP.Lex(Tok);

    if (LexID) {
      if (Tok.is(tok::identifier)) {
        Identifiers.push_back(Tok);
        LexID = false;
        continue;
      }

      // Illegal token!
      PP.Diag(Tok.getLocation(), diag::warn_pragma_unused_expected_var);
      return;
    }

    // We are execting a ')' or a ','.
    if (Tok.is(tok::comma)) {
      LexID = true;
      continue;
    }

    if (Tok.is(tok::r_paren)) {
      RParenLoc = Tok.getLocation();
      break;
    }

    // Illegal token!
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_punc) << "unused";
    return;
  }

  PP.Lex(Tok);
  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol) <<
        "unused";
    return;
  }

  // Verify that we have a location for the right parenthesis.
  assert(RParenLoc.isValid() && "Valid '#pragma unused' must have ')'");
  assert(!Identifiers.empty() && "Valid '#pragma unused' must have arguments");

  // For each identifier token, insert into the token stream a
  // annot_pragma_unused token followed by the identifier token.
  // This allows us to cache a "#pragma unused" that occurs inside an inline
  // C++ member function.

  MutableArrayRef<Token> Toks(
      PP.getPreprocessorAllocator().Allocate<Token>(2 * Identifiers.size()),
      2 * Identifiers.size());
  for (unsigned i=0; i != Identifiers.size(); i++) {
    Token &pragmaUnusedTok = Toks[2*i], &idTok = Toks[2*i+1];
    pragmaUnusedTok.startToken();
    pragmaUnusedTok.setKind(tok::annot_pragma_unused);
    pragmaUnusedTok.setLocation(UnusedLoc);
    idTok = Identifiers[i];
  }
  PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                      /*IsReinject=*/false);
}

// #pragma weak identifier
// #pragma weak identifier '=' identifier
void PragmaWeakHandler::HandlePragma(Preprocessor &PP,
                                     PragmaIntroducer Introducer,
                                     Token &WeakTok) {
  SourceLocation WeakLoc = WeakTok.getLocation();

  Token Tok;
  PP.Lex(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_identifier) << "weak";
    return;
  }

  Token WeakName = Tok;
  bool HasAlias = false;
  Token AliasName;

  PP.Lex(Tok);
  if (Tok.is(tok::equal)) {
    HasAlias = true;
    PP.Lex(Tok);
    if (Tok.isNot(tok::identifier)) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_identifier)
          << "weak";
      return;
    }
    AliasName = Tok;
    PP.Lex(Tok);
  }

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol) << "weak";
    return;
  }

  if (HasAlias) {
    MutableArrayRef<Token> Toks(
        PP.getPreprocessorAllocator().Allocate<Token>(3), 3);
    Token &pragmaUnusedTok = Toks[0];
    pragmaUnusedTok.startToken();
    pragmaUnusedTok.setKind(tok::annot_pragma_weakalias);
    pragmaUnusedTok.setLocation(WeakLoc);
    pragmaUnusedTok.setAnnotationEndLoc(AliasName.getLocation());
    Toks[1] = WeakName;
    Toks[2] = AliasName;
    PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                        /*IsReinject=*/false);
  } else {
    MutableArrayRef<Token> Toks(
        PP.getPreprocessorAllocator().Allocate<Token>(2), 2);
    Token &pragmaUnusedTok = Toks[0];
    pragmaUnusedTok.startToken();
    pragmaUnusedTok.setKind(tok::annot_pragma_weak);
    pragmaUnusedTok.setLocation(WeakLoc);
    pragmaUnusedTok.setAnnotationEndLoc(WeakLoc);
    Toks[1] = WeakName;
    PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                        /*IsReinject=*/false);
  }
}

// #pragma redefine_extname identifier identifier
void PragmaRedefineExtnameHandler::HandlePragma(Preprocessor &PP,
                                                PragmaIntroducer Introducer,
                                                Token &RedefToken) {
  SourceLocation RedefLoc = RedefToken.getLocation();

  Token Tok;
  PP.Lex(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_identifier) <<
      "redefine_extname";
    return;
  }

  Token RedefName = Tok;
  PP.Lex(Tok);

  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_identifier)
        << "redefine_extname";
    return;
  }

  Token AliasName = Tok;
  PP.Lex(Tok);

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol) <<
      "redefine_extname";
    return;
  }

  MutableArrayRef<Token> Toks(PP.getPreprocessorAllocator().Allocate<Token>(3),
                              3);
  Token &pragmaRedefTok = Toks[0];
  pragmaRedefTok.startToken();
  pragmaRedefTok.setKind(tok::annot_pragma_redefine_extname);
  pragmaRedefTok.setLocation(RedefLoc);
  pragmaRedefTok.setAnnotationEndLoc(AliasName.getLocation());
  Toks[1] = RedefName;
  Toks[2] = AliasName;
  PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                      /*IsReinject=*/false);
}

void PragmaFPContractHandler::HandlePragma(Preprocessor &PP,
                                           PragmaIntroducer Introducer,
                                           Token &Tok) {
  tok::OnOffSwitch OOS;
  if (PP.LexOnOffSwitch(OOS))
    return;

  MutableArrayRef<Token> Toks(PP.getPreprocessorAllocator().Allocate<Token>(1),
                              1);
  Toks[0].startToken();
  Toks[0].setKind(tok::annot_pragma_fp_contract);
  Toks[0].setLocation(Tok.getLocation());
  Toks[0].setAnnotationEndLoc(Tok.getLocation());
  Toks[0].setAnnotationValue(reinterpret_cast<void*>(
                             static_cast<uintptr_t>(OOS)));
  PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                      /*IsReinject=*/false);
}

void PragmaOpenCLExtensionHandler::HandlePragma(Preprocessor &PP,
                                                PragmaIntroducer Introducer,
                                                Token &Tok) {
  PP.LexUnexpandedToken(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_identifier) <<
      "OPENCL";
    return;
  }
  IdentifierInfo *Ext = Tok.getIdentifierInfo();
  SourceLocation NameLoc = Tok.getLocation();

  PP.Lex(Tok);
  if (Tok.isNot(tok::colon)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_colon) << Ext;
    return;
  }

  PP.Lex(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_predicate) << 0;
    return;
  }
  IdentifierInfo *Pred = Tok.getIdentifierInfo();

  OpenCLExtState State;
  if (Pred->isStr("enable")) {
    State = Enable;
  } else if (Pred->isStr("disable")) {
    State = Disable;
  } else if (Pred->isStr("begin"))
    State = Begin;
  else if (Pred->isStr("end"))
    State = End;
  else {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_predicate)
      << Ext->isStr("all");
    return;
  }
  SourceLocation StateLoc = Tok.getLocation();

  PP.Lex(Tok);
  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol) <<
      "OPENCL EXTENSION";
    return;
  }

  auto Info = PP.getPreprocessorAllocator().Allocate<OpenCLExtData>(1);
  Info->first = Ext;
  Info->second = State;
  MutableArrayRef<Token> Toks(PP.getPreprocessorAllocator().Allocate<Token>(1),
                              1);
  Toks[0].startToken();
  Toks[0].setKind(tok::annot_pragma_opencl_extension);
  Toks[0].setLocation(NameLoc);
  Toks[0].setAnnotationValue(static_cast<void*>(Info));
  Toks[0].setAnnotationEndLoc(StateLoc);
  PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                      /*IsReinject=*/false);

  if (PP.getPPCallbacks())
    PP.getPPCallbacks()->PragmaOpenCLExtension(NameLoc, Ext,
                                               StateLoc, State);
}

/// Handle '#pragma omp ...' when OpenMP is disabled.
///
void PragmaNoOpenMPHandler::HandlePragma(Preprocessor &PP,
                                         PragmaIntroducer Introducer,
                                         Token &FirstTok) {
  if (!PP.getDiagnostics().isIgnored(diag::warn_pragma_omp_ignored,
                                     FirstTok.getLocation())) {
    PP.Diag(FirstTok, diag::warn_pragma_omp_ignored);
    PP.getDiagnostics().setSeverity(diag::warn_pragma_omp_ignored,
                                    diag::Severity::Ignored, SourceLocation());
  }
  PP.DiscardUntilEndOfDirective();
}

/// Handle '#pragma omp ...' when OpenMP is enabled.
///
void PragmaOpenMPHandler::HandlePragma(Preprocessor &PP,
                                       PragmaIntroducer Introducer,
                                       Token &FirstTok) {
  SmallVector<Token, 16> Pragma;
  Token Tok;
  Tok.startToken();
  Tok.setKind(tok::annot_pragma_openmp);
  Tok.setLocation(Introducer.Loc);

  while (Tok.isNot(tok::eod) && Tok.isNot(tok::eof)) {
    Pragma.push_back(Tok);
    PP.Lex(Tok);
    if (Tok.is(tok::annot_pragma_openmp)) {
      // #pragma omp INSIDE a #pragma omp?!?
      // According to
      // http://lab.llvm.org:8080/coverage/coverage-reports/clang/coverage/Users/buildslave/jenkins/workspace/clang-stage2-coverage-R/llvm/tools/clang/lib/Parse/ParsePragma.cpp.html#L2134
      // this does actually happen
      // Have to find when. Can this happen for #pragma clang loop as well?
      // Added in r325369

      PP.Diag(Tok, diag::err_omp_unexpected_directive) << 0;
      unsigned InnerPragmaCnt = 1;
      while (InnerPragmaCnt != 0) {
        PP.Lex(Tok);
        if (Tok.is(tok::annot_pragma_openmp))
          ++InnerPragmaCnt;
        else if (Tok.is(tok::annot_pragma_openmp_end))
          --InnerPragmaCnt;
      }
      PP.Lex(Tok);
    }
  }
  SourceLocation EodLoc = Tok.getLocation();
  Tok.startToken();
  Tok.setKind(tok::annot_pragma_openmp_end);
  Tok.setLocation(EodLoc);
  Pragma.push_back(Tok);

  auto Toks = std::make_unique<Token[]>(Pragma.size());
  std::copy(Pragma.begin(), Pragma.end(), Toks.get());
  PP.EnterTokenStream(std::move(Toks), Pragma.size(),
                      /*DisableMacroExpansion=*/false, /*IsReinject=*/false);
}

/// Handle '#pragma pointers_to_members'
// The grammar for this pragma is as follows:
//
// <inheritance model> ::= ('single' | 'multiple' | 'virtual') '_inheritance'
//
// #pragma pointers_to_members '(' 'best_case' ')'
// #pragma pointers_to_members '(' 'full_generality' [',' inheritance-model] ')'
// #pragma pointers_to_members '(' inheritance-model ')'
void PragmaMSPointersToMembers::HandlePragma(Preprocessor &PP,
                                             PragmaIntroducer Introducer,
                                             Token &Tok) {
  SourceLocation PointersToMembersLoc = Tok.getLocation();
  PP.Lex(Tok);
  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(PointersToMembersLoc, diag::warn_pragma_expected_lparen)
      << "pointers_to_members";
    return;
  }
  PP.Lex(Tok);
  const IdentifierInfo *Arg = Tok.getIdentifierInfo();
  if (!Arg) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_identifier)
      << "pointers_to_members";
    return;
  }
  PP.Lex(Tok);

  LangOptions::PragmaMSPointersToMembersKind RepresentationMethod;
  if (Arg->isStr("best_case")) {
    RepresentationMethod = LangOptions::PPTMK_BestCase;
  } else {
    if (Arg->isStr("full_generality")) {
      if (Tok.is(tok::comma)) {
        PP.Lex(Tok);

        Arg = Tok.getIdentifierInfo();
        if (!Arg) {
          PP.Diag(Tok.getLocation(),
                  diag::err_pragma_pointers_to_members_unknown_kind)
              << Tok.getKind() << /*OnlyInheritanceModels*/ 0;
          return;
        }
        PP.Lex(Tok);
      } else if (Tok.is(tok::r_paren)) {
        // #pragma pointers_to_members(full_generality) implicitly specifies
        // virtual_inheritance.
        Arg = nullptr;
        RepresentationMethod = LangOptions::PPTMK_FullGeneralityVirtualInheritance;
      } else {
        PP.Diag(Tok.getLocation(), diag::err_expected_punc)
            << "full_generality";
        return;
      }
    }

    if (Arg) {
      if (Arg->isStr("single_inheritance")) {
        RepresentationMethod =
            LangOptions::PPTMK_FullGeneralitySingleInheritance;
      } else if (Arg->isStr("multiple_inheritance")) {
        RepresentationMethod =
            LangOptions::PPTMK_FullGeneralityMultipleInheritance;
      } else if (Arg->isStr("virtual_inheritance")) {
        RepresentationMethod =
            LangOptions::PPTMK_FullGeneralityVirtualInheritance;
      } else {
        PP.Diag(Tok.getLocation(),
                diag::err_pragma_pointers_to_members_unknown_kind)
            << Arg << /*HasPointerDeclaration*/ 1;
        return;
      }
    }
  }

  if (Tok.isNot(tok::r_paren)) {
    PP.Diag(Tok.getLocation(), diag::err_expected_rparen_after)
        << (Arg ? Arg->getName() : "full_generality");
    return;
  }

  SourceLocation EndLoc = Tok.getLocation();
  PP.Lex(Tok);
  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
      << "pointers_to_members";
    return;
  }

  Token AnnotTok;
  AnnotTok.startToken();
  AnnotTok.setKind(tok::annot_pragma_ms_pointers_to_members);
  AnnotTok.setLocation(PointersToMembersLoc);
  AnnotTok.setAnnotationEndLoc(EndLoc);
  AnnotTok.setAnnotationValue(
      reinterpret_cast<void *>(static_cast<uintptr_t>(RepresentationMethod)));
  PP.EnterToken(AnnotTok, /*IsReinject=*/true);
}

/// Handle '#pragma vtordisp'
// The grammar for this pragma is as follows:
//
// <vtordisp-mode> ::= ('off' | 'on' | '0' | '1' | '2' )
//
// #pragma vtordisp '(' ['push' ','] vtordisp-mode ')'
// #pragma vtordisp '(' 'pop' ')'
// #pragma vtordisp '(' ')'
void PragmaMSVtorDisp::HandlePragma(Preprocessor &PP,
                                    PragmaIntroducer Introducer, Token &Tok) {
  SourceLocation VtorDispLoc = Tok.getLocation();
  PP.Lex(Tok);
  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(VtorDispLoc, diag::warn_pragma_expected_lparen) << "vtordisp";
    return;
  }
  PP.Lex(Tok);

  Sema::PragmaMsStackAction Action = Sema::PSK_Set;
  const IdentifierInfo *II = Tok.getIdentifierInfo();
  if (II) {
    if (II->isStr("push")) {
      // #pragma vtordisp(push, mode)
      PP.Lex(Tok);
      if (Tok.isNot(tok::comma)) {
        PP.Diag(VtorDispLoc, diag::warn_pragma_expected_punc) << "vtordisp";
        return;
      }
      PP.Lex(Tok);
      Action = Sema::PSK_Push_Set;
      // not push, could be on/off
    } else if (II->isStr("pop")) {
      // #pragma vtordisp(pop)
      PP.Lex(Tok);
      Action = Sema::PSK_Pop;
    }
    // not push or pop, could be on/off
  } else {
    if (Tok.is(tok::r_paren)) {
      // #pragma vtordisp()
      Action = Sema::PSK_Reset;
    }
  }


  uint64_t Value = 0;
  if (Action & Sema::PSK_Push || Action & Sema::PSK_Set) {
    const IdentifierInfo *II = Tok.getIdentifierInfo();
    if (II && II->isStr("off")) {
      PP.Lex(Tok);
      Value = 0;
    } else if (II && II->isStr("on")) {
      PP.Lex(Tok);
      Value = 1;
    } else if (Tok.is(tok::numeric_constant) &&
               PP.parseSimpleIntegerLiteral(Tok, Value)) {
      if (Value > 2) {
        PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_integer)
            << 0 << 2 << "vtordisp";
        return;
      }
    } else {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_invalid_action)
          << "vtordisp";
      return;
    }
  }

  // Finish the pragma: ')' $
  if (Tok.isNot(tok::r_paren)) {
    PP.Diag(VtorDispLoc, diag::warn_pragma_expected_rparen) << "vtordisp";
    return;
  }
  SourceLocation EndLoc = Tok.getLocation();
  PP.Lex(Tok);
  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << "vtordisp";
    return;
  }

  // Enter the annotation.
  Token AnnotTok;
  AnnotTok.startToken();
  AnnotTok.setKind(tok::annot_pragma_ms_vtordisp);
  AnnotTok.setLocation(VtorDispLoc);
  AnnotTok.setAnnotationEndLoc(EndLoc);
  AnnotTok.setAnnotationValue(reinterpret_cast<void *>(
      static_cast<uintptr_t>((Action << 16) | (Value & 0xFFFF))));
  PP.EnterToken(AnnotTok, /*IsReinject=*/false);
}

/// Handle all MS pragmas.  Simply forwards the tokens after inserting
/// an annotation token.
void PragmaMSPragma::HandlePragma(Preprocessor &PP,
                                  PragmaIntroducer Introducer, Token &Tok) {
  Token EoF, AnnotTok;
  EoF.startToken();
  EoF.setKind(tok::eof);
  AnnotTok.startToken();
  AnnotTok.setKind(tok::annot_pragma_ms_pragma);
  AnnotTok.setLocation(Tok.getLocation());
  AnnotTok.setAnnotationEndLoc(Tok.getLocation());
  SmallVector<Token, 8> TokenVector;
  // Suck up all of the tokens before the eod.
  for (; Tok.isNot(tok::eod); PP.Lex(Tok)) {
    TokenVector.push_back(Tok);
    AnnotTok.setAnnotationEndLoc(Tok.getLocation());
  }
  // Add a sentinel EoF token to the end of the list.
  TokenVector.push_back(EoF);
  // We must allocate this array with new because EnterTokenStream is going to
  // delete it later.
  markAsReinjectedForRelexing(TokenVector);
  auto TokenArray = std::make_unique<Token[]>(TokenVector.size());
  std::copy(TokenVector.begin(), TokenVector.end(), TokenArray.get());
  auto Value = new (PP.getPreprocessorAllocator())
      std::pair<std::unique_ptr<Token[]>, size_t>(std::move(TokenArray),
                                                  TokenVector.size());
  AnnotTok.setAnnotationValue(Value);
  PP.EnterToken(AnnotTok, /*IsReinject*/ false);
}

/// Handle the \#pragma float_control extension.
///
/// The syntax is:
/// \code
///   #pragma float_control(keyword[, setting] [,push])
/// \endcode
/// Where 'keyword' and 'setting' are identifiers.
// 'keyword' can be: precise, except, push, pop
// 'setting' can be: on, off
/// The optional arguments 'setting' and 'push' are supported only
/// when the keyword is 'precise' or 'except'.
void PragmaFloatControlHandler::HandlePragma(Preprocessor &PP,
                                             PragmaIntroducer Introducer,
                                             Token &Tok) {
  Sema::PragmaMsStackAction Action = Sema::PSK_Set;
  SourceLocation FloatControlLoc = Tok.getLocation();
  Token PragmaName = Tok;
  if (!PP.getTargetInfo().hasStrictFP() && !PP.getLangOpts().ExpStrictFP) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_fp_ignored)
        << PragmaName.getIdentifierInfo()->getName();
    return;
  }
  PP.Lex(Tok);
  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(FloatControlLoc, diag::err_expected) << tok::l_paren;
    return;
  }

  // Read the identifier.
  PP.Lex(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_float_control_malformed);
    return;
  }

  // Verify that this is one of the float control options.
  IdentifierInfo *II = Tok.getIdentifierInfo();
  PragmaFloatControlKind Kind =
      llvm::StringSwitch<PragmaFloatControlKind>(II->getName())
          .Case("precise", PFC_Precise)
          .Case("except", PFC_Except)
          .Case("push", PFC_Push)
          .Case("pop", PFC_Pop)
          .Default(PFC_Unknown);
  PP.Lex(Tok); // the identifier
  if (Kind == PFC_Unknown) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_float_control_malformed);
    return;
  } else if (Kind == PFC_Push || Kind == PFC_Pop) {
    if (Tok.isNot(tok::r_paren)) {
      PP.Diag(Tok.getLocation(), diag::err_pragma_float_control_malformed);
      return;
    }
    PP.Lex(Tok); // Eat the r_paren
    Action = (Kind == PFC_Pop) ? Sema::PSK_Pop : Sema::PSK_Push;
  } else {
    if (Tok.is(tok::r_paren))
      // Selecting Precise or Except
      PP.Lex(Tok); // the r_paren
    else if (Tok.isNot(tok::comma)) {
      PP.Diag(Tok.getLocation(), diag::err_pragma_float_control_malformed);
      return;
    } else {
      PP.Lex(Tok); // ,
      if (!Tok.isAnyIdentifier()) {
        PP.Diag(Tok.getLocation(), diag::err_pragma_float_control_malformed);
        return;
      }
      StringRef PushOnOff = Tok.getIdentifierInfo()->getName();
      if (PushOnOff == "on")
        // Kind is set correctly
        ;
      else if (PushOnOff == "off") {
        if (Kind == PFC_Precise)
          Kind = PFC_NoPrecise;
        if (Kind == PFC_Except)
          Kind = PFC_NoExcept;
      } else if (PushOnOff == "push") {
        Action = Sema::PSK_Push_Set;
      } else {
        PP.Diag(Tok.getLocation(), diag::err_pragma_float_control_malformed);
        return;
      }
      PP.Lex(Tok); // the identifier
      if (Tok.is(tok::comma)) {
        PP.Lex(Tok); // ,
        if (!Tok.isAnyIdentifier()) {
          PP.Diag(Tok.getLocation(), diag::err_pragma_float_control_malformed);
          return;
        }
        StringRef ExpectedPush = Tok.getIdentifierInfo()->getName();
        if (ExpectedPush == "push") {
          Action = Sema::PSK_Push_Set;
        } else {
          PP.Diag(Tok.getLocation(), diag::err_pragma_float_control_malformed);
          return;
        }
        PP.Lex(Tok); // the push identifier
      }
      if (Tok.isNot(tok::r_paren)) {
        PP.Diag(Tok.getLocation(), diag::err_pragma_float_control_malformed);
        return;
      }
      PP.Lex(Tok); // the r_paren
    }
  }
  SourceLocation EndLoc = Tok.getLocation();
  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << "float_control";
    return;
  }

  // Note: there is no accomodation for PP callback for this pragma.

  // Enter the annotation.
  auto TokenArray = std::make_unique<Token[]>(1);
  TokenArray[0].startToken();
  TokenArray[0].setKind(tok::annot_pragma_float_control);
  TokenArray[0].setLocation(FloatControlLoc);
  TokenArray[0].setAnnotationEndLoc(EndLoc);
  // Create an encoding of Action and Value by shifting the Action into
  // the high 16 bits then union with the Kind.
  TokenArray[0].setAnnotationValue(reinterpret_cast<void *>(
      static_cast<uintptr_t>((Action << 16) | (Kind & 0xFFFF))));
  PP.EnterTokenStream(std::move(TokenArray), 1,
                      /*DisableMacroExpansion=*/false, /*IsReinject=*/false);
}

/// Handle the Microsoft \#pragma detect_mismatch extension.
///
/// The syntax is:
/// \code
///   #pragma detect_mismatch("name", "value")
/// \endcode
/// Where 'name' and 'value' are quoted strings.  The values are embedded in
/// the object file and passed along to the linker.  If the linker detects a
/// mismatch in the object file's values for the given name, a LNK2038 error
/// is emitted.  See MSDN for more details.
void PragmaDetectMismatchHandler::HandlePragma(Preprocessor &PP,
                                               PragmaIntroducer Introducer,
                                               Token &Tok) {
  SourceLocation DetectMismatchLoc = Tok.getLocation();
  PP.Lex(Tok);
  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(DetectMismatchLoc, diag::err_expected) << tok::l_paren;
    return;
  }

  // Read the name to embed, which must be a string literal.
  std::string NameString;
  if (!PP.LexStringLiteral(Tok, NameString,
                           "pragma detect_mismatch",
                           /*AllowMacroExpansion=*/true))
    return;

  // Read the comma followed by a second string literal.
  std::string ValueString;
  if (Tok.isNot(tok::comma)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_detect_mismatch_malformed);
    return;
  }

  if (!PP.LexStringLiteral(Tok, ValueString, "pragma detect_mismatch",
                           /*AllowMacroExpansion=*/true))
    return;

  if (Tok.isNot(tok::r_paren)) {
    PP.Diag(Tok.getLocation(), diag::err_expected) << tok::r_paren;
    return;
  }
  PP.Lex(Tok);  // Eat the r_paren.

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_detect_mismatch_malformed);
    return;
  }

  // If the pragma is lexically sound, notify any interested PPCallbacks.
  if (PP.getPPCallbacks())
    PP.getPPCallbacks()->PragmaDetectMismatch(DetectMismatchLoc, NameString,
                                              ValueString);

  Actions.ActOnPragmaDetectMismatch(DetectMismatchLoc, NameString, ValueString);
}

/// Handle the microsoft \#pragma comment extension.
///
/// The syntax is:
/// \code
///   #pragma comment(linker, "foo")
/// \endcode
/// 'linker' is one of five identifiers: compiler, exestr, lib, linker, user.
/// "foo" is a string, which is fully macro expanded, and permits string
/// concatenation, embedded escape characters etc.  See MSDN for more details.
void PragmaCommentHandler::HandlePragma(Preprocessor &PP,
                                        PragmaIntroducer Introducer,
                                        Token &Tok) {
  SourceLocation CommentLoc = Tok.getLocation();
  PP.Lex(Tok);
  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(CommentLoc, diag::err_pragma_comment_malformed);
    return;
  }

  // Read the identifier.
  PP.Lex(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(CommentLoc, diag::err_pragma_comment_malformed);
    return;
  }

  // Verify that this is one of the 5 explicitly listed options.
  IdentifierInfo *II = Tok.getIdentifierInfo();
  PragmaMSCommentKind Kind =
    llvm::StringSwitch<PragmaMSCommentKind>(II->getName())
    .Case("linker",   PCK_Linker)
    .Case("lib",      PCK_Lib)
    .Case("compiler", PCK_Compiler)
    .Case("exestr",   PCK_ExeStr)
    .Case("user",     PCK_User)
    .Default(PCK_Unknown);
  if (Kind == PCK_Unknown) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_comment_unknown_kind);
    return;
  }

  if (PP.getTargetInfo().getTriple().isOSBinFormatELF() && Kind != PCK_Lib) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_comment_ignored)
        << II->getName();
    return;
  }

  // On PS4, issue a warning about any pragma comments other than
  // #pragma comment lib.
  if (PP.getTargetInfo().getTriple().isPS4() && Kind != PCK_Lib) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_comment_ignored)
      << II->getName();
    return;
  }

  // Read the optional string if present.
  PP.Lex(Tok);
  std::string ArgumentString;
  if (Tok.is(tok::comma) && !PP.LexStringLiteral(Tok, ArgumentString,
                                                 "pragma comment",
                                                 /*AllowMacroExpansion=*/true))
    return;

  // FIXME: warn that 'exestr' is deprecated.
  // FIXME: If the kind is "compiler" warn if the string is present (it is
  // ignored).
  // The MSDN docs say that "lib" and "linker" require a string and have a short
  // list of linker options they support, but in practice MSVC doesn't
  // issue a diagnostic.  Therefore neither does clang.

  if (Tok.isNot(tok::r_paren)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_comment_malformed);
    return;
  }
  PP.Lex(Tok);  // eat the r_paren.

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_comment_malformed);
    return;
  }

  // If the pragma is lexically sound, notify any interested PPCallbacks.
  if (PP.getPPCallbacks())
    PP.getPPCallbacks()->PragmaComment(CommentLoc, II, ArgumentString);

  Actions.ActOnPragmaMSComment(CommentLoc, Kind, ArgumentString);
}

// #pragma clang optimize off
// #pragma clang optimize on
void PragmaOptimizeHandler::HandlePragma(Preprocessor &PP,
                                         PragmaIntroducer Introducer,
                                         Token &FirstToken) {
  Token Tok;
  PP.Lex(Tok);
  if (Tok.is(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_missing_argument)
        << "clang optimize" << /*Expected=*/true << "'on' or 'off'";
    return;
  }
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_optimize_invalid_argument)
      << PP.getSpelling(Tok);
    return;
  }
  const IdentifierInfo *II = Tok.getIdentifierInfo();
  // The only accepted values are 'on' or 'off'.
  bool IsOn = false;
  if (II->isStr("on")) {
    IsOn = true;
  } else if (!II->isStr("off")) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_optimize_invalid_argument)
      << PP.getSpelling(Tok);
    return;
  }
  PP.Lex(Tok);

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_optimize_extra_argument)
      << PP.getSpelling(Tok);
    return;
  }

  Actions.ActOnPragmaOptimize(IsOn, FirstToken.getLocation());
}

namespace {
/// Used as the annotation value for tok::annot_pragma_fp.
struct TokFPAnnotValue {
  enum FlagKinds { Contract, Reassociate, Exceptions };
  enum FlagValues { On, Off, Fast };

  llvm::Optional<LangOptions::FPModeKind> ContractValue;
  llvm::Optional<LangOptions::FPModeKind> ReassociateValue;
  llvm::Optional<LangOptions::FPExceptionModeKind> ExceptionsValue;
};
} // end anonymous namespace

void PragmaFPHandler::HandlePragma(Preprocessor &PP,
                                   PragmaIntroducer Introducer, Token &Tok) {
  // fp
  Token PragmaName = Tok;
  SmallVector<Token, 1> TokenList;

  PP.Lex(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_fp_invalid_option)
        << /*MissingOption=*/true << "";
    return;
  }

  auto *AnnotValue = new (PP.getPreprocessorAllocator()) TokFPAnnotValue;
  while (Tok.is(tok::identifier)) {
    IdentifierInfo *OptionInfo = Tok.getIdentifierInfo();

    auto FlagKind =
        llvm::StringSwitch<llvm::Optional<TokFPAnnotValue::FlagKinds>>(
            OptionInfo->getName())
            .Case("contract", TokFPAnnotValue::Contract)
            .Case("reassociate", TokFPAnnotValue::Reassociate)
            .Case("exceptions", TokFPAnnotValue::Exceptions)
            .Default(None);
    if (!FlagKind) {
      PP.Diag(Tok.getLocation(), diag::err_pragma_fp_invalid_option)
          << /*MissingOption=*/false << OptionInfo;
      return;
    }
    PP.Lex(Tok);

    // Read '('
    if (Tok.isNot(tok::l_paren)) {
      PP.Diag(Tok.getLocation(), diag::err_expected) << tok::l_paren;
      return;
    }
    PP.Lex(Tok);

    if (Tok.isNot(tok::identifier)) {
      PP.Diag(Tok.getLocation(), diag::err_pragma_fp_invalid_argument)
          << PP.getSpelling(Tok) << OptionInfo->getName()
          << static_cast<int>(*FlagKind);
      return;
    }
    const IdentifierInfo *II = Tok.getIdentifierInfo();

    if (FlagKind == TokFPAnnotValue::Contract) {
      AnnotValue->ContractValue =
          llvm::StringSwitch<llvm::Optional<LangOptions::FPModeKind>>(
              II->getName())
              .Case("on", LangOptions::FPModeKind::FPM_On)
              .Case("off", LangOptions::FPModeKind::FPM_Off)
              .Case("fast", LangOptions::FPModeKind::FPM_Fast)
              .Default(llvm::None);
      if (!AnnotValue->ContractValue) {
        PP.Diag(Tok.getLocation(), diag::err_pragma_fp_invalid_argument)
            << PP.getSpelling(Tok) << OptionInfo->getName() << *FlagKind;
        return;
      }
    } else if (FlagKind == TokFPAnnotValue::Reassociate) {
      AnnotValue->ReassociateValue =
          llvm::StringSwitch<llvm::Optional<LangOptions::FPModeKind>>(
              II->getName())
              .Case("on", LangOptions::FPModeKind::FPM_On)
              .Case("off", LangOptions::FPModeKind::FPM_Off)
              .Default(llvm::None);
      if (!AnnotValue->ReassociateValue) {
        PP.Diag(Tok.getLocation(), diag::err_pragma_fp_invalid_argument)
            << PP.getSpelling(Tok) << OptionInfo->getName() << *FlagKind;
        return;
      }
    } else if (FlagKind == TokFPAnnotValue::Exceptions) {
      AnnotValue->ExceptionsValue =
          llvm::StringSwitch<llvm::Optional<LangOptions::FPExceptionModeKind>>(
              II->getName())
              .Case("ignore", LangOptions::FPE_Ignore)
              .Case("maytrap", LangOptions::FPE_MayTrap)
              .Case("strict", LangOptions::FPE_Strict)
              .Default(llvm::None);
      if (!AnnotValue->ExceptionsValue) {
        PP.Diag(Tok.getLocation(), diag::err_pragma_fp_invalid_argument)
            << PP.getSpelling(Tok) << OptionInfo->getName() << *FlagKind;
        return;
      }
    }
    PP.Lex(Tok);

    // Read ')'
    if (Tok.isNot(tok::r_paren)) {
      PP.Diag(Tok.getLocation(), diag::err_expected) << tok::r_paren;
      return;
    }
    PP.Lex(Tok);
  }

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << "clang fp";
    return;
  }

  Token FPTok;
  FPTok.startToken();
  FPTok.setKind(tok::annot_pragma_fp);
  FPTok.setLocation(PragmaName.getLocation());
  FPTok.setAnnotationEndLoc(PragmaName.getLocation());
  FPTok.setAnnotationValue(reinterpret_cast<void *>(AnnotValue));
  TokenList.push_back(FPTok);

  auto TokenArray = std::make_unique<Token[]>(TokenList.size());
  std::copy(TokenList.begin(), TokenList.end(), TokenArray.get());

  PP.EnterTokenStream(std::move(TokenArray), TokenList.size(),
                      /*DisableMacroExpansion=*/false, /*IsReinject=*/false);
}

void PragmaSTDC_FENV_ROUNDHandler::HandlePragma(Preprocessor &PP,
                                                PragmaIntroducer Introducer,
                                                Token &Tok) {
  Token PragmaName = Tok;
  SmallVector<Token, 1> TokenList;
  if (!PP.getTargetInfo().hasStrictFP() && !PP.getLangOpts().ExpStrictFP) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_fp_ignored)
        << PragmaName.getIdentifierInfo()->getName();
    return;
  }

  PP.Lex(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_identifier)
        << PragmaName.getIdentifierInfo()->getName();
    return;
  }
  IdentifierInfo *II = Tok.getIdentifierInfo();

  auto RM =
      llvm::StringSwitch<llvm::RoundingMode>(II->getName())
          .Case("FE_TOWARDZERO", llvm::RoundingMode::TowardZero)
          .Case("FE_TONEAREST", llvm::RoundingMode::NearestTiesToEven)
          .Case("FE_UPWARD", llvm::RoundingMode::TowardPositive)
          .Case("FE_DOWNWARD", llvm::RoundingMode::TowardNegative)
          .Case("FE_TONEARESTFROMZERO", llvm::RoundingMode::NearestTiesToAway)
          .Case("FE_DYNAMIC", llvm::RoundingMode::Dynamic)
          .Default(llvm::RoundingMode::Invalid);
  if (RM == llvm::RoundingMode::Invalid) {
    PP.Diag(Tok.getLocation(), diag::warn_stdc_unknown_rounding_mode);
    return;
  }
  PP.Lex(Tok);

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << "STDC FENV_ROUND";
    return;
  }

  // Until the pragma is fully implemented, issue a warning.
  PP.Diag(Tok.getLocation(), diag::warn_stdc_fenv_round_not_supported);

  MutableArrayRef<Token> Toks(PP.getPreprocessorAllocator().Allocate<Token>(1),
                              1);
  Toks[0].startToken();
  Toks[0].setKind(tok::annot_pragma_fenv_round);
  Toks[0].setLocation(Tok.getLocation());
  Toks[0].setAnnotationEndLoc(Tok.getLocation());
  Toks[0].setAnnotationValue(
      reinterpret_cast<void *>(static_cast<uintptr_t>(RM)));
  PP.EnterTokenStream(Toks, /*DisableMacroExpansion=*/true,
                      /*IsReinject=*/false);
}

void Parser::HandlePragmaFP() {
  assert(Tok.is(tok::annot_pragma_fp));
  auto *AnnotValue =
      reinterpret_cast<TokFPAnnotValue *>(Tok.getAnnotationValue());

  if (AnnotValue->ReassociateValue)
    Actions.ActOnPragmaFPReassociate(Tok.getLocation(),
                                     *AnnotValue->ReassociateValue ==
                                         LangOptions::FPModeKind::FPM_On);
  if (AnnotValue->ContractValue)
    Actions.ActOnPragmaFPContract(Tok.getLocation(),
                                  *AnnotValue->ContractValue);
  if (AnnotValue->ExceptionsValue)
    Actions.ActOnPragmaFPExceptions(Tok.getLocation(),
                                    *AnnotValue->ExceptionsValue);
  ConsumeAnnotationToken();
}

/// Parses loop or unroll pragma hint value and fills in Info.
static bool ParseLoopHintValue(Preprocessor &PP, Token &Tok, Token PragmaName,
                               Token Option, bool ValueInParens,
                               PragmaLoopHintInfo &Info) {
  SmallVector<Token, 1> ValueList;
  int OpenParens = ValueInParens ? 1 : 0;
  // Read constant expression.
  while (Tok.isNot(tok::eod)) {
    if (Tok.is(tok::l_paren))
      OpenParens++;
    else if (Tok.is(tok::r_paren)) {
      OpenParens--;
      if (OpenParens == 0 && ValueInParens)
        break;
    }

    ValueList.push_back(Tok);
    PP.Lex(Tok);
  }

  if (ValueInParens) {
    // Read ')'
    if (Tok.isNot(tok::r_paren)) {
      PP.Diag(Tok.getLocation(), diag::err_expected) << tok::r_paren;
      return true;
    }
    PP.Lex(Tok);
  }

  Token EOFTok;
  EOFTok.startToken();
  EOFTok.setKind(tok::eof);
  EOFTok.setLocation(Tok.getLocation());
  ValueList.push_back(EOFTok); // Terminates expression for parsing.

  markAsReinjectedForRelexing(ValueList);
  Info.Toks = llvm::makeArrayRef(ValueList).copy(PP.getPreprocessorAllocator());

  Info.PragmaName = PragmaName;
  Info.Option = Option;
  return false;
}

void PragmaLoopHintHandler::HandlePragma(Preprocessor &PP,
                                         PragmaIntroducer Introducer,
                                         Token &Tok) {
  // Identify the legacy syntax
  // Matches one of:
  //   "loop" <keyword> "("
  //
  // New syntax does not have "(" after <keyword>

  auto &KeywordLoopToken = Tok;
  assert(KeywordLoopToken.is(tok::identifier));
  assert(KeywordLoopToken.getIdentifierInfo()->getName() == "loop");

  Token HintToken;
  PP.Lex(HintToken);

  if (HintToken.is(tok::l_paren)) {
    // New Syntax:
    // #pragma clang loop(loopname) ...
    PP.EnterTokenStream(HintToken, true, /*IsReinject=*/true);
    return HandleOmpSyntax(PP, Introducer, KeywordLoopToken);
  }

  if (HintToken.is(tok::eof) ||
      (HintToken.is(tok::identifier) &&
       llvm::StringSwitch<bool>(HintToken.getIdentifierInfo()->getName())
           .Cases("vectorize", "vectorize_width", "interleave",
                  "interleave_count", "unroll", "unroll_count", "distribute",
                  "unrollandjam", "unrollandjam_count", "badkeyword", true)
           .Cases("pipeline", "pipeline_initiation_interval", true)
           .Default(false))) {
    // Known legacy keywords, not (yet) supported by new syntax
    // #pragma clang loop <keyword>(<option>)
    PP.EnterTokenStream(HintToken, true, /*IsReinject=*/true);
    return HandleLegacySyntax(PP, Introducer, Tok);
  }

  if (HintToken.is(tok::identifier) &&
      HintToken.getIdentifierInfo()->getName() == "id") {
    // New keyword
    // #pragma clang loop id(loopname)
    PP.EnterTokenStream(HintToken, true, /*IsReinject=*/true);
    return HandleOmpSyntax(PP, Introducer, KeywordLoopToken);
  }

  Token LParToken;
  PP.Lex(LParToken);

  if (!LParToken.is(tok::l_paren)) {
    // New syntax has no direct option after <keyword>
    PP.EnterTokenStream({HintToken, LParToken}, true, /*IsReinject=*/true);
    return HandleOmpSyntax(PP, Introducer, KeywordLoopToken);
  }

  PP.EnterTokenStream({HintToken, LParToken}, true, /*IsReinject=*/true);
  return HandleLegacySyntax(PP, Introducer, Tok);
}

/// Handle the \#pragma clang loop directive.
///  #pragma clang 'loop' loop-hints
///
///  loop-hints:
///    loop-hint loop-hints[opt]
///
///  loop-hint:
///    'vectorize' '(' loop-hint-keyword ')'
///    'interleave' '(' loop-hint-keyword ')'
///    'unroll' '(' unroll-hint-keyword ')'
///    'vectorize_predicate' '(' loop-hint-keyword ')'
///    'vectorize_width' '(' loop-hint-value ')'
///    'interleave_count' '(' loop-hint-value ')'
///    'unroll_count' '(' loop-hint-value ')'
///    'pipeline' '(' disable ')'
///    'pipeline_initiation_interval' '(' loop-hint-value ')'
///
///  loop-hint-keyword:
///    'enable'
///    'disable'
///    'assume_safety'
///
///  unroll-hint-keyword:
///    'enable'
///    'disable'
///    'full'
///
///  loop-hint-value:
///    constant-expression
///
/// Specifying vectorize(enable) or vectorize_width(_value_) instructs llvm to
/// try vectorizing the instructions of the loop it precedes. Specifying
/// interleave(enable) or interleave_count(_value_) instructs llvm to try
/// interleaving multiple iterations of the loop it precedes. The width of the
/// vector instructions is specified by vectorize_width() and the number of
/// interleaved loop iterations is specified by interleave_count(). Specifying a
/// value of 1 effectively disables vectorization/interleaving, even if it is
/// possible and profitable, and 0 is invalid. The loop vectorizer currently
/// only works on inner loops.
///
/// The unroll and unroll_count directives control the concatenation
/// unroller. Specifying unroll(enable) instructs llvm to unroll the loop
/// completely if the trip count is known at compile time and unroll partially
/// if the trip count is not known.  Specifying unroll(full) is similar to
/// unroll(enable) but will unroll the loop only if the trip count is known at
/// compile time.  Specifying unroll(disable) disables unrolling for the
/// loop. Specifying unroll_count(_value_) instructs llvm to try to unroll the
/// loop the number of times indicated by the value.
void PragmaLoopHintHandler::HandleLegacySyntax(Preprocessor &PP,
                                               PragmaIntroducer Introducer,
                                               Token &Tok) {
  // Incoming token is "loop" from "#pragma clang loop".
  Token PragmaName = Tok;
  SmallVector<Token, 1> TokenList;

  // Lex the optimization option and verify it is an identifier.
  PP.Lex(Tok);
  if (Tok.isNot(tok::identifier)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_loop_invalid_option)
        << /*MissingOption=*/true << "";
    return;
  }

  while (Tok.is(tok::identifier)) {
    Token Option = Tok;
    IdentifierInfo *OptionInfo = Tok.getIdentifierInfo();

    bool OptionValid = llvm::StringSwitch<bool>(OptionInfo->getName())
                           .Case("vectorize", true)
                           .Case("interleave", true)
                           .Case("unroll", true)
                           .Case("distribute", true)
                           .Case("vectorize_predicate", true)
                           .Case("vectorize_width", true)
                           .Case("interleave_count", true)
                           .Case("unroll_count", true)
                           .Case("pipeline", true)
                           .Case("pipeline_initiation_interval", true)
                           .Default(false);
    if (!OptionValid) {
      PP.Diag(Tok.getLocation(), diag::err_pragma_loop_invalid_option)
          << /*MissingOption=*/false << OptionInfo;
      return;
    }
    PP.Lex(Tok);

    // Read '('
    if (Tok.isNot(tok::l_paren)) {
      PP.Diag(Tok.getLocation(), diag::err_expected) << tok::l_paren;
      return;
    }
    PP.Lex(Tok);

    auto *Info = new (PP.getPreprocessorAllocator()) PragmaLoopHintInfo;
    if (ParseLoopHintValue(PP, Tok, PragmaName, Option, /*ValueInParens=*/true,
                           *Info))
      return;

    // Generate the loop hint token.
    Token LoopHintTok;
    LoopHintTok.startToken();
    LoopHintTok.setKind(tok::annot_pragma_loop_hint);
    LoopHintTok.setLocation(Introducer.Loc);
    LoopHintTok.setAnnotationEndLoc(PragmaName.getLocation());
    LoopHintTok.setAnnotationValue(static_cast<void *>(Info));
    TokenList.push_back(LoopHintTok);
  }

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << "clang loop";
    return;
  }

  auto TokenArray = std::make_unique<Token[]>(TokenList.size());
  std::copy(TokenList.begin(), TokenList.end(), TokenArray.get());

  PP.EnterTokenStream(std::move(TokenArray), TokenList.size(),
                      /*DisableMacroExpansion=*/false, /*IsReinject=*/false);
}

void PragmaLoopHintHandler::HandleOmpSyntax(Preprocessor &PP,
                                            PragmaIntroducer Introducer,
                                            Token &Tok) {
  // New #pragma clang loop syntax, one hint per line.

  // Add all tokens for later parsing
  auto StartLoc = Tok.getLocation();
  auto *Info = new (PP.getPreprocessorAllocator()) PragmaLoopHintInfo;

  SmallVector<Token, 4> ValueList;
  while (Tok.isNot(tok::eod)) {
    ValueList.push_back(Tok);
    PP.Lex(Tok);
  }
  auto EndLoc = Tok.getLocation();

  Token EOFTok;
  EOFTok.startToken();
  EOFTok.setKind(tok::eof);
  EOFTok.setLocation(EndLoc);
  ValueList.push_back(EOFTok); // Terminates expression for parsing.

  Info->Toks =
      llvm::makeArrayRef(ValueList).copy(PP.getPreprocessorAllocator());
  // Info->PragmaName = PragmaName;
  // Info->Option = Option;

  auto TokenArray = std::make_unique<Token[]>(1);
  Token &LoopHintTok = TokenArray[0];

  LoopHintTok.startToken();
  LoopHintTok.setKind(tok::annot_pragma_loop_transform);
  LoopHintTok.setLocation(StartLoc);
  LoopHintTok.setAnnotationEndLoc(EndLoc);
  LoopHintTok.setAnnotationValue(static_cast<void *>(Info));

  PP.EnterTokenStream(std::move(TokenArray), 1,
                      /*DisableMacroExpansion=*/false, /*IsReinject=*/false);
}

/// Handle the loop unroll optimization pragmas.
///  #pragma unroll
///  #pragma unroll unroll-hint-value
///  #pragma unroll '(' unroll-hint-value ')'
///  #pragma nounroll
///  #pragma unroll_and_jam
///  #pragma unroll_and_jam unroll-hint-value
///  #pragma unroll_and_jam '(' unroll-hint-value ')'
///  #pragma nounroll_and_jam
///
///  unroll-hint-value:
///    constant-expression
///
/// Loop unrolling hints can be specified with '#pragma unroll' or
/// '#pragma nounroll'. '#pragma unroll' can take a numeric argument optionally
/// contained in parentheses. With no argument the directive instructs llvm to
/// try to unroll the loop completely. A positive integer argument can be
/// specified to indicate the number of times the loop should be unrolled.  To
/// maximize compatibility with other compilers the unroll count argument can be
/// specified with or without parentheses.  Specifying, '#pragma nounroll'
/// disables unrolling of the loop.
void PragmaUnrollHintHandler::HandlePragma(Preprocessor &PP,
                                           PragmaIntroducer Introducer,
                                           Token &Tok) {
  // Incoming token is "unroll" for "#pragma unroll", or "nounroll" for
  // "#pragma nounroll".
  Token PragmaName = Tok;
  PP.Lex(Tok);
  auto *Info = new (PP.getPreprocessorAllocator()) PragmaLoopHintInfo;
  if (Tok.is(tok::eod)) {
    // nounroll or unroll pragma without an argument.
    Info->PragmaName = PragmaName;
    Info->Option.startToken();
  } else if (PragmaName.getIdentifierInfo()->getName() == "nounroll" ||
             PragmaName.getIdentifierInfo()->getName() == "nounroll_and_jam") {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << PragmaName.getIdentifierInfo()->getName();
    return;
  } else {
    // Unroll pragma with an argument: "#pragma unroll N" or
    // "#pragma unroll(N)".
    // Read '(' if it exists.
    bool ValueInParens = Tok.is(tok::l_paren);
    if (ValueInParens)
      PP.Lex(Tok);

    Token Option;
    Option.startToken();
    if (ParseLoopHintValue(PP, Tok, PragmaName, Option, ValueInParens, *Info))
      return;

    // In CUDA, the argument to '#pragma unroll' should not be contained in
    // parentheses.
    if (PP.getLangOpts().CUDA && ValueInParens)
      PP.Diag(Info->Toks[0].getLocation(),
              diag::warn_pragma_unroll_cuda_value_in_parens);

    if (Tok.isNot(tok::eod)) {
      PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
          << "unroll";
      return;
    }
  }

  // Generate the hint token.
  auto TokenArray = std::make_unique<Token[]>(1);
  TokenArray[0].startToken();
  TokenArray[0].setKind(tok::annot_pragma_loop_hint);
  TokenArray[0].setLocation(Introducer.Loc);
  TokenArray[0].setAnnotationEndLoc(PragmaName.getLocation());
  TokenArray[0].setAnnotationValue(static_cast<void *>(Info));
  PP.EnterTokenStream(std::move(TokenArray), 1,
                      /*DisableMacroExpansion=*/false, /*IsReinject=*/false);
}

/// Handle the Microsoft \#pragma intrinsic extension.
///
/// The syntax is:
/// \code
///  #pragma intrinsic(memset)
///  #pragma intrinsic(strlen, memcpy)
/// \endcode
///
/// Pragma intrisic tells the compiler to use a builtin version of the
/// function. Clang does it anyway, so the pragma doesn't really do anything.
/// Anyway, we emit a warning if the function specified in \#pragma intrinsic
/// isn't an intrinsic in clang and suggest to include intrin.h.
void PragmaMSIntrinsicHandler::HandlePragma(Preprocessor &PP,
                                            PragmaIntroducer Introducer,
                                            Token &Tok) {
  PP.Lex(Tok);

  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_lparen)
        << "intrinsic";
    return;
  }
  PP.Lex(Tok);

  bool SuggestIntrinH = !PP.isMacroDefined("__INTRIN_H");

  while (Tok.is(tok::identifier)) {
    IdentifierInfo *II = Tok.getIdentifierInfo();
    if (!II->getBuiltinID())
      PP.Diag(Tok.getLocation(), diag::warn_pragma_intrinsic_builtin)
          << II << SuggestIntrinH;

    PP.Lex(Tok);
    if (Tok.isNot(tok::comma))
      break;
    PP.Lex(Tok);
  }

  if (Tok.isNot(tok::r_paren)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_rparen)
        << "intrinsic";
    return;
  }
  PP.Lex(Tok);

  if (Tok.isNot(tok::eod))
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << "intrinsic";
}

// #pragma optimize("gsty", on|off)
void PragmaMSOptimizeHandler::HandlePragma(Preprocessor &PP,
                                           PragmaIntroducer Introducer,
                                           Token &Tok) {
  SourceLocation StartLoc = Tok.getLocation();
  PP.Lex(Tok);

  if (Tok.isNot(tok::l_paren)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_lparen) << "optimize";
    return;
  }
  PP.Lex(Tok);

  if (Tok.isNot(tok::string_literal)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_string) << "optimize";
    return;
  }
  // We could syntax check the string but it's probably not worth the effort.
  PP.Lex(Tok);

  if (Tok.isNot(tok::comma)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_comma) << "optimize";
    return;
  }
  PP.Lex(Tok);

  if (Tok.is(tok::eod) || Tok.is(tok::r_paren)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_missing_argument)
        << "optimize" << /*Expected=*/true << "'on' or 'off'";
    return;
  }
  IdentifierInfo *II = Tok.getIdentifierInfo();
  if (!II || (!II->isStr("on") && !II->isStr("off"))) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_invalid_argument)
        << PP.getSpelling(Tok) << "optimize" << /*Expected=*/true
        << "'on' or 'off'";
    return;
  }
  PP.Lex(Tok);

  if (Tok.isNot(tok::r_paren)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_expected_rparen) << "optimize";
    return;
  }
  PP.Lex(Tok);

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << "optimize";
    return;
  }
  PP.Diag(StartLoc, diag::warn_pragma_optimize);
}

void PragmaForceCUDAHostDeviceHandler::HandlePragma(
    Preprocessor &PP, PragmaIntroducer Introducer, Token &Tok) {
  Token FirstTok = Tok;

  PP.Lex(Tok);
  IdentifierInfo *Info = Tok.getIdentifierInfo();
  if (!Info || (!Info->isStr("begin") && !Info->isStr("end"))) {
    PP.Diag(FirstTok.getLocation(),
            diag::warn_pragma_force_cuda_host_device_bad_arg);
    return;
  }

  if (Info->isStr("begin"))
    Actions.PushForceCUDAHostDevice();
  else if (!Actions.PopForceCUDAHostDevice())
    PP.Diag(FirstTok.getLocation(),
            diag::err_pragma_cannot_end_force_cuda_host_device);

  PP.Lex(Tok);
  if (!Tok.is(tok::eod))
    PP.Diag(FirstTok.getLocation(),
            diag::warn_pragma_force_cuda_host_device_bad_arg);
}

/// Handle the #pragma clang attribute directive.
///
/// The syntax is:
/// \code
///  #pragma clang attribute push (attribute, subject-set)
///  #pragma clang attribute push
///  #pragma clang attribute (attribute, subject-set)
///  #pragma clang attribute pop
/// \endcode
///
/// There are also 'namespace' variants of push and pop directives. The bare
/// '#pragma clang attribute (attribute, subject-set)' version doesn't require a
/// namespace, since it always applies attributes to the most recently pushed
/// group, regardless of namespace.
/// \code
///  #pragma clang attribute namespace.push (attribute, subject-set)
///  #pragma clang attribute namespace.push
///  #pragma clang attribute namespace.pop
/// \endcode
///
/// The subject-set clause defines the set of declarations which receive the
/// attribute. Its exact syntax is described in the LanguageExtensions document
/// in Clang's documentation.
///
/// This directive instructs the compiler to begin/finish applying the specified
/// attribute to the set of attribute-specific declarations in the active range
/// of the pragma.
void PragmaAttributeHandler::HandlePragma(Preprocessor &PP,
                                          PragmaIntroducer Introducer,
                                          Token &FirstToken) {
  Token Tok;
  PP.Lex(Tok);
  auto *Info = new (PP.getPreprocessorAllocator())
      PragmaAttributeInfo(AttributesForPragmaAttribute);

  // Parse the optional namespace followed by a period.
  if (Tok.is(tok::identifier)) {
    IdentifierInfo *II = Tok.getIdentifierInfo();
    if (!II->isStr("push") && !II->isStr("pop")) {
      Info->Namespace = II;
      PP.Lex(Tok);

      if (!Tok.is(tok::period)) {
        PP.Diag(Tok.getLocation(), diag::err_pragma_attribute_expected_period)
            << II;
        return;
      }
      PP.Lex(Tok);
    }
  }

  if (!Tok.isOneOf(tok::identifier, tok::l_paren)) {
    PP.Diag(Tok.getLocation(),
            diag::err_pragma_attribute_expected_push_pop_paren);
    return;
  }

  // Determine what action this pragma clang attribute represents.
  if (Tok.is(tok::l_paren)) {
    if (Info->Namespace) {
      PP.Diag(Tok.getLocation(),
              diag::err_pragma_attribute_namespace_on_attribute);
      PP.Diag(Tok.getLocation(),
              diag::note_pragma_attribute_namespace_on_attribute);
      return;
    }
    Info->Action = PragmaAttributeInfo::Attribute;
  } else {
    const IdentifierInfo *II = Tok.getIdentifierInfo();
    if (II->isStr("push"))
      Info->Action = PragmaAttributeInfo::Push;
    else if (II->isStr("pop"))
      Info->Action = PragmaAttributeInfo::Pop;
    else {
      PP.Diag(Tok.getLocation(), diag::err_pragma_attribute_invalid_argument)
          << PP.getSpelling(Tok);
      return;
    }

    PP.Lex(Tok);
  }

  // Parse the actual attribute.
  if ((Info->Action == PragmaAttributeInfo::Push && Tok.isNot(tok::eod)) ||
      Info->Action == PragmaAttributeInfo::Attribute) {
    if (Tok.isNot(tok::l_paren)) {
      PP.Diag(Tok.getLocation(), diag::err_expected) << tok::l_paren;
      return;
    }
    PP.Lex(Tok);

    // Lex the attribute tokens.
    SmallVector<Token, 16> AttributeTokens;
    int OpenParens = 1;
    while (Tok.isNot(tok::eod)) {
      if (Tok.is(tok::l_paren))
        OpenParens++;
      else if (Tok.is(tok::r_paren)) {
        OpenParens--;
        if (OpenParens == 0)
          break;
      }

      AttributeTokens.push_back(Tok);
      PP.Lex(Tok);
    }

    if (AttributeTokens.empty()) {
      PP.Diag(Tok.getLocation(), diag::err_pragma_attribute_expected_attribute);
      return;
    }
    if (Tok.isNot(tok::r_paren)) {
      PP.Diag(Tok.getLocation(), diag::err_expected) << tok::r_paren;
      return;
    }
    SourceLocation EndLoc = Tok.getLocation();
    PP.Lex(Tok);

    // Terminate the attribute for parsing.
    Token EOFTok;
    EOFTok.startToken();
    EOFTok.setKind(tok::eof);
    EOFTok.setLocation(EndLoc);
    AttributeTokens.push_back(EOFTok);

    markAsReinjectedForRelexing(AttributeTokens);
    Info->Tokens =
        llvm::makeArrayRef(AttributeTokens).copy(PP.getPreprocessorAllocator());
  }

  if (Tok.isNot(tok::eod))
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << "clang attribute";

  // Generate the annotated pragma token.
  auto TokenArray = std::make_unique<Token[]>(1);
  TokenArray[0].startToken();
  TokenArray[0].setKind(tok::annot_pragma_attribute);
  TokenArray[0].setLocation(FirstToken.getLocation());
  TokenArray[0].setAnnotationEndLoc(FirstToken.getLocation());
  TokenArray[0].setAnnotationValue(static_cast<void *>(Info));
  PP.EnterTokenStream(std::move(TokenArray), 1,
                      /*DisableMacroExpansion=*/false, /*IsReinject=*/false);
}

// Handle '#pragma clang max_tokens 12345'.
void PragmaMaxTokensHereHandler::HandlePragma(Preprocessor &PP,
                                              PragmaIntroducer Introducer,
                                              Token &Tok) {
  PP.Lex(Tok);
  if (Tok.is(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_missing_argument)
        << "clang max_tokens_here" << /*Expected=*/true << "integer";
    return;
  }

  SourceLocation Loc = Tok.getLocation();
  uint64_t MaxTokens;
  if (Tok.isNot(tok::numeric_constant) ||
      !PP.parseSimpleIntegerLiteral(Tok, MaxTokens)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_expected_integer)
        << "clang max_tokens_here";
    return;
  }

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << "clang max_tokens_here";
    return;
  }

  if (PP.getTokenCount() > MaxTokens) {
    PP.Diag(Loc, diag::warn_max_tokens)
        << PP.getTokenCount() << (unsigned)MaxTokens;
  }
}

// Handle '#pragma clang max_tokens_total 12345'.
void PragmaMaxTokensTotalHandler::HandlePragma(Preprocessor &PP,
                                               PragmaIntroducer Introducer,
                                               Token &Tok) {
  PP.Lex(Tok);
  if (Tok.is(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_missing_argument)
        << "clang max_tokens_total" << /*Expected=*/true << "integer";
    return;
  }

  SourceLocation Loc = Tok.getLocation();
  uint64_t MaxTokens;
  if (Tok.isNot(tok::numeric_constant) ||
      !PP.parseSimpleIntegerLiteral(Tok, MaxTokens)) {
    PP.Diag(Tok.getLocation(), diag::err_pragma_expected_integer)
        << "clang max_tokens_total";
    return;
  }

  if (Tok.isNot(tok::eod)) {
    PP.Diag(Tok.getLocation(), diag::warn_pragma_extra_tokens_at_eol)
        << "clang max_tokens_total";
    return;
  }

  PP.overrideMaxTokens(MaxTokens, Loc);
}
