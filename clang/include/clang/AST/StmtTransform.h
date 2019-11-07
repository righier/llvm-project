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

#ifndef LLVM_CLANG_AST_STMTTRANSFROM_H
#define LLVM_CLANG_AST_STMTTRANSFROM_H

#include "clang/AST/Stmt.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Basic/Transform.h"

namespace clang {

/// Represents a clause of a \p TransformExecutableDirective.
class TransformClause {
public:
  enum Kind {
    UnknownKind,
#define TRANSFORM_CLAUSE(Keyword, Name) Name##Kind,
#define TRANSFORM_CLAUSE_LAST(Keyword, Name) Name##Kind, LastKind = Name##Kind
#include "clang/AST/TransformClauseKinds.def"
  };

  static bool isValidForTransform(Transform::Kind TransformKind,
                                  TransformClause::Kind ClauseKind);
  static Kind getClauseKind(Transform::Kind TransformKind, llvm::StringRef Str);
  static llvm::StringRef getClauseKeyword(TransformClause::Kind ClauseKind);

  // TODO: implement
};

/// Represents
///
///   #pragma clang transform
///
/// in the AST.
class TransformExecutableDirective final {
  // TODO: implement
};

const Stmt *getAssociatedLoop(const Stmt *S);
} // namespace clang

#endif /* LLVM_CLANG_AST_STMTTRANSFROM_H */
