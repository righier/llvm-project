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
  /// to each of them. A typical example is the 'all' followup which applies to
  /// all loops emitted by a transformation. The "all" follow-up role is a meta
  /// output whose' attributes are added to all generated loops.
  bool isMetaRole(int R) const { return R == 0; }

  /// Used to warn users that the current LLVM pass pipeline cannot apply
  /// arbitrary transformation orders yet.
  int getLoopPipelineStage() const;
};

/// Partially or fully unroll a loop.
///
/// A full unroll transforms a loop such as
///
///     for (int i = 0; i < 2; i+=1)
///       Stmt(i);
///
/// into
///
///     {
///       Stmt(0);
///       Stmt(1);
///     }
///
/// Partial unrolling can also be applied when the loop trip count is only known
/// at runtime. For instance, partial unrolling by a factor of 2 transforms
///
///     for (int i = 0; i < N; i+=1)
///       Stmt(i);
///
/// into
///
///     int i = 0;
///     for (; i < N; i+=2) { // unrolled
///       Stmt(i);
///       Stmt(i+1);
///     }
///     for (; i < N; i+=1) // epilogue/remainder
///       Stmt(i);
///
/// LLVM's LoopUnroll pass uses the name runtime unrolling if N is not a
/// constant.
///
/// When using heuristic unrolling, the optimizer decides itself whether to
/// unroll fully or partially. Because the front-end does not know what the
/// optimizer will do, there is no followup loop. Note that this is different to
/// partial unrolling with an undefined factor, which has always has followup
/// loops but may not be executed.
class LoopUnrollTransform final : public Transform {
private:
  int64_t Factor;

  LoopUnrollTransform(SourceRange Loc, int64_t Factor)
      : Transform(LoopUnrollKind, Loc), Factor(Factor) {
    assert(Factor >= 2);
  }

public:
  static bool classof(const LoopUnrollTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopUnrollKind;
  }

  /// Create an instance of partial unrolling. The unroll factor must be at
  /// least 2 or -1. When -1, the unroll factor can be chosen by the optimizer.
  /// An unroll factor of 0 or 1 is not valid.
  static LoopUnrollTransform *createPartial(SourceRange Loc,
                                            int64_t Factor = -1) {
    assert(Factor >= 2 || Factor == -1);
    LoopUnrollTransform *Instance = new LoopUnrollTransform(Loc, Factor);
    assert(Instance->isPartial());
    return Instance;
  }

  static LoopUnrollTransform *createFull(SourceRange Loc) {
    LoopUnrollTransform *Instance = new LoopUnrollTransform(Loc, -2);
    assert(Instance->isFull());
    return Instance;
  }

  static LoopUnrollTransform *createHeuristic(SourceRange Loc) {
    LoopUnrollTransform *Instance = new LoopUnrollTransform(Loc, -3);
    assert(Instance->isHeuristic());
    return Instance;
  }

  bool isPartial() const { return Factor >= 2 || Factor == -1; }
  bool isFull() const { return Factor == -2; }
  bool isHeuristic() const { return Factor == -3; }

  enum Input { InputToUnroll };
  int getNumInputs() const override { return 1; }

  enum Followup {
    FollowupAll,
    FollowupUnrolled, // only for partial unrolling
    FollowupRemainder // only for partial unrolling
  };
  int getNumFollowups() const override {
    if (isPartial())
      return 3;
    return 0;
  }

  int64_t getFactor() const { return Factor; }
};

/// Apply partial unroll-and-jam to a loop.
///
/// That is, with a unroll factor of 2, transform
///
///     for (int i = 0; i < N; i+=1)
///        for (int j = 0; j < M; j+=1)
///          Stmt(i,j);
///
/// into
///
///     int i = 0;
///     for (; i < N; i+=2) {             // inner
///        for (int j = 0; j < M; j+=1) { // outer
///          Stmt(i,j);
///          Stmt(i+1,j);
///       }
///     for (; i < N; i+=1)               // remainder/epilogue
///        for (int j = 0; j < M; j+=1)
///          Stmt(i,j);
///
/// Note that LLVM's LoopUnrollAndJam pass does not support full unroll.
class LoopUnrollAndJamTransform final : public Transform {
private:
  int64_t Factor;

  LoopUnrollAndJamTransform(SourceRange Loc, int64_t Factor)
      : Transform(LoopUnrollAndJamKind, Loc), Factor(Factor) {}

public:
  static bool classof(const LoopUnrollAndJamTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopUnrollAndJamKind;
  }

  /// Create an instance of unroll-and-jam. The unroll factor must be at least 2
  /// or -1. When -1, the unroll factor can be chosen by the optimizer. An
  /// unroll factor of 0 or 1 is not valid.
  static LoopUnrollAndJamTransform *createPartial(SourceRange Loc,
                                                  int64_t Factor = -1) {
    LoopUnrollAndJamTransform *Instance =
        new LoopUnrollAndJamTransform(Loc, Factor);
    assert(Instance->isPartial());
    return Instance;
  }

