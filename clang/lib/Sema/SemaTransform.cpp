//===---- SemaTransform.h ------------------------------------- -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Semantic analysis for code transformations.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/SemaTransform.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtTransform.h"
#include "clang/Analysis/AnalysisTransform.h"
#include "clang/Analysis/TransformedTree.h"
#include "clang/Basic/Transform.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"

using namespace clang;

struct SemaExtractTransform : ExtractTransform<SemaExtractTransform> {
  Sema &Sem;
  SemaExtractTransform(TransformExecutableDirective *Directive, Sema &Sem)
      : ExtractTransform(Sem.getASTContext(), Directive), Sem(Sem) {}

  auto Diag(SourceLocation Loc, unsigned DiagID) {
    return Sem.Diag(Loc, DiagID);
  }
};

StmtResult
Sema::ActOnLoopTransformDirective(Transform::Kind Kind,
                                  llvm::ArrayRef<TransformClause *> Clauses,
                                  Stmt *AStmt, SourceRange Loc) {
  const Stmt *Loop = getAssociatedLoop(AStmt);
  if (!Loop)
    return StmtError(
        Diag(Loc.getBegin(), diag::err_sema_transform_expected_loop));

  auto *Result =
      TransformExecutableDirective::create(Context, Loc, AStmt, Clauses, Kind);

  // Emit errors and warnings.
  SemaExtractTransform VerifyTransform(Result, *this);
  VerifyTransform.createTransform();

  return Result;
}

TransformClause *Sema::ActOnFullClause(SourceRange Loc) {
  return FullClause::create(Context, Loc);
}

TransformClause *Sema::ActOnPartialClause(SourceRange Loc, Expr *Factor) {
  return PartialClause::create(Context, Loc, Factor);
}

TransformClause *Sema::ActOnWidthClause(SourceRange Loc, Expr *Width) {
  return WidthClause::create(Context, Loc, Width);
}

TransformClause *Sema::ActOnFactorClause(SourceRange Loc, Expr *Factor) {
  return FactorClause::create(Context, Loc, Factor);
}

void Sema::HandleLoopTransformations(FunctionDecl *FD) {
  if (!FD || FD->isInvalidDecl())
    return;

  // Note: this is called on a template-code and the instantiated code.
  llvm::DenseMap<Stmt *, SemaTransformedTree *> StmtToTree;
  llvm::SmallVector<SemaTransformedTree *, 64> AllNodes;
  llvm::SmallVector<Transform *, 64> AllTransforms;
  SemaTransformedTreeBuilder Builder(getASTContext(), getLangOpts(), AllNodes,
                                     AllTransforms, *this);
  Builder.computeTransformedStructure(FD->getBody(), StmtToTree);

  for (auto N : AllNodes)
    delete N;
  for (auto T : AllTransforms)
    delete T;
}
