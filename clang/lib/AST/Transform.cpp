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

#include "clang/AST/Transform.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Casting.h"

using namespace clang;

Transform::Kind Transform ::getTransformDirectiveKind(llvm::StringRef Str) {
  return llvm::StringSwitch<Transform::Kind>(Str)
#define TRANSFORM_DIRECTIVE(Keyword, Name)                                     \
  .Case(#Keyword, Transform::Kind::Name##Kind)
#include "clang/AST/TransformKinds.def"
      .Default(Transform::UnknownKind);
}

llvm::StringRef Transform ::getTransformDirectiveKeyword(Kind K) {
  assert(K >= UnknownKind);
  assert(K <= LastKind);
  const char *Keywords[LastKind + 1] = {
      "<<Unknown>>",
#define TRANSFORM_DIRECTIVE(Keyword, Name) #Keyword,
#include "clang/AST/TransformKinds.def"
  };
  return Keywords[K];
}

llvm::StringRef Transform ::getTransformDirectiveName(Kind K) {
  assert(K >= UnknownKind);
  assert(K <= LastKind);
  const char *Keywords[LastKind + 1] = {
      "<<Unknown>>",
#define TRANSFORM_DIRECTIVE(Keyword, Name) #Name,
#include "clang/AST/TransformKinds.def"
  };
  return Keywords[K];
}

int Transform::getLoopPipelineStage() const {
  switch (getKind()) {
  case Transform::Kind::LoopDistributionKind:
    return 1;
  case Transform::Kind::LoopUnrollingKind:
    return cast<LoopUnrollingTransform>(this)->isFull() ? 0 : 4;
  case Transform::Kind::LoopInterleavingKind:
  case Transform::Kind::LoopVectorizationKind:
  case Transform::Kind::LoopVectorizationInterleavingKind:
    return 2;
  case Transform::Kind::LoopUnrollAndJamKind:
    return 3;
  default:
    return -1;
  }
}

int Transform::getNumInputs() const {
  assert(getKind() > UnknownKind);
  assert(getKind() <= LastKind);
  static const decltype(
      &Transform::getNumInputs) GetNumInputFuncs[Transform::Kind::LastKind] = {
#define TRANSFORM_DIRECTIVE(Keyword, Name)                                     \
  static_cast<decltype(&Transform::getNumInputs)>(                             \
      &Name##Transform ::getNumInputs),
#include "clang/AST/TransformKinds.def"
  };
  return (this->*GetNumInputFuncs[getKind() - 1])();
}

int Transform::getNumFollowups() const {
  assert(getKind() > UnknownKind);
  assert(getKind() <= LastKind);
  static const decltype(&Transform::getNumInputs)
      GetNumFollowupFuncs[Transform::Kind::LastKind] = {
#define TRANSFORM_DIRECTIVE(Keyword, Name)                                     \
  static_cast<decltype(&Transform::getNumFollowups)>(                          \
      &Name##Transform ::getNumFollowups),
#include "clang/AST/TransformKinds.def"
      };
  return (this->*GetNumFollowupFuncs[getKind() - 1])();
}
