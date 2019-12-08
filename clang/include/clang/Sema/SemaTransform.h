//===---- SemaTransform.h ------------------------------------- -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Semantic analysis for code transformations.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_SEMATRANSFORM_H
#define LLVM_CLANG_SEMA_SEMATRANSFORM_H

#include "clang/Analysis/AnalysisTransform.h"
#include "clang/Analysis/TransformedTree.h"
#include "clang/Sema/Sema.h"

namespace clang {
class Sema;

class SemaTransformedTree : public TransformedTree<SemaTransformedTree> {
  friend class TransformedTree<SemaTransformedTree>;
  using BaseTy = TransformedTree<SemaTransformedTree>;
  using NodeTy = SemaTransformedTree;

  BaseTy &getBase() { return *this; }
  const BaseTy &getBase() const { return *this; }

public:
  SemaTransformedTree(llvm::ArrayRef<NodeTy *> SubLoops, NodeTy *BasedOn,
                      clang::Stmt *Original, int FollowupRole)
      : TransformedTree(SubLoops, BasedOn, Original, FollowupRole) {}
};

class SemaTransformedTreeBuilder
    : public TransformedTreeBuilder<SemaTransformedTreeBuilder,
                                    SemaTransformedTree> {
  using NodeTy = SemaTransformedTree;

  Sema &Sem;

public:
  SemaTransformedTreeBuilder(ASTContext &ASTCtx,const LangOptions &LangOpts, 
                             llvm::SmallVectorImpl<NodeTy *> &AllNodes,
                             llvm::SmallVectorImpl<Transform *> &AllTransforms,
                             Sema &Sem)
      : TransformedTreeBuilder(ASTCtx,LangOpts, AllNodes, AllTransforms), Sem(Sem) {}

  auto Diag(SourceLocation Loc, unsigned DiagID) {
    return Sem.Diag(Loc, DiagID);
  }

  void applyOriginal(SemaTransformedTree *L) {}

  void disableDistribution(SemaTransformedTree *L) {}
  void disableVectorizeInterleave(SemaTransformedTree *L) {}
  void disableUnrollAndJam(SemaTransformedTree *L) {}
  void disableUnroll(SemaTransformedTree *L) {}
  void disablePipelining(SemaTransformedTree *L) {}

  void applyDistribution(LoopDistributionTransform *Trans,
                         SemaTransformedTree *InputLoop) {}
  void applyVectorizeInterleave(LoopVectorizationInterleavingTransform *Trans,
                                SemaTransformedTree *MainLoop) {}
  void disableOMDSimd(SemaTransformedTree *L) {  }
  void applyUnrollAndJam(LoopUnrollAndJamTransform *Trans,
                         SemaTransformedTree *OuterLoop,
                         SemaTransformedTree *InnerLoop) {}
  void applyUnroll(LoopUnrollingTransform *Trans,
                   SemaTransformedTree *OriginalLoop) {}
  void applyPipelining(LoopPipeliningTransform *Trans,
                       SemaTransformedTree *MainLoop) {}
  void applyOMPIfClauseVersioning(OMPIfClauseVersioningTransform *Trans, SemaTransformedTree *MainLoop) {
    assert(MainLoop->isOriginal());
  }


  void inheritLoopAttributes(SemaTransformedTree *Dst, SemaTransformedTree *Src,
                             bool IsAll, bool IsSuccessor) {}

  void finalize(NodeTy* Root) {}
};

} // namespace clang
#endif /* LLVM_CLANG_SEMA_SEMATRANSFORM_H */
