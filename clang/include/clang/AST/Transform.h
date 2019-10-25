//===--- Transform.h - Code transformation classes --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines classes used for code transformations such as
//  #pragma clang transform ...
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_TRANSFORM_H
#define LLVM_CLANG_AST_TRANSFORM_H

#include "llvm/ADT/StringRef.h"

namespace clang {

class Transform {
public:
  enum Kind {
    UnknownKind,
#define TRANSFORM_DIRECTIVE(Keyword, Name) Name##Kind,
#define TRANSFORM_DIRECTIVE_LAST(Keyword, Name)                                \
  TRANSFORM_DIRECTIVE(Keyword, Name)                                           \
  LastKind = Name##Kind
#include "clang/AST/TransformKinds.def"
  };

  static Kind getTransformDirectiveKind(llvm::StringRef Str);
  static llvm::StringRef getTransformDirectiveKeyword(Kind K);
  static llvm::StringRef getTransformDirectiveName(Kind K);

  // TODO: implement
};

} // namespace clang
#endif /* LLVM_CLANG_AST_TRANSFORM_H */
