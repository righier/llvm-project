//===--- TransformedTree.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  Applies code transformations.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_TRANSFORMEDTREE_H
#define LLVM_CLANG_ANALYSIS_TRANSFORMEDTREE_H

#include "clang/AST/OpenMPClause.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtOpenMP.h"
#include "clang/Analysis/AnalysisTransform.h"
#include "clang/Basic/DiagnosticSema.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {
template <typename Derived, typename NodeTy> class TransformedTreeBuilder;

struct DefaultExtractTransform : ExtractTransform<DefaultExtractTransform> {
  DefaultExtractTransform(ASTContext &ASTCtx,
                          const TransformExecutableDirective *Directive)
      : ExtractTransform(ASTCtx, Directive) {}

  // Ignore any diagnostic and its arguments.
  struct DummyDiag {
    template <typename T> DummyDiag operator<<(const T &) const { return {}; }
  };
  DummyDiag Diag(SourceLocation Loc, unsigned DiagID) { return {}; }
};

/// Represents an input of a code representation.
/// Current can reference the input code only by the AST node, but in the future
/// loops can also given identifiers to reference them.
class TransformInput {
  const Stmt *StmtInput = nullptr;
  Transform* PrecTrans = nullptr;
  int FollowupIdx = -1;

  TransformInput(const Stmt *StmtInput, Transform *PrecTrans, int FollowupIdx) : StmtInput(StmtInput), PrecTrans(PrecTrans), FollowupIdx(FollowupIdx)  {}

public:
  TransformInput() {}

  static TransformInput createByStmt(const Stmt *StmtInput) {
    assert(StmtInput);
    return TransformInput(StmtInput,nullptr, -1);
  }

  // In general, the same clang::Transform can be reused multiple times with different inputs, when referencing its followup using this constructor, the clang::Transform can only be used once per function to ensure that its followup can be uniquely identified.
  static TransformInput createByFollowup(Transform *Transform, int FollowupIdx) {
    assert(Transform);
    assert(0 <= FollowupIdx && FollowupIdx < Transform->getNumFollowups());
    return TransformInput(nullptr, Transform, FollowupIdx );
  }

  bool isByStmt() const { return StmtInput; }
  bool isByFollowup() const { return PrecTrans; }

  const Stmt *getStmtInput() const { return StmtInput; }

  Transform* getPrecTrans() const { return PrecTrans; }
  int getFollowupIdx() const { return FollowupIdx; }
};


/// Represents a transformation together with the input loops.
/// in the future it will also identify the generated loop.
struct NodeTransform {
  Transform *Trans = nullptr;
  llvm::SmallVector<TransformInput, 2> Inputs;

  NodeTransform() {}
  NodeTransform(Transform *Trans, TransformInput TopLevelInput) : Trans(Trans) {
    assert(Trans->getNumInputs() >= 1);

    Inputs.resize(Trans->getNumInputs());
    setInput(0, TopLevelInput);
  };

  void setInput(int Idx, TransformInput Input) { Inputs[Idx] = Input; }
};

/// This class represents a loop in a loop nest to which transformations are
/// applied to. It is intended to be instantiated for it specific purpose, that
/// is SemaTransformedTree for semantic analysis (consistency warnings and
/// errors) and CGTransformedTree for emitting IR.
template <typename Derived> class TransformedTree {
  template <typename, typename> friend class TransformedTreeBuilder;
  using NodeTy = Derived;

  Derived &getDerived() { return *static_cast<Derived *>(this); }
  const Derived &getDerived() const {
    return *static_cast<const Derived *>(this);
  }

protected:
  /// Is this the root node of the loop hierarchy?
  bool IsRoot = false;

  /// Nested loops.
  llvm::SmallVector<Derived *, 2> Subloops;

  /// Origin of this loop.
  /// @{

  /// If not the result of the transformation, this is the loop statement that
  /// this node represents.
  Stmt *Original;

  Derived *BasedOn;
  int FollowupRole;
  /// @}

  /// Things applied to this loop.
  /// @{

  /// If this is the primary transformation input.
  Transform *TransformedBy = nullptr;

  // Primary/secondary transformation input
  Derived *PrimaryInput = nullptr;

  llvm::SmallVector<Derived *, 4> Followups;

  // Derived *Successor = nullptr;
  // First successor is the primary (the one that #pragma clang transform is applied to)
  llvm::SmallVector<Derived*, 2> Successors;

  int InputRole = -1;

  bool IsParallel = false;
  bool HasLegacyDisable = false;
  /// @}

protected:
  TransformedTree(llvm::ArrayRef<Derived *> SubLoops, Derived *BasedOn,
                  clang::Stmt *Original, int FollowupRole)
      : Subloops(SubLoops.begin(), SubLoops.end()), Original(Original),
        BasedOn(BasedOn), FollowupRole(FollowupRole) {}

public:
  ~TransformedTree() {
    int a = 0;
  }

  ArrayRef<Derived *> getSubLoops() const { return Subloops; }

  std::vector<Derived*> getLatestSubLoops() {
    std::vector<Derived*> Result;
    Result.reserve(Subloops.size());
    for (auto SubL : Subloops) {
      // TODO: Efficiency
      auto SubLatest = SubL->getLatestSuccessors();
      Result.insert(Result.end(), SubLatest.begin(), SubLatest.end());
    }
    return Result;
  }

  Derived *getPrimaryInput() const { return PrimaryInput; }
  Transform *getTransformedBy() const { return TransformedBy; }

  /// Return the transformation that generated this loop. Return nullptr if not
  /// the result of any transformation, i.e. it is an original loop.
  Transform *getSourceTransformation() const {
    assert(!BasedOn == isOriginal() &&
           "Non-original loops must be based on some other loop");
    if (isOriginal())
      return nullptr;

    assert(BasedOn);
    assert(BasedOn->isTransformationInput());
    Transform *Result = BasedOn->PrimaryInput->TransformedBy;
    assert(Result &&
           "Non-original loops must have a generating transformation");
    return Result;
  }



  Stmt *getOriginal() const { return Original; }
  Stmt *getInheritedOriginal() const {
    if (Original)
      return Original;
    if (BasedOn)
      return BasedOn->getInheritedOriginal();
    return nullptr;
  }

  Derived *getBasedOn() const { return BasedOn; }

  void markParallel() { IsParallel = true; }
  bool isParallel() const { return IsParallel; }

  bool isRoot() const { return IsRoot; }

  ArrayRef<Derived*> getSuccessors()const { return Successors; }

  // TODO: Accept SmallArrayImpl to fill
  std::vector<Derived* > getLatestSuccessors()  {
    // If the loop is not being consumed, this is the latest successor.
    if (!isTransformationInput()) {
      std::vector<Derived* > Result;
      Result.push_back(&getDerived() );
      return Result;
      //return { &getDerived() };
    }

    std::vector<Derived* > Result;
    for (auto Succ : Successors) {
      auto SuccResult = Succ->getLatestSuccessors();
      Result.insert(Result.end(),   SuccResult.begin(),SuccResult.end());
    }
    return Result;
  }




  bool isOriginal() const { return Original; }

  bool isTransformationInput() const {
    bool Result = InputRole >= 0;
    assert(Result == (PrimaryInput != nullptr));
    return Result;
  }

  bool isTransformationFollowup() const {
    bool Result = FollowupRole >= 0;
    assert(Result == (BasedOn != nullptr));
    return Result;
  }

  bool isPrimaryInput() const {
    bool Result = (InputRole == 0);
    assert(Result == (PrimaryInput == this));
    return PrimaryInput == this;
  }

  int getFollowupRole() const {
    return FollowupRole;
  }

  ArrayRef<Derived*> getFollowups() const {
    return Followups;
  }

  void applyTransformation(Transform *Trans,
                           llvm::ArrayRef<Derived *> Followups,
                           ArrayRef< Derived *>Successors) {
    assert(!isTransformationInput());
    assert(llvm::find(Successors, nullptr) == Successors.end() );

    this->TransformedBy = Trans;
    this->Followups.insert(this->Followups.end(), Followups.begin(),
                           Followups.end());
    this->Successors .assign( Successors.begin(), Successors.end());
    this->PrimaryInput = &getDerived();
    this->InputRole = 0; // for primary

#ifndef NDEBUG
    assert(isTransformationInput() && isPrimaryInput());
    for (NodeTy *S : Followups) {
      assert(S->BasedOn == &getDerived());
    }
#endif
  }

  void applySuccessors(Derived *PrimaryInput, int InputRole,
                       llvm::ArrayRef<Derived *> Followups,
                       ArrayRef< Derived * > Successors) {
    assert(!isTransformationInput());
    assert(InputRole > 0);
    assert(llvm::find(Successors, nullptr) == Successors.end() );

    this->PrimaryInput = PrimaryInput;
    this->Followups.insert(this->Followups.end(), Followups.begin(),
                           Followups.end());
    this->Successors .assign( Successors.begin(), Successors.end());
    this->InputRole = InputRole;

#ifndef NDEBUG
    assert(isTransformationInput() && !isPrimaryInput());
    for (NodeTy *S : Followups) {
      assert(S->BasedOn == &getDerived());
    }
#endif
  }
};

