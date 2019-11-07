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

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtTransform.h"
#include "clang/Basic/Transform.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"

using namespace clang;

StmtResult
Sema::ActOnLoopTransformDirective(Transform::Kind Kind,
                                  llvm::ArrayRef<TransformClause *> Clauses,
                                  Stmt *AStmt, SourceRange Loc) {
  // TOOD: implement
  return StmtError();
}

TransformClause *Sema::ActOnFullClause(SourceRange Loc) {
  // TOOD: implement
  return nullptr;
}

TransformClause *Sema::ActOnPartialClause(SourceRange Loc, Expr *Factor) {
  // TOOD: implement
  return nullptr;
}

TransformClause *Sema::ActOnWidthClause(SourceRange Loc, Expr *Width) {
  // TOOD: implement
  return nullptr;
}

TransformClause *Sema::ActOnFactorClause(SourceRange Loc, Expr *Factor) {
  // TOOD: implement
  return nullptr;
}
