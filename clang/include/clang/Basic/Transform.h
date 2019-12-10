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

#ifndef LLVM_CLANG_BASIC_TRANSFORM_H
#define LLVM_CLANG_BASIC_TRANSFORM_H

#include "clang/AST/Stmt.h"
#include "llvm/ADT/StringRef.h"

namespace clang {

class Transform {
public:
  enum Kind {
    UnknownKind,
#define TRANSFORM_DIRECTIVE(Name) Name##Kind,
#define TRANSFORM_DIRECTIVE_LAST(Name)                                         \
  TRANSFORM_DIRECTIVE(Name)                                                    \
  LastKind = Name##Kind
#include "TransformKinds.def"
  };

  static Kind getTransformDirectiveKind(llvm::StringRef Str);
  static llvm::StringRef getTransformDirectiveKeyword(Kind K);

private:
  Kind TransformKind;
  SourceRange LocRange;

protected:
  Transform(Kind K, SourceRange LocRange)
      : TransformKind(K), LocRange(LocRange) {}

public:
  virtual ~Transform() {}

  Kind getKind() const { return TransformKind; }
  static bool classof(const Transform *Trans) { return true; }

  /// Source location of the code transformation directive.
  /// @{
  SourceRange getRange() const { return LocRange; }
  SourceLocation getBeginLoc() const { return LocRange.getBegin(); }
  SourceLocation getEndLoc() const { return LocRange.getEnd(); }
  void setRange(SourceRange L) { LocRange = L; }
  void setRange(SourceLocation BeginLoc, SourceLocation EndLoc) {
    LocRange = SourceRange(BeginLoc, EndLoc);
  }
  /// @}

  /// Each transformation defines how many loops it consumes and generates.
  /// Users of this class can store arrays holding the information regarding the
  /// loops, such as pointer to the AST node or the loop name. The index in this
  /// array is its "role".
  /// @{
  virtual int getNumInputs() const { return 1; }
  virtual int getNumFollowups() const { return 0; }
  /// @}

  /// A meta role may apply to multiple output loops, its attributes are added
  /// to each of them. A typical axample is the 'all' followup which applies to
  /// all loops emitted by a transformation. The "all" follow-up role is a meta
  /// output whose' attributes are added to all generated loops.
  bool isMetaRole(int R) const { return R == 0; }

  /// Used to warn users that the current LLVM pass pipeline cannot apply
  /// arbitrary transformation orders yet.
  int getLoopPipelineStage() const;
};

class LoopUnrollingTransform final : public Transform {
private:
  bool ImplicitEnable;
  bool ExplicitEnable;
  int64_t Factor;

  LoopUnrollingTransform(SourceRange Loc, bool ImplicitEnable,
                         bool ExplicitEnable, int Factor)
      : Transform(LoopUnrollingKind, Loc), ImplicitEnable(ImplicitEnable),
        ExplicitEnable(ExplicitEnable), Factor(Factor) {}

public:
  static bool classof(const LoopUnrollingTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopUnrollingKind;
  }

  static LoopUnrollingTransform *create(SourceRange Loc, bool ImplicitEnable,
                                        bool ExplicitEnable) {
    return new LoopUnrollingTransform(Loc, ImplicitEnable, ExplicitEnable, 0);
  }
  static LoopUnrollingTransform *
  createFull(SourceRange Loc, bool ImplicitEnable, bool ExplicitEnable) {
    return new LoopUnrollingTransform(Loc, ImplicitEnable, ExplicitEnable, -1);
  }
  static LoopUnrollingTransform *createPartial(SourceRange Loc,
                                               bool ImplicitEnable,
                                               bool ExplicitEnable,
                                               int Factor) {
    return new LoopUnrollingTransform(Loc, ImplicitEnable, ExplicitEnable,
                                      Factor);
  }

  int getNumInputs() const { return 1; }
  int getNumFollowups() const { return 3; }
  enum Input {
    InputToUnroll,
  };
  enum Followup {
    FollowupAll,      // if not full
    FollowupUnrolled, // if not full
    FollowupRemainder
  };

  bool hasFollowupRole(int i) { return isFull(); }
  int getDefaultSuccessor() const { return FollowupUnrolled; }

  bool isImplicitEnable() const { return ImplicitEnable; }

  bool isExplicitEnable() const { return ExplicitEnable; }

  int getFactor() const { return Factor; }

  bool isFull() const { return Factor == -1; }
};

class LoopUnrollAndJamTransform final : public Transform {
private:
  bool ExplicitEnable;
  int Factor;