/// Constructs a loop nest from source and applies transformations on it.
template <typename Derived, typename NodeTy> class TransformedTreeBuilder {
  using BuilderTy = Derived;

  Derived &getDerived() { return *static_cast<Derived *>(this); }
  const Derived &getDerived() const {
    return *static_cast<const Derived *>(this);
  }

  ASTContext &ASTCtx;
  llvm::SmallVectorImpl<NodeTy *> &AllNodes;
  llvm::SmallVectorImpl<Transform *> &AllTransforms;

private:
  /// Build the original loop nest hierarchy from the AST.
  void buildPhysicalLoopTree(Stmt *S, SmallVectorImpl<NodeTy *> &SubLoops,
                             llvm::DenseMap<Stmt *, NodeTy *> &StmtToTree) {
    if (!S)
      return;

    Stmt *Body;
    switch (S->getStmtClass()) {
    case Stmt::ForStmtClass:
      Body = cast<ForStmt>(S)->getBody();
      break;
    case Stmt::WhileStmtClass:
      Body = cast<WhileStmt>(S)->getBody();
      break;
    case Stmt::DoStmtClass:
      Body = cast<DoStmt>(S)->getBody();
      break;
    case Stmt::CXXForRangeStmtClass:
      Body = cast<CXXForRangeStmt>(S)->getBody();
      break;
    case Stmt::CapturedStmtClass:
      buildPhysicalLoopTree(cast<CapturedStmt>(S)->getCapturedStmt(), SubLoops,
                            StmtToTree);
      return;
    case Expr::LambdaExprClass:
      // Call to getBody materializes its body, children() (which is called in
      // the default case) does not.
      buildPhysicalLoopTree(cast<LambdaExpr>(S)->getBody(), SubLoops,
                            StmtToTree);
      return;
    case Expr::BlockExprClass:
      buildPhysicalLoopTree(cast<BlockExpr>(S)->getBody(), SubLoops,
                            StmtToTree);
      return;
    default:
      if (auto *O = dyn_cast<OMPExecutableDirective>(S)) {
        if (!O->hasAssociatedStmt())
          return;
        Stmt *Associated = O->getAssociatedStmt();
        buildPhysicalLoopTree(Associated, SubLoops, StmtToTree);
        return;
      }

      for (Stmt *Child : S->children())
        buildPhysicalLoopTree(Child, SubLoops, StmtToTree);

      return;
    }

    SmallVector<NodeTy *, 8> SubSubLoops;
    buildPhysicalLoopTree(Body, SubSubLoops, StmtToTree);

    NodeTy *L = getDerived().createPhysical(SubSubLoops, S);
    SubLoops.push_back(L);
    assert(StmtToTree.count(S) == 0);
    StmtToTree[S] = L;

    getDerived().applyOriginal(L);
  }

  /// Collect all loop transformations in the function's AST.
  class CollectTransformationsVisitor
      : public RecursiveASTVisitor<CollectTransformationsVisitor> {

    Derived &Builder;
    llvm::DenseMap<Stmt *, NodeTy *> &StmtToTree;

  public:
    CollectTransformationsVisitor(Derived &Builder,
                                  llvm::DenseMap<Stmt *, NodeTy *> &StmtToTree)
        : Builder(Builder), StmtToTree(StmtToTree) {}

    /// Transformations collected so far.
    llvm::SmallVector<NodeTransform, 16> Transforms;

    bool shouldTraversePostOrder() const { return true; }

    /// Read and apply LoopHint (#pragma clang loop) attributes.
    void applyAttributed(const Stmt *S, ArrayRef<const Attr *> Attrs,
                         NodeTy *L) {

      /// State of loop vectorization or unrolling.
      enum LVEnableState { Unspecified, Enable, Disable, Full };

      /// Value for llvm.loop.vectorize.enable metadata.
      LVEnableState VectorizeEnable = Unspecified;
      bool VectorizeAssumeSafety = false;

      /// Value for llvm.loop.unroll.* metadata (enable, disable, or full).
      LVEnableState UnrollEnable = Unspecified;

      /// Value for llvm.loop.unroll_and_jam.* metadata (enable, disable, or
      /// full).
      LVEnableState UnrollAndJamEnable = Unspecified;

      /// Value for llvm.loop.vectorize.predicate metadata
      LVEnableState VectorizePredicateEnable = Unspecified;

      /// Value for llvm.loop.vectorize.width metadata.
      unsigned VectorizeWidth = 0;

      /// Value for llvm.loop.interleave.count metadata.
      unsigned InterleaveCount = 0;

      /// llvm.unroll.
      unsigned UnrollCount = 0;

      /// llvm.unroll.
      unsigned UnrollAndJamCount = 0;

      /// Value for llvm.loop.distribute.enable metadata.
      LVEnableState DistributeEnable = Unspecified;

      /// Value for llvm.loop.pipeline.disable metadata.
      bool PipelineDisabled = false;

      /// Value for llvm.loop.pipeline.iicount metadata.
      unsigned PipelineInitiationInterval = 0;

      SourceLocation DistLoc;
      SourceLocation VecLoc;
      SourceLocation UnrollLoc;
      SourceLocation UnrollAndJamLoc;
      SourceLocation PipelineLoc;
      for (const Attr *Attr : Attrs) {
        const LoopHintAttr *LH = dyn_cast<LoopHintAttr>(Attr);
        const OpenCLUnrollHintAttr *OpenCLHint =
            dyn_cast<OpenCLUnrollHintAttr>(Attr);

        // Skip non loop hint attributes
        if (!LH && !OpenCLHint) {
          continue;
        }

        LoopHintAttr::OptionType Option = LoopHintAttr::Unroll;
        LoopHintAttr::LoopHintState State = LoopHintAttr::Disable;
        unsigned ValueInt = 1;
        // Translate opencl_unroll_hint attribute argument to
        // equivalent LoopHintAttr enums.
        // OpenCL v2.0 s6.11.5:
        // 0 - enable unroll (no argument).
        // 1 - disable unroll.
        // other positive integer n - unroll by n.
        if (OpenCLHint) {
          ValueInt = OpenCLHint->getUnrollHint();
          if (ValueInt == 0) {
            State = LoopHintAttr::Enable;
          } else if (ValueInt != 1) {
            Option = LoopHintAttr::UnrollCount;
            State = LoopHintAttr::Numeric;
          }
        } else if (LH) {
          Expr *ValueExpr = LH->getValue();
          if (ValueExpr) {
            if (ValueExpr->isValueDependent())
              continue; // Ignore this attribute until it is instantiated.
            llvm::APSInt ValueAPS =
                ValueExpr->EvaluateKnownConstInt(Builder.ASTCtx);
            ValueInt = ValueAPS.getSExtValue();
          }

          Option = LH->getOption();
          State = LH->getState();
        }

        switch (Option) {
        case LoopHintAttr::Vectorize:
        case LoopHintAttr::Interleave:
        case LoopHintAttr::VectorizeWidth:
        case LoopHintAttr::InterleaveCount:
        case LoopHintAttr::VectorizePredicate:
          VecLoc = Attr->getLocation();
          break;
        case LoopHintAttr::Unroll:
        case LoopHintAttr::UnrollCount:
          UnrollLoc = Attr->getLocation();
          break;
        case LoopHintAttr::UnrollAndJam:
        case LoopHintAttr::UnrollAndJamCount:
          UnrollAndJamLoc = Attr->getLocation();
          break;
        case LoopHintAttr::Distribute:
          DistLoc = Attr->getLocation();
          break;

        case LoopHintAttr::PipelineDisabled:
        case LoopHintAttr::PipelineInitiationInterval:
          PipelineLoc = Attr->getLocation();
          break;
        }

        switch (State) {
        case LoopHintAttr::Disable:
          switch (Option) {
          case LoopHintAttr::Vectorize:
            // Disable vectorization by specifying a width of 1.
            VectorizeWidth = 1;
            break;
          case LoopHintAttr::Interleave:
            // Disable interleaving by specifying a count of 1.
            InterleaveCount = 1;
            break;
          case LoopHintAttr::Unroll:
            UnrollEnable = Disable;
            break;
          case LoopHintAttr::UnrollAndJam:
            UnrollAndJamEnable = Disable;
            break;
          case LoopHintAttr::VectorizePredicate:
            VectorizePredicateEnable = Disable;
            break;
          case LoopHintAttr::Distribute:
            DistributeEnable = Disable;
            break;
          case LoopHintAttr::PipelineDisabled:
            PipelineDisabled = true;
            break;
          case LoopHintAttr::UnrollCount:
          case LoopHintAttr::UnrollAndJamCount:
          case LoopHintAttr::VectorizeWidth:
          case LoopHintAttr::InterleaveCount:
          case LoopHintAttr::PipelineInitiationInterval:
            llvm_unreachable("Options cannot be disabled.");
            break;
          }
          break;
        case LoopHintAttr::Enable:
          switch (Option) {
          case LoopHintAttr::Vectorize:
          case LoopHintAttr::Interleave:
            VectorizeEnable = Enable;
            break;
          case LoopHintAttr::Unroll:
            UnrollEnable = Enable;
            break;
          case LoopHintAttr::UnrollAndJam:
            UnrollAndJamEnable = Enable;
            break;
          case LoopHintAttr::VectorizePredicate:
            VectorizePredicateEnable = Enable;
            break;
          case LoopHintAttr::Distribute:
            DistributeEnable = Enable;
            break;
          case LoopHintAttr::UnrollCount:
          case LoopHintAttr::UnrollAndJamCount:
          case LoopHintAttr::VectorizeWidth:
          case LoopHintAttr::InterleaveCount:
          case LoopHintAttr::PipelineDisabled:
          case LoopHintAttr::PipelineInitiationInterval:
            llvm_unreachable("Options cannot enabled.");
            break;
          }
          break;
        case LoopHintAttr::AssumeSafety:
          switch (Option) {
          case LoopHintAttr::Vectorize:
          case LoopHintAttr::Interleave:
            // Apply "llvm.mem.parallel_loop_access" metadata to load/stores.
            VectorizeEnable = Enable;
            VectorizeAssumeSafety = true;
            break;
          case LoopHintAttr::Unroll:
          case LoopHintAttr::UnrollAndJam:
          case LoopHintAttr::VectorizePredicate:
          case LoopHintAttr::UnrollCount:
          case LoopHintAttr::UnrollAndJamCount:
          case LoopHintAttr::VectorizeWidth:
          case LoopHintAttr::InterleaveCount:
          case LoopHintAttr::Distribute:
          case LoopHintAttr::PipelineDisabled:
          case LoopHintAttr::PipelineInitiationInterval:
            llvm_unreachable("Options cannot be used to assume mem safety.");
            break;
          }
          break;
        case LoopHintAttr::Full:
          switch (Option) {
          case LoopHintAttr::Unroll:
            UnrollEnable = Full;
            break;
          case LoopHintAttr::UnrollAndJam:
            UnrollAndJamEnable = Full;
            break;
          case LoopHintAttr::Vectorize:
          case LoopHintAttr::Interleave:
          case LoopHintAttr::UnrollCount:
          case LoopHintAttr::UnrollAndJamCount:
          case LoopHintAttr::VectorizeWidth:
          case LoopHintAttr::InterleaveCount:
          case LoopHintAttr::Distribute:
          case LoopHintAttr::PipelineDisabled:
          case LoopHintAttr::PipelineInitiationInterval:
          case LoopHintAttr::VectorizePredicate:
            llvm_unreachable("Options cannot be used with 'full' hint.");
            break;
          }
          break;
        case LoopHintAttr::Numeric:
          switch (Option) {
          case LoopHintAttr::VectorizeWidth:
            VectorizeWidth = ValueInt;
            break;
          case LoopHintAttr::InterleaveCount:
            InterleaveCount = ValueInt;
            break;
          case LoopHintAttr::UnrollCount:
            UnrollCount = ValueInt;
            break;
          case LoopHintAttr::UnrollAndJamCount:
            UnrollAndJamCount = ValueInt;
            break;
          case LoopHintAttr::PipelineInitiationInterval:
            PipelineInitiationInterval = ValueInt;
            break;
          case LoopHintAttr::Unroll:
          case LoopHintAttr::UnrollAndJam:
          case LoopHintAttr::VectorizePredicate:
          case LoopHintAttr::Vectorize:
          case LoopHintAttr::Interleave:
          case LoopHintAttr::Distribute:
          case LoopHintAttr::PipelineDisabled:
            llvm_unreachable("Options cannot be assigned a value.");
            break;
          }
          break;
        }
      }

      bool VectorizeDisabled =
          (VectorizeEnable == Disable || VectorizeWidth == 1);
      bool InterleaveDisabled = (InterleaveCount == 1);
      bool VectorizeInterleaveDisabled =
          VectorizeDisabled && InterleaveDisabled && !VectorizeAssumeSafety;

      if (UnrollEnable == Disable) {
        L->HasLegacyDisable = true;
        Builder.disableUnroll(L);
      }

      if (UnrollAndJamEnable == Disable) {
        L->HasLegacyDisable = true;
        Builder.disableUnrollAndJam(L);
      }

      if (DistributeEnable == Disable) {
        L->HasLegacyDisable = true;
        Builder.disableDistribution(L);
      }

      // If the LoopVectorize pass is completely disabled (ie. vectorize and
      // interleave).
      if (VectorizeInterleaveDisabled) {
        L->HasLegacyDisable = true;
        Builder.disableVectorizeInterleave(L);
      }

      if (PipelineDisabled) {
        L->HasLegacyDisable = true;
        Builder.disablePipelining(L);
      }

      if (UnrollEnable == Full) {
        Transform *Trans =
            LoopUnrollingTransform::createFull(UnrollLoc, true, false, true);
        Transforms.emplace_back(Trans, TransformInput::createByStmt(S));
      }

      if (DistributeEnable == Enable) {
        auto *Trans = LoopDistributionTransform::create(DistLoc, true);
        Transforms.emplace_back(Trans, TransformInput::createByStmt(S));
      }

      if ((VectorizeEnable == Enable || VectorizeWidth > 0 ||
           VectorizePredicateEnable != Unspecified || InterleaveCount > 0) &&
          !VectorizeInterleaveDisabled) {
        auto *Trans = LoopVectorizationInterleavingTransform::create(
            VecLoc, true, VectorizeAssumeSafety,
            VectorizeEnable != Unspecified
                ? llvm::Optional<bool>(VectorizeEnable == Enable)
                : None,
            None, VectorizeWidth,
            (VectorizePredicateEnable != Unspecified)
                ? llvm::Optional<bool>(VectorizePredicateEnable == Enable)
                : None,
            InterleaveCount);
        Transforms.emplace_back(Trans, TransformInput::createByStmt(S));
      }

      if (UnrollAndJamEnable == Enable || UnrollAndJamEnable == Full ||
          UnrollAndJamCount > 0) {
        LoopUnrollAndJamTransform *Trans;
        if (UnrollAndJamEnable == Full)
          Trans = LoopUnrollAndJamTransform::createFull(UnrollAndJamLoc, true,
                                                        true);
        else
          Trans = LoopUnrollAndJamTransform::createPartial(
              UnrollAndJamLoc, true, UnrollAndJamEnable == Enable,
              UnrollAndJamCount);
        Transforms.emplace_back(Trans, TransformInput::createByStmt(S));
      }

      if (UnrollEnable != Full && (UnrollEnable == Enable || UnrollCount > 0)) {
        auto *Trans = LoopUnrollingTransform::createPartial(
            UnrollLoc, true, false, UnrollEnable == Enable, UnrollCount);
        Transforms.emplace_back(Trans, TransformInput::createByStmt(S));
      }

      if (PipelineInitiationInterval > 0) {
        auto *Trans = LoopPipeliningTransform::create(
            PipelineLoc, PipelineInitiationInterval);
        Transforms.emplace_back(Trans, TransformInput::createByStmt(S));
      }
    }

    bool VisitAttributedStmt(const AttributedStmt *S) {
      const Stmt *LoopStmt = getAssociatedLoop(S);

      // Not every attributed statement is associated with a loop.
      if (!LoopStmt)
        return true;

      NodeTy *Node = StmtToTree.lookup(LoopStmt);
      assert(Node && "We should have created a node for ever loop");
      applyAttributed(LoopStmt, S->getAttrs(), Node);
      return true;
    }

    bool
    VisitTransformExecutableDirective(const TransformExecutableDirective *S) {
      // This ExtractTransform does not emit any diagnostics. Diagnostics should
      // have been emitted in Sema::ActOnTransformExecutableDirective.
      DefaultExtractTransform ExtractTransform(Builder.ASTCtx, S);
      std::unique_ptr<Transform> Trans = ExtractTransform.createTransform();

      // We might not get a transform in non-instantiated templates or
      // inconsistent clauses.
      if (Trans) {
        const Stmt *TheLoop = getAssociatedLoop(S->getAssociated());
        Transforms.emplace_back(Trans.get(),
                                TransformInput::createByStmt(TheLoop));
        Builder.AllTransforms.push_back(Trans.release());
      }

      return true;
    }

    bool handleOMPLoopClauses(OMPLoopDirective *Directive, bool HasTaskloop,
                              bool HasFor, bool HasSimd) {
      assert((!HasTaskloop || !HasFor) &&
             "taskloop and for are mutually exclusive");
      const Stmt *TopLevel = getAssociatedLoop(Directive);

    auto IfClauses =  Directive->getClausesOfKind < OMPIfClause> ();
    if (!llvm::empty(IfClauses)) {
      // OpenMP specifies loop versioning with the if clause: A "then" loop where the directive is applied and a "else" loop where it is not. Currently, it is only supported in the "simd" construct where vectorization is disabled on the else loop.
      assert(HasSimd);

     // assert(llvm::size(IfClauses) == 1);
      //assert(std::distance(IfClauses.begin(), IfClauses.end()) == 1);
      auto IfClause = *IfClauses.begin();
      auto LocRange = SourceRange( IfClause->getBeginLoc(), IfClause->getEndLoc());

      auto VersioningTrans= OMPIfClauseVersioningTransform::create(LocRange);
      Builder.AllTransforms.push_back(VersioningTrans);
      Transforms.emplace_back(VersioningTrans,TransformInput::createByStmt(TopLevel ) );

      auto DisableSimd = OMPDisableSimdTransform::create(LocRange);
      Builder.AllTransforms.push_back(DisableSimd);
      Transforms.emplace_back(  DisableSimd, TransformInput::createByFollowup(VersioningTrans, OMPIfClauseVersioningTransform:: FollowupElse) );
    }



      bool IsMonotonic = true;
      if (HasFor) {
        if (auto *ScheduleClause =
                Directive->getSingleClause<OMPScheduleClause>()) {
          OpenMPScheduleClauseKind Schedule = ScheduleClause->getScheduleKind();
          if (Schedule != OMPC_SCHEDULE_unknown &&
              Schedule != OMPC_SCHEDULE_static)
            IsMonotonic = false;

          if (ScheduleClause->getFirstScheduleModifier() ==
              OMPC_SCHEDULE_MODIFIER_monotonic)
            IsMonotonic = true;
          if (ScheduleClause->getSecondScheduleModifier() ==
              OMPC_SCHEDULE_MODIFIER_monotonic)
            IsMonotonic = true;
        }

        if (auto *OrderedClause =
                Directive->getSingleClause<OMPOrderedClause>()) {
          if (!OrderedClause->getNumForLoops())
            IsMonotonic = true;
        }
      }

      if (HasTaskloop)
        IsMonotonic = false;

      int64_t SimdWidth = 0;
      bool SimdImplicitParallel = false;
      if (HasSimd) {
        if (!HasFor)
          SimdImplicitParallel = true;

        if (auto *LenClause = Directive->getSingleClause<OMPSafelenClause>()) {
          auto SafelenExpr = LenClause->getSafelen();
          if (!SafelenExpr->isValueDependent() &&
              SafelenExpr->isEvaluatable(Builder.ASTCtx)) {
            llvm::APSInt ValueAPS =
                SafelenExpr->EvaluateKnownConstInt(Builder.ASTCtx);
            SimdWidth = ValueAPS.getSExtValue();
          }

          // In presence of finite 'safelen', it may be unsafe to mark all
          // the memory instructions parallel, because loop-carried
          // dependences of 'safelen' iterations are possible.
          SimdImplicitParallel = false;
          IsMonotonic = true;
        }

        if (auto LenClause = Directive->getSingleClause<OMPSimdlenClause>()) {
          auto SimdlenExpr = LenClause->getSimdlen();
          if (!SimdlenExpr->isValueDependent() &&
              SimdlenExpr->isEvaluatable(Builder.ASTCtx)) {
            llvm::APSInt ValueAPS =
                SimdlenExpr->EvaluateKnownConstInt(Builder.ASTCtx);
            SimdWidth = ValueAPS.getSExtValue();
          }
        }
      }

      bool IsImplicitParallel = !IsMonotonic || SimdImplicitParallel;

      if (HasSimd) {
        auto *Trans = LoopVectorizationInterleavingTransform::create(
            Directive->getSourceRange(), true, IsImplicitParallel, true, None,
            SimdWidth, None, 0);
        Transforms.emplace_back(Trans, TransformInput::createByStmt(TopLevel));
      } else if (IsImplicitParallel) {
        auto *Assume =
            LoopAssumeParallelTransform::create(Directive->getSourceRange());
        Transforms.emplace_back(Assume, TransformInput::createByStmt(TopLevel));
      }

      return true;
    }

    bool VisitOMPForDirective(OMPForDirective *For) {
      return handleOMPLoopClauses(For, false, true, false);
    }

    bool VisitOMPDistributeParallelForDirective(
        OMPDistributeParallelForDirective *L) {
      return handleOMPLoopClauses(L, false, true, false);
    }

    bool VisitOMPTeamsDistributeParallelForDirective(
        OMPTeamsDistributeParallelForDirective *L) {
      return handleOMPLoopClauses(L, false, true, false);
    }

    bool VisitOMPTargetTeamsDistributeParallelForDirective(
        OMPTargetTeamsDistributeParallelForDirective *L) {
      return handleOMPLoopClauses(L, false, true, false);
    }

    bool VisitOMPSimdDirective(OMPLoopDirective *Simd) {
      return handleOMPLoopClauses(Simd, false, false, true);
    }

    bool VisitOMPForSimdDirective(OMPForSimdDirective *ForSimd) {
      return handleOMPLoopClauses(ForSimd, false, true, true);
    }

    bool
    VisitOMPParallelForSimdDirective(OMPParallelForSimdDirective *ForSimd) {
      return handleOMPLoopClauses(ForSimd, false, true, true);
    }

    bool VisitOMPDistributeSimdDirective(
        OMPDistributeSimdDirective *DistributeSimd) {
      return handleOMPLoopClauses(DistributeSimd, false, false, true);
    }

    bool VisitOMPDistributeParallelForSimdDirective(
        OMPDistributeParallelForSimdDirective *DistributeSimd) {
      return handleOMPLoopClauses(DistributeSimd, false, true, true);
    }

    bool VisitOMPTeamsDistributeSimdDirective(
        OMPTeamsDistributeSimdDirective *TeamsDistributeSimd) {
      return handleOMPLoopClauses(TeamsDistributeSimd, false, false, true);
    }

    bool VisitOMPTeamsDistributeParallelForSimdDirective(
        OMPTeamsDistributeParallelForSimdDirective
            *TeamsDistributeParallelForSimd) {
      return handleOMPLoopClauses(TeamsDistributeParallelForSimd, false, true,
                                  true);
    }

    bool VisitOMPTargetTeamsDistributeSimdDirective(
        OMPTargetTeamsDistributeSimdDirective *TargetTeamsDistributeSimd) {
      return handleOMPLoopClauses(TargetTeamsDistributeSimd, false, false,
                                  true);
    }

    bool VisitOMPTargetTeamsDistributeParallelForSimdDirective(
        OMPTargetTeamsDistributeParallelForSimdDirective
            *TargetTeamsDistributeParallelForSimd) {
      return handleOMPLoopClauses(TargetTeamsDistributeParallelForSimd, false,
                                  true, true);
    }

    bool VisitOMPTaskLoopSimdDirective(
        OMPTaskLoopSimdDirective *TaskLoopSimdDirective) {
      return handleOMPLoopClauses(TaskLoopSimdDirective, true, false, true);
    }

    bool
    VisitOMPTargetSimdDirective(OMPTargetSimdDirective *TargetSimdDirective) {
      return handleOMPLoopClauses(TargetSimdDirective, false, false, true);
    }

    bool VisitOMPTargetParallelForSimdDirective(
        OMPTargetParallelForSimdDirective *TargetParallelForSimdDirective) {
      return handleOMPLoopClauses(TargetParallelForSimdDirective, false, true,
                                  true);
    }

    bool VisitOMPMasterTaskLoopDirective(
        OMPMasterTaskLoopDirective *MasterTaskloop) {
      return handleOMPLoopClauses(MasterTaskloop, true, false, false);
    }

    bool VisitOMPMasterTaskLoopSimdDirective(
        OMPMasterTaskLoopSimdDirective *MasterTaskloopSimd) {
      return handleOMPLoopClauses(MasterTaskloopSimd, true, false, true);
    }

    bool VisitOMPParallelMasterTaskLoopDirective(
        OMPParallelMasterTaskLoopDirective *ParallelMasterTaskloop) {
      return handleOMPLoopClauses(ParallelMasterTaskloop, true, false, false);
    }

    bool VisitOMPParallelMasterTaskLoopSimdDirective(
        OMPParallelMasterTaskLoopSimdDirective *ParallelMasterTaskloopSimd) {
      return handleOMPLoopClauses(ParallelMasterTaskloopSimd, true, false,
                                  true);
    }
  };

  /// Applies collected transformations to the loop nest representation.
  struct TransformApplicator {
    Derived &Builder;
    llvm::DenseMap<const Stmt *, llvm::SmallVector<NodeTransform *, 2>>        TransByStmt;
    llvm::DenseMap<Transform*, llvm::SmallVector<NodeTransform*, 2> > TransformByFollowup;

    TransformApplicator(Derived &Builder) : Builder(Builder) {}

    void addNodeTransform(NodeTransform *NT) {
      TransformInput &TopLevelInput = NT->Inputs[0];
      if (TopLevelInput.isByStmt()) {
        TransByStmt[TopLevelInput.getStmtInput()].push_back(NT);
      }      else if (TopLevelInput.isByFollowup()) {
        TransformByFollowup[TopLevelInput.getPrecTrans()].push_back(NT);
      }
      else
        llvm_unreachable("Transformation must apply to something");
    }

    void checkStageOrder(ArrayRef<NodeTy *> PrevLoops, Transform *NewTrans) {
      for (NodeTy *PrevLoop : PrevLoops) {
        // Cannot combine legacy disable pragmas (e.g. #pragma clang loop
        // unroll(disable)) and new transformations (#pragma clang transform).
        if (!NewTrans->isLegacy() && PrevLoop->HasLegacyDisable) {
          Builder.Diag(NewTrans->getBeginLoc(),
                       diag::err_sema_transform_legacy_mix);
          return;
        }

        Transform *PrevSourceTrans = PrevLoop->getSourceTransformation();
        if (!PrevSourceTrans)
          continue;

        // Cannot combine legacy constructs (#pragma clang loop, ...) with new
        // ones (#pragma clang transform).
        if (NewTrans->isLegacy() != PrevSourceTrans->isLegacy()) {
          Builder.Diag(NewTrans->getBeginLoc(),
                       diag::err_sema_transform_legacy_mix);
          return;
        }

        int PrevStage = PrevSourceTrans->getLoopPipelineStage();
        int NewStage = NewTrans->getLoopPipelineStage();
        if (PrevStage >= 0 && NewStage >= 0 && PrevStage > NewStage) {
          Builder.Diag(NewTrans->getBeginLoc(),
                       diag::warn_sema_transform_pass_order);

          // At most one warning per transformation.
          return;
        }
      }
    }

    NodeTy *applyTransform(Transform *Trans, NodeTy *MainLoop) {
      switch (Trans->getKind()) {
      case Transform::Kind::LoopUnrollingKind:
        return applyUnrolling(cast<LoopUnrollingTransform>(Trans), MainLoop);
      case Transform::Kind::LoopUnrollAndJamKind:
        return applyUnrollAndJam(cast<LoopUnrollAndJamTransform>(Trans),
                                 MainLoop);
      case Transform::Kind::LoopDistributionKind:
        return applyDistribution(cast<LoopDistributionTransform>(Trans),
                                 MainLoop);
      case Transform::Kind::LoopVectorizationInterleavingKind:
        return applyVectorizeInterleave(
            cast<LoopVectorizationInterleavingTransform>(Trans), MainLoop);
      case Transform::Kind::LoopVectorizationKind:
        return applyVectorize(cast<LoopVectorizationTransform>(Trans),
                              MainLoop);
      case Transform::Kind::LoopInterleavingKind:
        return applyInterleave(cast<LoopInterleavingTransform>(Trans),
                               MainLoop);
      case Transform::Kind::LoopPipeliningKind:
        return applyPipelining(cast<LoopPipeliningTransform>(Trans), MainLoop);
      case Transform::Kind::LoopAssumeParallelKind:
        return applyAssumeParallel(cast<LoopAssumeParallelTransform>(Trans),
                                   MainLoop);
      case Transform::Kind::OMPIfClauseVersioningKind:
        return applyOMPIfClauseVersioning(cast<OMPIfClauseVersioningTransform>(Trans), MainLoop  );
      case Transform::Kind::OMPDisableSimdKind:
        return applyOMPDisableSimd(cast<OMPDisableSimdTransform>(Trans), MainLoop  );
      default:
        llvm_unreachable("unimplemented transformation");
      }
    }

    void inheritLoopAttributes(NodeTy *Dst, NodeTy *Src, bool IsAll,
                               bool IsSuccessor) {
      Builder.inheritLoopAttributes(Dst, Src, IsAll, IsSuccessor);
    }

    NodeTy *applyUnrolling(LoopUnrollingTransform *Trans, NodeTy *MainLoop) {
      checkStageOrder({MainLoop}, Trans);

      NodeTy *Successor = nullptr;
      if (Trans->isFull()) {
        // Full unrolling has no followup-loop.
        MainLoop->applyTransformation(Trans, {}, nullptr);
      } else {
        NodeTy *All = Builder.createFollowup(
            MainLoop->Subloops, MainLoop, LoopUnrollingTransform::FollowupAll);
        NodeTy *Unrolled =
            Builder.createFollowup(MainLoop->Subloops, MainLoop,
                                   LoopUnrollingTransform::FollowupUnrolled);
        NodeTy *Remainder =
            Builder.createFollowup(MainLoop->Subloops, MainLoop,
                                   LoopUnrollingTransform::FollowupRemainder);
        Successor = Trans->isLegacy() ? All : Unrolled;

        inheritLoopAttributes(All, MainLoop, true, All == Successor);
        MainLoop->applyTransformation(Trans, {All, Unrolled, Remainder},
                                      Successor);
      }

      Builder.applyUnroll(Trans, MainLoop);
      return Successor;
    }

    NodeTy *applyUnrollAndJam(LoopUnrollAndJamTransform *Trans,
                              NodeTy *MainLoop) {
      // Search for the innermost loop that is being jammed.
      NodeTy *Cur = MainLoop;
      NodeTy *Inner = nullptr;
      auto LatestInner = Cur->getLatestSubLoops();
      if (LatestInner.size() == 1) {
        Inner = LatestInner[0];
      } else if (!Trans->isLegacy()) {
        Builder.Diag(Trans->getBeginLoc(),
                     diag::err_sema_transform_unrollandjam_expect_nested_loop);
        return nullptr;
      }

      if (!Trans->isLegacy() && !Inner) {
        if (!Trans->isLegacy())
          Builder.Diag(
              Trans->getBeginLoc(),
              diag::err_sema_transform_unrollandjam_expect_nested_loop);
        return nullptr;
      }

      if (!Trans->isLegacy() && Inner->Subloops.size() != 0) {
        if (!Trans->isLegacy())
          Builder.Diag(Trans->getBeginLoc(),
                       diag::err_sema_transform_unrollandjam_not_innermost);
        return nullptr;
      }

      // Having no loop to jam does not make a lot of sense, but fixes
      // regression tests.
      if (!Inner) {
        checkStageOrder({MainLoop}, Trans);

        NodeTy *UnrolledOuter = Builder.createFollowup(
            {}, MainLoop, LoopUnrollAndJamTransform::FollowupOuter);
        inheritLoopAttributes(UnrolledOuter, MainLoop, true, false);

        MainLoop->applyTransformation(Trans, {UnrolledOuter}, UnrolledOuter);
        Builder.applyUnrollAndJam(Trans, MainLoop, nullptr);
        return UnrolledOuter;
      }

      checkStageOrder({MainLoop, Inner}, Trans);

      NodeTy *TransformedInner = Builder.createFollowup(
          Inner->Subloops, Inner, LoopUnrollAndJamTransform::FollowupInner);
      inheritLoopAttributes(TransformedInner, Inner, false, false);

      // TODO: Handle full unrolling
      NodeTy *UnrolledOuter = Builder.createFollowup(
          {Inner}, MainLoop, LoopUnrollAndJamTransform::FollowupOuter);
      inheritLoopAttributes(UnrolledOuter, MainLoop, false, true);

      MainLoop->applyTransformation(Trans, {UnrolledOuter}, UnrolledOuter);
      Inner->applySuccessors(MainLoop, LoopUnrollAndJamTransform::InputInner,
                             {TransformedInner}, TransformedInner);

      Builder.applyUnrollAndJam(Trans, MainLoop, Inner);
      return UnrolledOuter;
    }

    NodeTy *applyDistribution(LoopDistributionTransform *Trans,
                              NodeTy *MainLoop) {
      checkStageOrder({MainLoop}, Trans);

      NodeTy *All = Builder.createFollowup(
          MainLoop->Subloops, MainLoop, LoopDistributionTransform::FollowupAll);
      NodeTy *Successor = Trans->isLegacy() ? All : nullptr;
      inheritLoopAttributes(All, MainLoop, true, Successor == All);

      MainLoop->applyTransformation(Trans, {All}, Successor);
      Builder.applyDistribution(Trans, MainLoop);
      return Successor;
    }

    NodeTy *
    applyVectorizeInterleave(LoopVectorizationInterleavingTransform *Trans,
                             NodeTy *MainLoop) {
      checkStageOrder({MainLoop}, Trans);

      NodeTy *All = Builder.createFollowup(
          MainLoop->Subloops, MainLoop,
          LoopVectorizationInterleavingTransform::FollowupAll);
      NodeTy *Vectorized = Builder.createFollowup(
          MainLoop->Subloops, MainLoop,
          LoopVectorizationInterleavingTransform::FollowupVectorized);
      NodeTy *Epilogue = Builder.createFollowup(
          MainLoop->Subloops, MainLoop,
          LoopVectorizationInterleavingTransform::FollowupEpilogue);

      NodeTy *Successor = Trans->isLegacy() ? All : Vectorized;
      if (Trans->isAssumeSafety() && !MainLoop->IsParallel) {
        MainLoop->IsParallel = true;
        Builder.markParallel(MainLoop);
      }

      inheritLoopAttributes(All, MainLoop, true, All == Successor);
      MainLoop->applyTransformation(Trans, {All, Vectorized, Epilogue},
                                    Successor);
      Builder.applyVectorizeInterleave(Trans, MainLoop);
      return Successor;
    }

    NodeTy *applyVectorize(LoopVectorizationTransform *Trans,
                           NodeTy *MainLoop) {
      auto *VecInterleaveTrans = LoopVectorizationInterleavingTransform::create(
          Trans->getRange(), false, false, true, false, Trans->getWidth(),
          Trans->isPredicateEnabled(), 1);
      return applyVectorizeInterleave(VecInterleaveTrans, MainLoop);
    }

    NodeTy *applyInterleave(LoopInterleavingTransform *Trans,
                            NodeTy *MainLoop) {
      auto *VecInterleaveTrans = LoopVectorizationInterleavingTransform::create(
          Trans->getRange(), false, false, false, true, 1, None,
          Trans->getInterleaveCount());
      return applyVectorizeInterleave(VecInterleaveTrans, MainLoop);
    }

    NodeTy *applyPipelining(LoopPipeliningTransform *Trans, NodeTy *MainLoop) {
      checkStageOrder({MainLoop}, Trans);
      MainLoop->applyTransformation(Trans, {}, nullptr);
      Builder.applyPipelining(Trans, MainLoop);
      return nullptr;
    }

    NodeTy *applyAssumeParallel(LoopAssumeParallelTransform *Assumption,
                                NodeTy *MainLoop) {
      if (MainLoop->isParallel())
        return MainLoop;

      MainLoop->IsParallel = true;
      Builder.markParallel(MainLoop);

      return MainLoop;
    }


   NodeTy *applyOMPIfClauseVersioning(OMPIfClauseVersioningTransform *Trans,        NodeTy *MainLoop) {
     NodeTy *Then = Builder.createFollowup(  MainLoop->Subloops, MainLoop,       OMPIfClauseVersioningTransform::FollowupIf);
     inheritLoopAttributes(Then, MainLoop, true, true);

     NodeTy *Else = Builder.createFollowup(  MainLoop->Subloops, MainLoop,       OMPIfClauseVersioningTransform::FollowupElse);
     inheritLoopAttributes(Else, MainLoop, true, false);

     MainLoop->applyTransformation(Trans, {Then, Else},  {Then, Else});
     Builder.applyOMPIfClauseVersioning(Trans, MainLoop);

      return Then;
    }

   NodeTy *applyOMPDisableSimd(OMPDisableSimdTransform *Trans,        NodeTy *MainLoop) {
     MainLoop->applyTransformation(Trans, {}, {});
     Builder.disableOMDSimd(MainLoop);
 return nullptr;
}

    void traverseSubloops(NodeTy *L) {
      // TODO: Instead of recursively traversing the entire subtree, in case we are re-traversing after a transformation, only traverse followups of that transformation.
      for (NodeTy *SubL : L->getSubLoops()) {
        auto Latest =  SubL->getLatestSuccessors();
        for (auto SubL: Latest)
          traverse(SubL);
      }

#if 0
      // Transform subloops first.
      // TODO: Instead of recursively traversing the entire subtree, in case we are re-traversing after a transformation, only traverse followups of that transformation.
      for (NodeTy *SubL : L->getSubLoops()) {
        SubL = SubL->getLatestSuccessor();
        if (!SubL)
          continue;
        traverse(SubL);
      }
#endif
    }

    bool applyTransform(NodeTy *L) {
      if (L->isRoot())
        return false;
     // assert(L == L->getLatestSuccessor() && "Loop must not have been consumed by another transformation");

      // Look for transformations that apply syntactically to this loop.
      Stmt *OrigStmt = L->getInheritedOriginal();
      auto TransformsOnStmt = TransByStmt.find(OrigStmt);
      if (TransformsOnStmt != TransByStmt.end()) {
        auto &List = TransformsOnStmt->second;
        if (!List.empty()) {
          NodeTransform *Trans = List.front();
          applyTransform(Trans->Trans, L);
          List.erase(List.begin());



          return true;
        }
      }


      // Look for transformations that are chained to one of the followups.
     auto SourceTrans = L-> getSourceTransformation();
     if (!SourceTrans)
       return false;
     auto Chained = TransformByFollowup.find(SourceTrans);
     if (Chained == TransformByFollowup.end())
       return false;
     auto LIdx = L->getFollowupRole();
     auto &List = Chained->second;
       for (auto It = List.begin(), E = List.end(); It != E; ++It){
         NodeTransform* Item = *It;
       auto FollowupIdx = Item->Inputs[0].getFollowupIdx();
       if (LIdx != FollowupIdx)
         continue;


       applyTransform(Item->Trans, L);
       List. erase(It);
       return true;
     }

#if 0
      if (auto LatestTransform = L->getTransformedBy()) {
        auto Chained = TransformByFollowup.find(LatestTransform);
        if (Chained != TransformByFollowup.end()) {
          auto &List = Chained->second;
          NodeTransform *NT = List  .front();
          auto MainInput = NT->Inputs[0];
          assert(MainInput.isByFollowup());
          assert(MainInput.getPrecTrans()==LatestTransform);
          auto FollowupNode = L->Followups[MainInput.getFollowupIdx()];
            assert(FollowupNode);
          applyTransform(NT->Trans, FollowupNode);
          List. erase(List.begin());
          return true;
        }
      }
#endif

      return false;
    }



    void traverse(NodeTy* N) {
      auto Latest = N->getLatestSuccessors();
      for (auto L : Latest) {
        traverseSubloops(L);
        if (applyTransform(L)) {
          // Apply transformations on nested followups.
          traverse(L);
        }
      }
    }

#if 0
      do {
        L = L->getLatestSuccessor();
        if (!L)
          break;
        traverseSubloops(L);
      } while (applyTransform(L));
    }
#endif
  };

