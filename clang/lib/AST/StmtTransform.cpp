//===--- StmtTransform.h - Code transformation AST nodes --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  Transformation directive statement and clauses for the AST.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/StmtTransform.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtOpenMP.h"

using namespace clang;

bool TransformClause::isValidForTransform(Transform::Kind TransformKind,
                                          TransformClause::Kind ClauseKind) {
  switch (TransformKind) {
  case clang::Transform::LoopUnrollKind:
    return ClauseKind == PartialKind || ClauseKind == FullKind;
  case clang::Transform::LoopUnrollAndJamKind:
    return ClauseKind == PartialKind;
  case clang::Transform::LoopVectorizationKind:
    return ClauseKind == WidthKind;
  case clang::Transform::LoopInterleavingKind:
    return ClauseKind == FactorKind;
  default:
    return false;
  }
}

TransformClause::Kind
TransformClause ::getClauseKind(Transform::Kind TransformKind,
                                llvm::StringRef Str) {
#define TRANSFORM_CLAUSE(Keyword, Name)                                        \
  if (isValidForTransform(TransformKind, TransformClause::Kind::Name##Kind) && \
      Str == #Keyword)                                                         \
    return TransformClause::Kind::Name##Kind;
#include "clang/AST/TransformClauseKinds.def"
  return TransformClause::UnknownKind;
}

llvm::StringRef
TransformClause ::getClauseKeyword(TransformClause::Kind ClauseKind) {
  assert(ClauseKind > UnknownKind);
  assert(ClauseKind <= LastKind);
  static const char *ClauseKeyword[LastKind] = {
#define TRANSFORM_CLAUSE(Keyword, Name) #Keyword,
#include "clang/AST/TransformClauseKinds.def"

  };
  return ClauseKeyword[ClauseKind - 1];
}

const Stmt *clang::getAssociatedLoop(const Stmt *S) {
  switch (S->getStmtClass()) {
  case Stmt::ForStmtClass:
  case Stmt::WhileStmtClass:
  case Stmt::DoStmtClass:
  case Stmt::CXXForRangeStmtClass:
    return S;
  default:
    return nullptr;
  }
}
