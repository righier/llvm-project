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

#include "clang/Basic/Transform.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Casting.h"

using namespace clang;

Transform::Kind Transform ::getTransformDirectiveKind(llvm::StringRef Str) {
  return llvm::StringSwitch<Transform::Kind>(Str)
#define TRANSFORM_DIRECTIVE(Keyword, Name)                                     \
  .Case(#Keyword, Transform::Kind::Name##Kind)
#include "clang/Basic/TransformKinds.def"
      .Default(Transform::UnknownKind);
}

llvm::StringRef Transform ::getTransformDirectiveKeyword(Kind K) {
  assert(K >= UnknownKind);
  assert(K <= LastKind);
  const char *Keywords[LastKind + 1] = {
      "<<Unknown>>",
#define TRANSFORM_DIRECTIVE(Keyword, Name) #Keyword,
#include "clang/Basic/TransformKinds.def"
  };
  return Keywords[K];
}

llvm::StringRef Transform ::getTransformDirectiveName(Kind K) {
  assert(K >= UnknownKind);
  assert(K <= LastKind);
  const char *Keywords[LastKind + 1] = {
      "<<Unknown>>",
#define TRANSFORM_DIRECTIVE(Keyword, Name) #Name,
#include "clang/Basic/TransformKinds.def"
  };
  return Keywords[K];
}