protected:
  TransformedTreeBuilder(ASTContext &ASTCtx,
                         llvm::SmallVectorImpl<NodeTy *> &AllNodes,
                         llvm::SmallVectorImpl<Transform *> &AllTransforms)
      : ASTCtx(ASTCtx), AllNodes(AllNodes), AllTransforms(AllTransforms) {}

  NodeTy *createRoot(llvm::ArrayRef<NodeTy *> SubLoops) {
    auto *Result = new NodeTy(SubLoops, nullptr, nullptr, -1);
    AllNodes.push_back(Result);
    Result->IsRoot = true;
    return Result;
  }

  NodeTy *createPhysical(llvm::ArrayRef<NodeTy *> SubLoops,
                         clang::Stmt *Original) {
    assert(Original);
    auto *Result = new NodeTy(SubLoops, nullptr, Original, -1);
    AllNodes.push_back(Result);
    return Result;
  }

  NodeTy *createFollowup(llvm::ArrayRef<NodeTy *> SubLoops, NodeTy *BasedOn,
                         int FollowupRole) {
    assert(BasedOn);
    auto *Result = new NodeTy(SubLoops, BasedOn, nullptr, FollowupRole);
    AllNodes.push_back(Result);
    return Result;
  }

public:
  void markParallel(NodeTy *L) { L->markParallel(); }

  NodeTy *
  computeTransformedStructure(Stmt *Body,
                              llvm::DenseMap<Stmt *, NodeTy *> &StmtToTree) {
    if (!Body)
      return nullptr;

    // Create original tree.
    SmallVector<NodeTy *, 8> TopLevelLoops;
    buildPhysicalLoopTree(Body, TopLevelLoops, StmtToTree);
    NodeTy *Root = getDerived().createRoot(TopLevelLoops);

    // Collect all loop transformations.
    CollectTransformationsVisitor Collector(getDerived(), StmtToTree);
    Collector.TraverseStmt(Body);
    auto &TransformList = Collector.Transforms;

    // Local function to apply every transformation that the predicate returns
    // true to. This is to emulate LLVM's pass pipeline that would apply
    // transformation in this order.
    auto SelectiveApplicator = [this, &TransformList, Root](auto Pred) {
      TransformApplicator OMPApplicator(getDerived());
      bool AnyActive = false;
      for (NodeTransform &NT : TransformList) {
        if (Pred(NT)) {
          OMPApplicator.addNodeTransform(&NT);
          AnyActive = true;
        }
      }

      // No traversal needed if no transformations to apply.
      if (!AnyActive)
        return;

      OMPApplicator.traverse(Root);

      // Report leftover transformations.
      for (auto &P : OMPApplicator.TransByStmt) {
        for (NodeTransform *NT : P.second) {
          if (!NT->Trans->isLegacy())
            getDerived().Diag(NT->Trans->getBeginLoc(),
                              diag::err_sema_transform_missing_loop);
        }
      }

      // Remove applied transformations from list.
      auto NewEnd =
          std::remove_if(TransformList.begin(), TransformList.end(), Pred);
      TransformList.erase(NewEnd, TransformList.end());
    };

    SelectiveApplicator([](NodeTransform& NT) -> bool {
      if (auto IfVersioning = dyn_cast<OMPIfClauseVersioningTransform>(NT.Trans))
        return true;
      if (auto DisableSimd = dyn_cast<OMPDisableSimdTransform>(NT.Trans))
        return true;
      return false;
      });

    // Apply full unrolling, loop distribution, vectorization/interleaving.
    SelectiveApplicator([](NodeTransform &NT) -> bool {
      if (auto Unroll = dyn_cast<LoopUnrollingTransform>(NT.Trans))
        return Unroll->isLegacy() && Unroll->isFull();
      if (auto Dist = dyn_cast<LoopDistributionTransform>(NT.Trans))
        return Dist->isLegacy();
      if (auto Vec = dyn_cast<LoopVectorizationInterleavingTransform>(NT.Trans))
        return Vec->isLegacy();
      return false;
    });

    // Apply unrollandjam.
    // While all transformations in TransformList have already been added in the
    // order in the pass pipeline (relative to transformations on the same
    // loop), transformations on unrollandjam's inner loop are not ordered
    // relative to the outer loop.
    SelectiveApplicator([](NodeTransform &NT) -> bool {
      if (auto Unroll = dyn_cast<LoopUnrollAndJamTransform>(NT.Trans))
        return Unroll->isLegacy();
      return false;
    });

    // Apply partial unrolling and software pipelining.
    SelectiveApplicator([](NodeTransform &NT) -> bool {
      if (auto Unroll = dyn_cast<LoopUnrollingTransform>(NT.Trans))
        return Unroll->isLegacy();
      if (isa<LoopPipeliningTransform>(NT.Trans))
        return true;
      return false;
    });

    // Apply all others.
    SelectiveApplicator([](NodeTransform &NT) -> bool { return true; });
    assert(TransformList.size() == 0 && "Must apply all transformations");

    return Root;
  }
};

} // namespace clang
#endif /* LLVM_CLANG_ANALYSIS_TRANSFORMEDTREE_H */
