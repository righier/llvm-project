//===---- ParseTransform.h -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Parse #pragma clang transform ...
//
//===----------------------------------------------------------------------===//

#include "clang/AST/StmtTransform.h"
#include "clang/Parse/Parser.h"
#include "clang/Parse/RAIIObjectsForParser.h"

using namespace clang;

Transform::Kind
Parser::tryParsePragmaTransform(SourceLocation BeginLoc,
                                ParsedStmtContext StmtCtx,
                                SmallVectorImpl<TransformClause *> &Clauses) {
  // ... Tok=<transform> | <...> tok::annot_pragma_transform_end ...
  if (Tok.isNot(tok::identifier)) {
    Diag(Tok, diag::err_pragma_transform_expected_directive);
    return Transform::UnknownKind;
  }
  std::string DirectiveStr = PP.getSpelling(Tok);
  Transform::Kind DirectiveKind =
      Transform::getTransformDirectiveKind(DirectiveStr);
  ConsumeToken();

  switch (DirectiveKind) {
  case Transform::LoopUnrollKind:
  case Transform::LoopUnrollAndJamKind:
  case Transform::LoopDistributionKind:
  case Transform::LoopVectorizationKind:
  case Transform::LoopInterleavingKind:
    break;
  default:
    Diag(Tok, diag::err_pragma_transform_unknown_directive);
    return Transform::UnknownKind;
  }

  while (true) {
    TransformClauseResult Clause = ParseTransformClause(DirectiveKind);
    if (Clause.isInvalid())
      return Transform::UnknownKind;
    if (!Clause.isUsable())
      break;

    Clauses.push_back(Clause.get());
  }

  assert(Tok.is(tok::annot_pragma_transform_end));
  return DirectiveKind;
}

StmtResult Parser::ParsePragmaTransform(ParsedStmtContext StmtCtx) {
  assert(Tok.is(tok::annot_pragma_transform) && "Not a transform directive!");

  // ... Tok=annot_pragma_transform | <trans> <...> annot_pragma_transform_end
  // ...
  SourceLocation BeginLoc = ConsumeAnnotationToken();

  ParenBraceBracketBalancer BalancerRAIIObj(*this);

  SmallVector<TransformClause *, 8> DirectiveClauses;
  Transform::Kind DirectiveKind =
      tryParsePragmaTransform(BeginLoc, StmtCtx, DirectiveClauses);
  if (DirectiveKind == Transform::UnknownKind) {
    SkipUntil(tok::annot_pragma_transform_end);
    return StmtError();
  }

  assert(Tok.is(tok::annot_pragma_transform_end));
  SourceLocation EndLoc = ConsumeAnnotationToken();

  SourceLocation PreStmtLoc = Tok.getLocation();
  StmtResult AssociatedStmt = ParseStatement();
  if (AssociatedStmt.isInvalid())
    return AssociatedStmt;
  if (!getAssociatedLoop(AssociatedStmt.get()))
    return StmtError(
        Diag(PreStmtLoc, diag::err_pragma_transform_expected_loop));

  return Actions.ActOnLoopTransformDirective(DirectiveKind, DirectiveClauses,
                                             AssociatedStmt.get(),
                                             {BeginLoc, EndLoc});
}

Parser::TransformClauseResult
Parser::ParseTransformClause(Transform::Kind TransformKind) {
  // No more clauses
  if (Tok.is(tok::annot_pragma_transform_end))
    return ClauseEmpty();

  SourceLocation StartLoc = Tok.getLocation();
  if (Tok.isNot(tok::identifier))
    return ClauseError(Diag(Tok, diag::err_pragma_transform_expected_clause));
  std::string ClauseKeyword = PP.getSpelling(Tok);
  ConsumeToken();
  TransformClause::Kind Kind =
      TransformClause::getClauseKind(TransformKind, ClauseKeyword);

  switch (Kind) {
  case TransformClause::UnknownKind:
    return ClauseError(Diag(Tok, diag::err_pragma_transform_unknown_clause));

    // Clauses without arguments.
  case TransformClause::FullKind:
    return Actions.ActOnFullClause(SourceRange{StartLoc, StartLoc});

    // Clauses with integer argument.
  case TransformClause::PartialKind:
  case TransformClause::WidthKind:
  case TransformClause::FactorKind: {
    BalancedDelimiterTracker T(*this, tok::l_paren,
                               tok::annot_pragma_transform_end);
    if (T.expectAndConsume(diag::err_expected_lparen_after,
                           ClauseKeyword.data()))
      return ClauseError();

    ExprResult Expr = ParseConstantExpression();
    if (Expr.isInvalid())
      return ClauseError();

    if (T.consumeClose())
      return ClauseError();
    SourceLocation EndLoc = T.getCloseLocation();
    SourceRange Range{StartLoc, EndLoc};
    switch (Kind) {
    case TransformClause::PartialKind:
      return Actions.ActOnPartialClause(Range, Expr.get());
    case TransformClause::WidthKind:
      return Actions.ActOnWidthClause(Range, Expr.get());
    case TransformClause::FactorKind:
      return Actions.ActOnFactorClause(Range, Expr.get());
    default:
      llvm_unreachable("Unhandled clause");
    }
  }
  }
  llvm_unreachable("Unhandled clause");
}
