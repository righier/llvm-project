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
                      clang::Stmt *Original, int FollowupRole,
                      NodeTy *Predecessor)
      : TransformedTree(SubLoops, BasedOn, Original, FollowupRole,
                        Predecessor) {}
};

class SemaTransformedTreeBuilder
    : public TransformedTreeBuilder<SemaTransformedTreeBuilder,
                                    SemaTransformedTree> {
  using NodeTy = SemaTransformedTree;

  Sema &Sem;

public:
  SemaTransformedTreeBuilder(ASTContext &ASTCtx, const LangOptions &LangOpts,
                             llvm::SmallVectorImpl<NodeTy *> &AllNodes,
                             llvm::SmallVectorImpl<Transform *> &AllTransforms,
                             Sema &Sem)
      : TransformedTreeBuilder(ASTCtx, LangOpts, AllNodes, AllTransforms),
        Sem(Sem) {}

  auto Diag(SourceLocation Loc, unsigned DiagID) {
    return Sem.Diag(Loc, DiagID);
  }

  void applyOriginal(SemaTransformedTree *L) {}

  void applyUnrollAndJam(LoopUnrollAndJamTransform *Trans,
                         SemaTransformedTree *OuterLoop,
                         SemaTransformedTree *InnerLoop) {}
  void applyUnroll(LoopUnrollTransform *Trans,
                   SemaTransformedTree *OriginalLoop) {}
  void applyDistribution(LoopDistributionTransform *Trans,
                         SemaTransformedTree *InputLoop) {}
  void applyVectorization(LoopVectorizationTransform *Trans,
                          SemaTransformedTree *InputLoop) {}
  void applyInterleaving(LoopInterleavingTransform *Trans,
                         SemaTransformedTree *InputLoop) {}

  void inheritLoopAttributes(SemaTransformedTree *Dst, SemaTransformedTree *Src,
                             bool IsMeta, bool IsSuccessor) {}

  void finalize(NodeTy *Root) {}
};

} // namespace clang
#endif /* LLVM_CLANG_SEMA_SEMATRANSFORM_H */
