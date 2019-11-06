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
#define TRANSFORM_DIRECTIVE(Keyword, Name) Name##Kind,
#define TRANSFORM_DIRECTIVE_LAST(Keyword, Name)                                \
  TRANSFORM_DIRECTIVE(Keyword, Name)                                           \
  LastKind = Name##Kind
#include "TransformKinds.def"
  };

  static Kind getTransformDirectiveKind(llvm::StringRef Str);
  static llvm::StringRef getTransformDirectiveKeyword(Kind K);
  static llvm::StringRef getTransformDirectiveName(Kind K);

private:
  Kind TransformKind;
  SourceRange LocRange;
  bool IsLegacy;

public:
  Transform(Kind K, SourceRange LocRange, bool IsLegacy)
      : TransformKind(K), LocRange(LocRange), IsLegacy(IsLegacy) {}

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

  /// Non-legacy directives originate from a #pragma clang transform directive.
  /// Legacy transformations are introduced by #pragma clang loop,
  /// #pragma omp simd, #pragma unroll, etc.
  /// Differences include:
  ///  * Legacy directives transformation execution order is defined by the
  ///    compiler.
  ///  * Some warnings that clang historically did not warn about are disabled.
  ///  * Some differences of the emitted loop metadata for compatibility.
  bool isLegacy() const { return IsLegacy; }

  /// Each transformation defines how many loops it consumes and generates.
  /// Users of this class can store arrays holding the information regarding the
  /// loops, such as pointer to the AST node or the loop name. The index in this
  /// array is its "role".
  /// @{
  int getNumInputs() const;
  int getNumFollowups() const;
  /// @}

  /// The "all" follow-up role is a meta output whose' attributes are added to
  /// all generated loops.
  bool isAllRole(int R) const { return R == 0; }

  /// Used to warn users that the current LLVM pass pipeline cannot apply
  /// arbitrary transformation orders yet.
  int getLoopPipelineStage() const;
};

/// Default implementation of compile-time inherited methods to avoid infinite
/// recursion.
class TransformImpl : public Transform {
public:
  TransformImpl(Kind K, SourceRange Loc, bool IsLegacy)
      : Transform(K, Loc, IsLegacy) {}

  int getNumInputs() const { return 1; }
  int getNumFollowups() const { return 1; }
};

class LoopDistributionTransform final : public TransformImpl {
private:
  LoopDistributionTransform(SourceRange Loc, bool IsLegacy)
      : TransformImpl(LoopDistributionKind, Loc, IsLegacy) {}

public:
  static bool classof(const LoopDistributionTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopDistributionKind;
  }

  static LoopDistributionTransform *create(SourceRange Loc, bool IsLegacy) {
    return new LoopDistributionTransform(Loc, IsLegacy);
  }

  enum Input {
    InputToDistribute,
  };
  enum Followup {
    FollowupAll,
  };
};

class LoopVectorizationTransform final : public TransformImpl {
private:
  llvm::Optional<bool> EnableVectorization;
  int VectorizeWidth;
  llvm::Optional<bool> VectorizePredicateEnable;

  LoopVectorizationTransform(SourceRange Loc,
                             llvm::Optional<bool> EnableVectorization,
                             int VectorizeWidth,
                             llvm::Optional<bool> VectorizePredicateEnable)
      : TransformImpl(LoopVectorizationKind, Loc, true),
        EnableVectorization(EnableVectorization),
        VectorizeWidth(VectorizeWidth),
        VectorizePredicateEnable(VectorizePredicateEnable) {}

public:
  static bool classof(const LoopVectorizationTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopVectorizationKind;
  }

