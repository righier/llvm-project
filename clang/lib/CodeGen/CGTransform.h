//===---- CGTransform.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Emitting metadata for loop transformations.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_CODEGEN_CGTRANSFORM_H
#define LLVM_CLANG_LIB_CODEGEN_CGTRANSFORM_H

#include "clang/Analysis/TransformedTree.h"
#include "llvm/IR/DebugLoc.h"

namespace clang {
namespace CodeGen {
class CGDebugInfo;
class CGTransformedTreeBuilder;

class CGTransformedTree : public TransformedTree<CGTransformedTree> {
  friend class CGTransformedTreeBuilder;
  friend class TransformedTree<CGTransformedTree>;
  using BaseTy = TransformedTree<CGTransformedTree>;
  using NodeTy = CGTransformedTree;

  BaseTy &getBase() { return *this; }
  const BaseTy &getBase() const { return *this; }

  llvm::DebugLoc BeginLoc;
  llvm::DebugLoc EndLoc;

  llvm::MDNode *AccessGroup = nullptr;
  llvm::SmallVector<llvm::MDNode *, 2> ParallelAccessGroups;

  bool Finalized = false;

public:
  CGTransformedTree(llvm::ArrayRef<NodeTy *> SubLoops, NodeTy *BasedOn,
                    clang::Stmt *Original, int FollowupRole,
                    NodeTy *Predecessor)
      : TransformedTree(SubLoops, BasedOn, Original, FollowupRole, Predecessor),
        IsCodeGenned(Original) {}

  bool IsDefault = true;
  bool DisableHeuristic = false;
  bool IsCodeGenned;

  llvm::SmallVector<llvm::Metadata *, 8> Attributes;
  llvm::SmallVector<llvm::Metadata *, 4> Transforms;
  llvm::SmallVector<std::pair<llvm::StringRef, NodeTy *>, 4> FollowupAttributes;

  bool collectLoopProperties(llvm::SmallVectorImpl<llvm::Metadata *> &Props);
  void addAttribute(bool Inherited, llvm::Metadata *Node);

public:
  void markNondefault() { IsDefault = false; }
  void markDisableHeuristic() { DisableHeuristic = true; }

  void addAttribute(llvm::LLVMContext &LLVMCtx, bool Inherited,
                    llvm::ArrayRef<llvm::Metadata *> Vals);
  void addAttribute(llvm::LLVMContext &LLVMCtx, bool Inherited,
                    llvm::StringRef Name);
  void addAttribute(llvm::LLVMContext &LLVMCtx, bool Inherited,
                    llvm::StringRef Name, bool Val);
  void addAttribute(llvm::LLVMContext &LLVMCtx, bool Inherited,
                    llvm::StringRef Name, int Val);

  llvm::MDNode *getAccessGroupOrNull() { return AccessGroup; }

  llvm::MDNode *makeAccessGroup(llvm::LLVMContext &LLVMCtx);

  void
  getOrCreateAccessGroups(llvm::LLVMContext &LLVMCtx,
                          llvm::SmallVectorImpl<llvm::MDNode *> &AccessGroups);
  void collectAccessGroups(llvm::LLVMContext &LLVMCtx,
                           llvm::SmallVectorImpl<llvm::MDNode *> &AccessGroups);

  void finalize(llvm::LLVMContext &LLVMCtx) { Finalized = true; }

  llvm::ArrayRef<llvm::MDNode *> getParallelAccessGroups() const {
    assert(Finalized);
    return ParallelAccessGroups;
  }

  llvm::MDNode *makeLoopID(llvm::LLVMContext &Ctx, bool HasAllDisableNonforced);

  bool isCodeGenned() const { return IsCodeGenned; }
};

class CGTransformedTreeBuilder
    : public TransformedTreeBuilder<CGTransformedTreeBuilder,
                                    CGTransformedTree> {
  using BaseTy =
      TransformedTreeBuilder<CGTransformedTreeBuilder, CGTransformedTree>;
  using NodeTy = CGTransformedTree;

  BaseTy &getBase() { return *this; }
  const BaseTy &getBase() const { return *this; }

  llvm::LLVMContext &LLVMCtx;
  CGDebugInfo *DbgInfo;

public:
  CGTransformedTreeBuilder(ASTContext &ASTCtx, const LangOptions &LangOpts,
                           llvm::LLVMContext &LLVMCtx,
                           llvm::SmallVectorImpl<NodeTy *> &AllNodes,
                           llvm::SmallVectorImpl<Transform *> &AllTransforms,
                           CGDebugInfo *DbgInfo)
      : TransformedTreeBuilder(ASTCtx, LangOpts, AllNodes, AllTransforms),
        LLVMCtx(LLVMCtx), DbgInfo(DbgInfo) {}

  // Ignore any diagnostic and its arguments.
  struct DummyDiag {
    template <typename T> DummyDiag operator<<(const T &) const { return {}; }
  };
  DummyDiag Diag(SourceLocation Loc, unsigned DiagID) { return {}; }

  void applyOriginal(CGTransformedTree *L);

  void inheritLoopAttributes(CGTransformedTree *Dst, CGTransformedTree *Src,
                             bool IsMeta, bool IsSuccessor);

  void applyUnroll(LoopUnrollTransform *Trans, CGTransformedTree *OriginalLoop);
  void applyUnrollAndJam(LoopUnrollAndJamTransform *Trans,
                         CGTransformedTree *OuterLoop,
                         CGTransformedTree *InnerLoop);
  void applyDistribution(LoopDistributionTransform *Trans,
                         CGTransformedTree *OriginalLoop);
  void applyVectorization(LoopVectorizationTransform *Trans,
                          CGTransformedTree *InputLoop);
  void applyInterleaving(LoopInterleavingTransform *Trans,
                         CGTransformedTree *InputLoop);

  void finalize(NodeTy *Root);
};

} // namespace CodeGen
} // namespace clang
#endif /* LLVM_CLANG_LIB_CODEGEN_CGTRANSFORM_H */
