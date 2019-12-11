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

TransformExecutableDirective *TransformExecutableDirective::create(
    ASTContext &Ctx, SourceRange Range, Stmt *Associated,
    ArrayRef<TransformClause *> Clauses, Transform::Kind TransKind) {
  void *Mem = Ctx.Allocate(totalSizeToAlloc<TransformClause *>(Clauses.size()));
  return new (Mem)
      TransformExecutableDirective(Range, Associated, Clauses, TransKind);
}

TransformExecutableDirective *
TransformExecutableDirective::createEmpty(ASTContext &Ctx,
                                          unsigned NumClauses) {
  void *Mem = Ctx.Allocate(totalSizeToAlloc<TransformClause *>(NumClauses));
  return new (Mem) TransformExecutableDirective(NumClauses);
}

llvm::StringRef TransformClause::getClauseName(Kind K) {
  assert(K >= UnknownKind);
  assert(K <= LastKind);
  const char *Names[LastKind + 1] = {
      "Unknown",
#define TRANSFORM_CLAUSE(Keyword, Name) #Name,
#include "clang/AST/TransformClauseKinds.def"
  };
  return Names[K];
}

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

TransformClause ::child_range TransformClause ::children() {
  switch (getKind()) {
  case UnknownKind:
    llvm_unreachable("Unknown child");
#define TRANSFORM_CLAUSE(Keyword, Name)                                        \
  case TransformClause::Kind::Name##Kind:                                      \
    return static_cast<Name##Clause *>(this)->children();
#include "clang/AST/TransformClauseKinds.def"
  }
  llvm_unreachable("Unhandled clause kind");
}

void TransformClause ::print(llvm::raw_ostream &OS,
                             const PrintingPolicy &Policy) const {
  assert(getKind() > UnknownKind);
  assert(getKind() <= LastKind);
  static decltype(&TransformClause::print) PrintFuncs[LastKind] = {
#define TRANSFORM_CLAUSE(Keyword, Name)                                        \
  static_cast<decltype(&TransformClause::print)>(&Name##Clause ::print),
#include "clang/AST/TransformClauseKinds.def"
  };
  (this->*PrintFuncs[getKind() - 1])(OS, Policy);
}

void FullClause::print(llvm::raw_ostream &OS,
                       const PrintingPolicy &Policy) const {
  OS << "full";
}

void PartialClause::print(llvm::raw_ostream &OS,
                          const PrintingPolicy &Policy) const {
  OS << "partial(";
  Factor->printPretty(OS, nullptr, Policy, 0);
  OS << ')';
}

void WidthClause::print(llvm::raw_ostream &OS,
                        const PrintingPolicy &Policy) const {
  OS << "width(";
  Width->printPretty(OS, nullptr, Policy, 0);
  OS << ')';
}

void FactorClause::print(llvm::raw_ostream &OS,
                         const PrintingPolicy &Policy) const {
  OS << "factor(";
  Factor->printPretty(OS, nullptr, Policy, 0);
  OS << ')';
}

const Stmt *clang::getAssociatedLoop(const Stmt *S) {
  switch (S->getStmtClass()) {
  case Stmt::ForStmtClass:
  case Stmt::WhileStmtClass:
  case Stmt::DoStmtClass:
  case Stmt::CXXForRangeStmtClass:
    return S;
  case Stmt::TransformExecutableDirectiveClass:
    return getAssociatedLoop(
        cast<TransformExecutableDirective>(S)->getAssociated());
  default:
    return nullptr;
  }
}