  LoopUnrollAndJamTransform(SourceRange Loc, bool ExplicitEnable, int Factor)
      : Transform(LoopUnrollAndJamKind, Loc), ExplicitEnable(ExplicitEnable),
        Factor(Factor) {}

public:
  static bool classof(const LoopUnrollAndJamTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopUnrollAndJamKind;
  }

  static LoopUnrollAndJamTransform *create(SourceRange Loc,
                                           bool ExplicitEnable) {
    return new LoopUnrollAndJamTransform(Loc, ExplicitEnable, 0);
  }
  static LoopUnrollAndJamTransform *createFull(SourceRange Loc,
                                               bool ExplicitEnable) {
    return new LoopUnrollAndJamTransform(Loc, ExplicitEnable, -1);
  }
  static LoopUnrollAndJamTransform *
  createPartial(SourceRange Loc, bool ExplicitEnable, int Factor) {
    return new LoopUnrollAndJamTransform(Loc, ExplicitEnable, Factor);
  }

  int getNumInputs() const { return 1; }
  int getNumFollowups() const { return 3; }
  enum Input {
    InputOuter,
    InputInner,
  };
  enum Followup { FollowupAll, FollowupOuter, FollowupInner };

  bool isExplicitEnable() const { return ExplicitEnable; }

  int getFactor() const {
    assert(Factor >= 0);
    return Factor;
  }

  bool isFull() const { return Factor == -1; }
};

class LoopDistributionTransform final : public Transform {
private:
  LoopDistributionTransform(SourceRange Loc)
      : Transform(LoopDistributionKind, Loc) {}

public:
  static bool classof(const LoopDistributionTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopDistributionKind;
  }

  static LoopDistributionTransform *create(SourceRange Loc) {
    return new LoopDistributionTransform(Loc);
  }

  enum Input {
    InputToDistribute,
  };
  enum Followup {
    FollowupAll,
  };
};

class LoopVectorizationTransform final : public Transform {
private:
  llvm::Optional<bool> EnableVectorization;
  int VectorizeWidth;
  llvm::Optional<bool> VectorizePredicateEnable;

  LoopVectorizationTransform(SourceRange Loc,
                             llvm::Optional<bool> EnableVectorization,
                             int VectorizeWidth,
                             llvm::Optional<bool> VectorizePredicateEnable)
      : Transform(LoopVectorizationKind, Loc),
        EnableVectorization(EnableVectorization),
        VectorizeWidth(VectorizeWidth),
        VectorizePredicateEnable(VectorizePredicateEnable) {}

public:
  static bool classof(const LoopVectorizationTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopVectorizationKind;
  }

  static LoopVectorizationTransform *
  create(SourceRange Loc, llvm::Optional<bool> EnableVectorization,
         int VectorizeWidth, llvm::Optional<bool> VectorizePredicateEnable) {
    assert(EnableVectorization.getValueOr(true));
    return new LoopVectorizationTransform(
        Loc, EnableVectorization, VectorizeWidth, VectorizePredicateEnable);
  }

  int getNumInputs() const { return 1; }
  int getNumFollowups() const { return 3; }
  enum Input {
    InputToVectorize,
  };
  enum Followup { FollowupAll, FollowupVectorized, FollowupEpilogue };

  llvm::Optional<bool> isVectorizationEnabled() const {
    return EnableVectorization;
  }

  int getWidth() const { return VectorizeWidth; }

  llvm::Optional<bool> isPredicateEnabled() const {
    return VectorizePredicateEnable;
  }
};

class LoopInterleavingTransform final : public Transform {
private:
  llvm::Optional<bool> EnableInterleaving;
  int InterleaveCount;

  LoopInterleavingTransform(SourceRange Loc,
                            llvm::Optional<bool> EnableInterleaving,
                            int InterleaveCount)
      : Transform(LoopInterleavingKind, Loc),
        EnableInterleaving(EnableInterleaving),
        InterleaveCount(InterleaveCount) {}

public:
  static bool classof(const LoopInterleavingTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopInterleavingKind;
  }

  static LoopInterleavingTransform *
  create(SourceRange Loc, llvm::Optional<bool> EnableInterleaving,
         int InterleaveCount) {
    assert(EnableInterleaving.getValueOr(true));
    return new LoopInterleavingTransform(Loc, EnableInterleaving,
                                         InterleaveCount);
  }

  int getNumInputs() const { return 1; }
  int getNumFollowups() const { return 3; }
  enum Input {
    InputToVectorize,
  };
  enum Followup { FollowupAll, FollowupVectorized, FollowupEpilogue };

  llvm::Optional<bool> isInterleavingEnabled() const {
    return EnableInterleaving;
  }

  int getInterleaveCount() const { return InterleaveCount; }
};

} // namespace clang
#endif /* LLVM_CLANG_BASIC_TRANSFORM_H */