  static LoopVectorizationTransform *
  Create(SourceRange Loc, llvm::Optional<bool> EnableVectorization,
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

class LoopInterleavingTransform final : public TransformImpl {
private:
  llvm::Optional<bool> EnableInterleaving;
  int InterleaveCount;

  LoopInterleavingTransform(SourceRange Loc,
                            llvm::Optional<bool> EnableInterleaving,
                            int InterleaveCount)
      : TransformImpl(LoopInterleavingKind, Loc, false),
        EnableInterleaving(EnableInterleaving),
        InterleaveCount(InterleaveCount) {}

public:
  static bool classof(const LoopInterleavingTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopInterleavingKind;
  }

  static LoopInterleavingTransform *
  Create(SourceRange Loc, llvm::Optional<bool> EnableInterleaving,
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

class LoopVectorizationInterleavingTransform final : public TransformImpl {
private:
  bool AssumeSafety;
  llvm::Optional<bool> EnableVectorization;
  llvm::Optional<bool> EnableInterleaving;
  int VectorizeWidth;
  llvm::Optional<bool> VectorizePredicateEnable;
  int InterleaveCount;

  LoopVectorizationInterleavingTransform(
      SourceRange Loc, bool IsLegacy, bool AssumeSafety,
      llvm::Optional<bool> EnableVectorization,
      llvm::Optional<bool> EnableInterleaving, int VectorizeWidth,
      llvm::Optional<bool> VectorizePredicateEnable, int InterleaveCount)
      : TransformImpl(LoopVectorizationInterleavingKind, Loc, IsLegacy),
        AssumeSafety(AssumeSafety), EnableVectorization(EnableVectorization),
        EnableInterleaving(EnableInterleaving), VectorizeWidth(VectorizeWidth),
        VectorizePredicateEnable(VectorizePredicateEnable),
        InterleaveCount(InterleaveCount) {}

public:
  static bool classof(const LoopVectorizationInterleavingTransform *Trans) {
    return true;
  }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopVectorizationInterleavingKind;
  }

  static LoopVectorizationInterleavingTransform *
  create(SourceRange Loc, bool Legacy, bool AssumeSafety,
         llvm::Optional<bool> EnableVectorization,
         llvm::Optional<bool> EnableInterleaving, int VectorizeWidth,
         llvm::Optional<bool> VectorizePredicateEnable, int InterleaveCount) {
    assert(EnableVectorization.getValueOr(true) ||
           EnableInterleaving.getValueOr(true));
    return new LoopVectorizationInterleavingTransform(
        Loc, Legacy, AssumeSafety, EnableVectorization, EnableInterleaving,
        VectorizeWidth, VectorizePredicateEnable, InterleaveCount);
  }

  int getNumInputs() const { return 1; }
  int getNumFollowups() const { return 3; }
  enum Input {
    InputToVectorize,
  };
  enum Followup { FollowupAll, FollowupVectorized, FollowupEpilogue };

  bool isAssumeSafety() const { return AssumeSafety; }
  llvm::Optional<bool> isVectorizationEnabled() const {
    return EnableVectorization;
  }
  llvm::Optional<bool> isInterleavingEnabled() const {
    return EnableInterleaving;
  }

  int getWidth() const { return VectorizeWidth; }
  llvm::Optional<bool> isPredicateEnabled() const {
    return VectorizePredicateEnable;
  }
  int getInterleaveCount() const { return InterleaveCount; }
};

class LoopUnrollingTransform final : public TransformImpl {
private:
  bool ImplicitEnable;
  bool ExplicitEnable;
  int64_t Factor;

  LoopUnrollingTransform(SourceRange Loc, bool IsLegacy, bool ImplicitEnable,
                         bool ExplicitEnable, int Factor)
      : TransformImpl(LoopUnrollingKind, Loc, IsLegacy),
        ImplicitEnable(ImplicitEnable), ExplicitEnable(ExplicitEnable),
        Factor(Factor) {}

public:
  static bool classof(const LoopUnrollingTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopUnrollingKind;
  }

  static LoopUnrollingTransform *create(SourceRange Loc, bool Legacy,
                                        bool ImplicitEnable,
                                        bool ExplicitEnable) {
    return new LoopUnrollingTransform(Loc, Legacy, ImplicitEnable,
                                      ExplicitEnable, 0);
  }
  static LoopUnrollingTransform *createFull(SourceRange Loc, bool Legacy,
                                            bool ImplicitEnable,
                                            bool ExplicitEnable) {
    return new LoopUnrollingTransform(Loc, Legacy, ImplicitEnable,
                                      ExplicitEnable, -1);
  }
  static LoopUnrollingTransform *createPartial(SourceRange Loc, bool Legacy,
                                               bool ImplicitEnable,
                                               bool ExplicitEnable,
                                               int Factor) {
    return new LoopUnrollingTransform(Loc, Legacy, ImplicitEnable,
                                      ExplicitEnable, Factor);
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
  int getDefaultSuccessor() const {
    return isLegacy() ? FollowupAll : FollowupUnrolled;
  }

  bool isImplicitEnable() const { return ImplicitEnable; }

  bool isExplicitEnable() const { return ExplicitEnable; }

  int getFactor() const { return Factor; }

  bool isFull() const { return Factor == -1; }
};

class LoopUnrollAndJamTransform final : public TransformImpl {
private:
  bool ExplicitEnable;
  int Factor;

  LoopUnrollAndJamTransform(SourceRange Loc, bool IsLegacy, bool ExplicitEnable,
                            int Factor)
      : TransformImpl(LoopUnrollAndJamKind, Loc, IsLegacy),
        ExplicitEnable(ExplicitEnable), Factor(Factor) {}

public:
  static bool classof(const LoopUnrollAndJamTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopUnrollAndJamKind;
  }

  static LoopUnrollAndJamTransform *create(SourceRange Loc, bool Legacy,
                                           bool ExplicitEnable) {
    return new LoopUnrollAndJamTransform(Loc, Legacy, ExplicitEnable, 0);
  }
  static LoopUnrollAndJamTransform *createFull(SourceRange Loc, bool Legacy,
                                               bool ExplicitEnable) {
    return new LoopUnrollAndJamTransform(Loc, Legacy, ExplicitEnable, -1);
  }
  static LoopUnrollAndJamTransform *
  createPartial(SourceRange Loc, bool Legacy, bool ExplicitEnable, int Factor) {
    return new LoopUnrollAndJamTransform(Loc, Legacy, ExplicitEnable, Factor);
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

class LoopPipeliningTransform final : public TransformImpl {
private:
  int InitiationInterval;

  LoopPipeliningTransform(SourceRange Loc, int InitiationInterval)
      : TransformImpl(LoopPipeliningKind, Loc, true),
        InitiationInterval(InitiationInterval) {}

public:
  static bool classof(const LoopPipeliningTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopPipeliningKind;
  }

  static LoopPipeliningTransform *create(SourceRange Loc,
                                         int InitiationInterval) {
    return new LoopPipeliningTransform(Loc, InitiationInterval);
  }

  int getNumInputs() const { return 1; }
  int getNumFollowups() const { return 0; }
  enum Input {
    InputToPipeline,
  };

  int getInitiationInterval() const { return InitiationInterval; }
};

class LoopAssumeParallelTransform final : public TransformImpl {
private:
  LoopAssumeParallelTransform(SourceRange Loc)
      : TransformImpl(LoopAssumeParallelKind, Loc, false) {}

public:
  static bool classof(const LoopAssumeParallelTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopAssumeParallelKind;
  }

  static LoopAssumeParallelTransform *create(SourceRange Loc) {
    return new LoopAssumeParallelTransform(Loc);
  }

  int getNumInputs() const { return 1; }
  int getNumFollowups() const { return 1; }
  enum Input {
    InputParallel,
  };
  enum Followup {
    FollowupParallel,
  };
};

} // namespace clang
#endif /* LLVM_CLANG_BASIC_TRANSFORM_H */
