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
      .Case("unroll", LoopUnrollKind)
      .Case("unrollandjam", LoopUnrollAndJamKind)
      .Case("vectorize", LoopVectorizationKind)
      .Case("interleave", LoopInterleavingKind)
      .Case("distribute", LoopDistributionKind)
      .Default(UnknownKind);
}

llvm::StringRef Transform ::getTransformDirectiveKeyword(Kind K) {
  switch (K) {
  case UnknownKind:
    break;
  case LoopUnrollKind:
    return "unroll";
  case LoopUnrollAndJamKind:
    return "unrollandjam";
  case LoopVectorizationKind:
    return "vectorize";
  case LoopInterleavingKind:
    return "interleave";
  case LoopDistributionKind:
    return "distribute";
  }
  llvm_unreachable("Not a known transformation");
}

int Transform::getLoopPipelineStage() const {
  switch (getKind()) {
  case Transform::Kind::LoopUnrollKind:
    return cast<LoopUnrollTransform>(this)->isFull() ? 0 : 4;
  case Transform::Kind::LoopDistributionKind:
    return 1;
  case Transform::Kind::LoopInterleavingKind:
  case Transform::Kind::LoopVectorizationKind:
    return 2;
  case Transform::Kind::LoopUnrollAndJamKind:
    return 3;
  default:
    return -1;
  }
}