  static LoopUnrollAndJamTransform *createHeuristic(SourceRange Loc) {
    LoopUnrollAndJamTransform *Instance =
        new LoopUnrollAndJamTransform(Loc, -3);
    assert(Instance->isHeuristic());
    return Instance;
  }

  bool isPartial() const { return Factor >= 2 || Factor == -1; }
  bool isHeuristic() const { return Factor == -3; }

  enum Input { InputOuter, InputInner };
  int getNumInputs() const override { return 2; }

  enum Followup { FollowupAll, FollowupOuter, FollowupInner };
  int getNumFollowups() const override {
    if (isPartial())
      return 3;
    return 1;
  }

  int64_t getFactor() const { return Factor; }
};

/// Apply loop distribution (aka fission) to a loop.
///
/// For example, transform the loop
///
///     for (int i = 0; i < N; i+=1) {
///       StmtA(i);
///       StmtB(i);
///     }
///
/// into
///
///     for (int i = 0; i < N; i+=1)
///       StmtA(i);
///     for (int i = 0; i < N; i+=1)
///       StmtB(i);
///
/// LLVM's LoopDistribute pass does not allow to control how the loop is
/// distributed. Hence, there are no non-meta followups.
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

  enum Input { InputToDistribute };
  int getNumInputs() const override { return 1; }

  enum Followup { FollowupAll };
  int getNumFollowups() const override { return 1; }
};

/// Vectorize a loop by executing multiple loop iterations at the same time in
/// vector lanes.
///
/// For example, transform
///
///     for (int i = 0; i < N; i+=1)
///       Stmt(i);
///
/// into
///
///     int i = 0;
///     for (; i < N; i+=2) // vectorized
///       Stmt(i:i+1);
///     for (; i < N; i+=1) // epilogue/remainder
///       Stmt(i);
class LoopVectorizationTransform final : public Transform {
private:
  int64_t VectorizeWidth;

  LoopVectorizationTransform(SourceRange Loc, int64_t VectorizeWidth)
      : Transform(LoopVectorizationKind, Loc), VectorizeWidth(VectorizeWidth) {
    assert(VectorizeWidth >= 2);
  }

public:
  static bool classof(const LoopVectorizationTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopVectorizationKind;
  }

  static LoopVectorizationTransform *create(SourceRange Loc,
                                            int64_t VectorizeWidth = -1) {
    assert(VectorizeWidth >= 2 || VectorizeWidth == -1);
    return new LoopVectorizationTransform(Loc, VectorizeWidth);
  }

  enum Input { InputToVectorize };
  int getNumInputs() const override { return 1; }

  enum Followup { FollowupAll, FollowupVectorized, FollowupEpilogue };
  int getNumFollowups() const override { return 3; }

  int64_t getWidth() const { return VectorizeWidth; }
};

/// Execute multiple loop iterations at once by duplicating instructions. This
/// is different from unrolling in that it copies each instruction n times
/// instead of the entire loop body as loop unrolling does.
///
/// For example, transform
///
///     for (int i = 0; i < N; i+=1) {
///       InstA(i);
///       InstB(i);
///       InstC(i);
///     }
///
/// into
///
///     int i = 0;
///     for (; i < N; i+=2) { // interleaved
///       InstA(i);
///       InstA(i+1);
///       InstB(i);
///       InstB(i+1);
///       InstC(i);
///       InstC(i+1);
///     }
///     for (; i < N; i+=1) // epilogue/remainder
///       InstA(i);
///       InstB(i);
///       InstC(i);
///     }
class LoopInterleavingTransform final : public Transform {
private:
  int64_t Factor;

  LoopInterleavingTransform(SourceRange Loc, int64_t Factor)
      : Transform(LoopInterleavingKind, Loc), Factor(Factor) {}

public:
  static bool classof(const LoopInterleavingTransform *Trans) { return true; }
  static bool classof(const Transform *Trans) {
    return Trans->getKind() == LoopInterleavingKind;
  }

  static LoopInterleavingTransform *create(SourceRange Loc, int64_t Factor) {
    assert(Factor == -1 || Factor >= 2);
    return new LoopInterleavingTransform(Loc, Factor);
  }

  enum Input { InputToVectorize };
  int getNumInputs() const override { return 1; }

  enum Followup { FollowupAll, FollowupInterleaved, FollowupEpilogue };
  int getNumFollowups() const override { return 3; }

  int64_t getFactor() const { return Factor; }
};

} // namespace clang
#endif /* LLVM_CLANG_BASIC_TRANSFORM_H */
