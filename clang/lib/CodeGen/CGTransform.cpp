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

#include "CGTransform.h"
#include "CGDebugInfo.h"
#include "CGLoopInfo.h"
#include "CodeGenFunction.h"
#include "clang/AST/StmtTransform.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"

using namespace clang;
using namespace clang::CodeGen;
using namespace llvm;

void CodeGenFunction::HandleCodeTransformations(const Stmt *Body) {
  if (!getParentFn()) {
    // Transformations not supported for e.g. Objective-C
    return;
  }

  assert(CurFn && "Must be called after StartFunction");
  assert(Body);

  LoopStack.initBuild(getContext(), getLangOpts(), getLLVMContext(), DebugInfo,
                      getParentFn()->getBody());
}

void CodeGenFunction::EmitTransformExecutableDirective(
    const TransformExecutableDirective &D) {
  EmitStmt(D.getAssociated());
}

bool CGTransformedTree::collectLoopProperties(
    llvm::SmallVectorImpl<llvm::Metadata *> &Props) {
  for (Metadata *M : this->Attributes)
    Props.push_back(M);
  for (Metadata *M : this->Transforms)
    Props.push_back(M);
  return !Props.empty();
}

void CGTransformedTree::addAttribute(bool Inherited, llvm::Metadata *Node) {
  assert(!Finalized);
  if (Inherited)
    Attributes.push_back(Node);
  else
    Transforms.push_back(Node);
}

void CGTransformedTree::addAttribute(llvm::LLVMContext &LLVMCtx, bool Inherited,
                                     llvm::ArrayRef<llvm::Metadata *> Vals) {
  addAttribute(Inherited, MDNode::get(LLVMCtx, Vals));
}

void CGTransformedTree::addAttribute(llvm::LLVMContext &LLVMCtx, bool Inherited,
                                     llvm::StringRef Name) {
  addAttribute(LLVMCtx, Inherited, {MDString::get(LLVMCtx, Name)});
}

void CGTransformedTree::addAttribute(llvm::LLVMContext &LLVMCtx, bool Inherited,
                                     llvm::StringRef Name, bool Val) {
  addAttribute(LLVMCtx, Inherited,
               {MDString::get(LLVMCtx, Name),
                ConstantAsMetadata::get(
                    ConstantInt::get(llvm::Type::getInt1Ty(LLVMCtx), Val))});
}
void CGTransformedTree::addAttribute(llvm::LLVMContext &LLVMCtx, bool Inherited,
                                     llvm::StringRef Name, int Val) {
  addAttribute(LLVMCtx, Inherited,
               {MDString::get(LLVMCtx, Name),
                ConstantAsMetadata::get(
                    ConstantInt::get(llvm::Type::getInt32Ty(LLVMCtx), Val))});
}

llvm::MDNode *CGTransformedTree::makeAccessGroup(llvm::LLVMContext &LLVMCtx) {
  if (!AccessGroup) {
    if (IsCodeGenned)
      AccessGroup = MDNode::getDistinct(LLVMCtx, {});
  }
  assert(!AccessGroup == !IsCodeGenned &&
         "Non-codegenned loop must not have an access group");
  return AccessGroup;
}

void CGTransformedTree::getOrCreateAccessGroups(
    llvm::LLVMContext &LLVMCtx,
    llvm::SmallVectorImpl<llvm::MDNode *> &AccessGroups) {
  assert(
      (IsCodeGenned || !getOriginal()) &&
      "Original loop should not be emitted if its transformed successors are");

  if (IsCodeGenned) {
    if (!AccessGroup)
      AccessGroup = MDNode::getDistinct(LLVMCtx, {});
    AccessGroups.push_back(AccessGroup);
    return;
  }

  getBasedOn()->getOrCreateAccessGroups(LLVMCtx, AccessGroups);
}

void CGTransformedTree::collectAccessGroups(
    llvm::LLVMContext &LLVMCtx,
    llvm::SmallVectorImpl<llvm::MDNode *> &AccessGroups) {
  auto AccGroup = makeAccessGroup(LLVMCtx);
  if (AccGroup)
    AccessGroups.push_back(AccGroup);
  if (BasedOn)
    BasedOn->collectAccessGroups(LLVMCtx, AccessGroups);
}

llvm::MDNode *CGTransformedTree::makeLoopID(llvm::LLVMContext &Ctx,
                                            bool HasAllDisableNonforced) {
  assert(Finalized);
  if (IsDefault && (!DisableHeuristic || HasAllDisableNonforced))
    return nullptr;

  SmallVector<Metadata *, 16> Args;

  // Reserve operand 0 for loop id self reference.
  Args.push_back(nullptr);

  if (BeginLoc) {
    Args.push_back(BeginLoc.getAsMDNode());

    // If we also have a valid end debug location for the loop, add it.
    if (EndLoc)
      Args.push_back(EndLoc.getAsMDNode());
  }

  if (!ParallelAccessGroups.empty()) {
    SmallVector<Metadata *, 4> ArgOpts;
    ArgOpts.reserve(ParallelAccessGroups.size());
    ArgOpts.push_back(MDString::get(Ctx, "llvm.loop.parallel_accesses"));
    ArgOpts.insert(ArgOpts.end(), ParallelAccessGroups.begin(),
                   ParallelAccessGroups.end());
    Args.push_back(MDNode::get(Ctx, ArgOpts));
  }

  collectLoopProperties(Args);

  bool AllIsDisableHeuristic = false;
  bool OtherIsNondefault = false;
  for (auto P : FollowupAttributes) {
    int Role = P.second->FollowupRole;
    if (TransformedBy->isMetaRole(Role)) {
      if (P.second->DisableHeuristic)
        AllIsDisableHeuristic = true;
    } else {
      if (!P.second->IsDefault)
        OtherIsNondefault = true;
    }
  }

  for (auto P : FollowupAttributes) {
    StringRef FollowupName = P.first;
    NodeTy *FollowupNode = P.second;
    llvm::MDNode *FollowupId;
    if (TransformedBy->isMetaRole(FollowupNode->FollowupRole)) {
      if (OtherIsNondefault)
        FollowupNode->markNondefault();
      FollowupId = FollowupNode->makeLoopID(Ctx, false);
    } else {
      FollowupId = FollowupNode->makeLoopID(Ctx, AllIsDisableHeuristic);
    }
    if (!FollowupId)
      continue;

    Args.push_back(
        MDNode::get(Ctx, {MDString::get(Ctx, FollowupName), FollowupId}));
  }

  if (DisableHeuristic && !HasAllDisableNonforced)
    Args.push_back(
        MDNode::get(Ctx, {MDString::get(Ctx, "llvm.loop.disable_nonforced")}));

  // No need for an MDNode if it is empty.
  if (Args.size() <= 1)
    return nullptr;

  // Set the first operand to itself.
  MDNode *LoopID = MDNode::getDistinct(Ctx, Args);
  LoopID->replaceOperandWith(0, LoopID);
  return LoopID;
}

void CGTransformedTreeBuilder::applyOriginal(CGTransformedTree *L) {
  if (!DbgInfo)
    return;

  L->BeginLoc = DbgInfo->SourceLocToDebugLoc(L->getOriginal()->getBeginLoc());
  L->EndLoc = DbgInfo->SourceLocToDebugLoc(L->getOriginal()->getEndLoc());
}

void CGTransformedTreeBuilder::inheritLoopAttributes(CGTransformedTree *Dst,
                                                     CGTransformedTree *Src,
                                                     bool IsMeta,
                                                     bool IsSuccessor) {
  Dst->BeginLoc = Src->BeginLoc;
  Dst->EndLoc = Src->EndLoc;

  if (!IsMeta)
    for (llvm::Metadata *A : Src->Attributes)
      Dst->Attributes.push_back(A);
}

void CGTransformedTreeBuilder::applyUnroll(LoopUnrollTransform *Trans,
                                           CGTransformedTree *OriginalLoop) {
  OriginalLoop->addAttribute(LLVMCtx, false, "llvm.loop.unroll.enable");

  if (Trans->isFull()) {
    OriginalLoop->addAttribute(LLVMCtx, false, "llvm.loop.unroll.full");
  } else {
    int Factor = Trans->getFactor();
    if (Factor > 0)
      OriginalLoop->addAttribute(LLVMCtx, false, "llvm.loop.unroll.count",
                                 Factor);
  }

  for (CGTransformedTree *F : OriginalLoop->Followups) {
    switch (F->FollowupRole) {
    case LoopUnrollTransform::FollowupAll:
      OriginalLoop->FollowupAttributes.emplace_back(LLVMLoopUnrollFollowupAll,
                                                    F);
      F->markDisableHeuristic();
      break;
    case LoopUnrollTransform::FollowupUnrolled:
      OriginalLoop->FollowupAttributes.emplace_back(
          LLVMLoopUnrollFollowupUnrolled, F);
      break;
    case LoopUnrollTransform::FollowupRemainder:
      OriginalLoop->FollowupAttributes.emplace_back(
          LLVMLoopUnrollFollowupRemainder, F);
      break;
    }
  }

  OriginalLoop->markNondefault();
  OriginalLoop->markDisableHeuristic();
}

void CGTransformedTreeBuilder::applyUnrollAndJam(
    LoopUnrollAndJamTransform *Trans, CGTransformedTree *OuterLoop,
    CGTransformedTree *InnerLoop) {
  OuterLoop->addAttribute(LLVMCtx, false, "llvm.loop.unroll_and_jam.enable");

  int Factor = Trans->getFactor();
  if (Factor > 0)
    OuterLoop->addAttribute(LLVMCtx, false, "llvm.loop.unroll_and_jam.count",
                            Factor);

  for (CGTransformedTree *F : OuterLoop->Followups) {
    switch (F->FollowupRole) {
    case LoopUnrollAndJamTransform::FollowupAll:
      OuterLoop->FollowupAttributes.emplace_back(
          "llvm.loop.unroll_and_jam.followup_all", F);
      F->markDisableHeuristic();
      break;
    case LoopUnrollAndJamTransform::FollowupOuter:
      OuterLoop->FollowupAttributes.emplace_back(
          "llvm.loop.unroll_and_jam.followup_outer", F);
      break;
    case LoopUnrollAndJamTransform::FollowupInner:
      // LLVM's LoopUnrollAndJam pass expects the followup attributes for the
      // inner loop to be attached to the outer loop.
      OuterLoop->FollowupAttributes.emplace_back(
          "llvm.loop.unroll_and_jam.followup_inner", F);
      break;
    }
  }

  OuterLoop->markNondefault();
  OuterLoop->markDisableHeuristic();
  InnerLoop->markDisableHeuristic();
}

void CGTransformedTreeBuilder::applyDistribution(
    LoopDistributionTransform *Trans, CGTransformedTree *OriginalLoop) {
  OriginalLoop->addAttribute(LLVMCtx, false, "llvm.loop.distribute.enable",
                             true);

  for (CGTransformedTree *F : OriginalLoop->Followups) {
    switch (F->FollowupRole) {
    case LoopDistributionTransform::FollowupAll:
      OriginalLoop->FollowupAttributes.emplace_back(
          "llvm.loop.distribute.followup_all", F);
      F->markDisableHeuristic();
      break;
    }
  }

  OriginalLoop->markNondefault();
  OriginalLoop->markDisableHeuristic();
}

void CGTransformedTreeBuilder::applyVectorization(
    LoopVectorizationTransform *Trans, CGTransformedTree *MainLoop) {
  // Enable vectorization, disable interleaving.
  MainLoop->addAttribute(LLVMCtx, false, "llvm.loop.vectorize.enable", true);
  MainLoop->addAttribute(LLVMCtx, false, "llvm.loop.interleave.count", 1);

  // If SIMD width is specified, forward it.
  int Width = Trans->getWidth();
  if (Width > 0)
    MainLoop->addAttribute(LLVMCtx, false, "llvm.loop.vectorize.width", Width);

  for (CGTransformedTree *F : MainLoop->Followups) {
    switch (F->FollowupRole) {
    case LoopVectorizationTransform ::FollowupAll:
      MainLoop->FollowupAttributes.emplace_back(
          "llvm.loop.vectorize.followup_all", F);
      F->markDisableHeuristic();
      break;
    case LoopVectorizationTransform ::FollowupVectorized:
      MainLoop->FollowupAttributes.emplace_back(
          "llvm.loop.vectorize.followup_vectorized", F);
      // F->markDisableHeuristic();
      break;
    case LoopVectorizationTransform ::FollowupEpilogue:
      MainLoop->FollowupAttributes.emplace_back(
          "llvm.loop.vectorize.followup_epilogue", F);
      // F->markDisableHeuristic();
      break;
    }
  }

  MainLoop->markNondefault();
  MainLoop->markDisableHeuristic();
}

void CGTransformedTreeBuilder::applyInterleaving(
    LoopInterleavingTransform *Trans, CGTransformedTree *MainLoop) {
  // Enable the LoopVectorize pass, but explicitly disable vectorization to only
  // apply interleaving.
  MainLoop->addAttribute(LLVMCtx, false, "llvm.loop.vectorize.enable", true);
  MainLoop->addAttribute(LLVMCtx, false, "llvm.loop.vectorize.width", 1);

  // If interleave factor is specified, forward it.
  int Factor = Trans->getFactor();
  if (Factor > 0)
    MainLoop->addAttribute(LLVMCtx, false, "llvm.loop.interleave.count",
                           Factor);

  for (CGTransformedTree *F : MainLoop->Followups) {
    switch (F->FollowupRole) {
    case LoopInterleavingTransform ::FollowupAll:
      MainLoop->FollowupAttributes.emplace_back(
          "llvm.loop.vectorize.followup_all", F);
      F->markDisableHeuristic();
      break;
    case LoopInterleavingTransform ::FollowupInterleaved:
      MainLoop->FollowupAttributes.emplace_back(
          "llvm.loop.vectorize.followup_vectorized", F);
      break;
    case LoopInterleavingTransform ::FollowupEpilogue:
      MainLoop->FollowupAttributes.emplace_back(
          "llvm.loop.vectorize.followup_epilogue", F);
      break;
    }
  }

  MainLoop->markNondefault();
  MainLoop->markDisableHeuristic();
}

void CGTransformedTreeBuilder::finalize(NodeTy *Root) {
  SmallVector<CGTransformedTree *, 32> Worklist;
  SmallSet<CGTransformedTree *, 32> Visited;
  Worklist.push_back(Root);

  while (!Worklist.empty()) {
    auto *N = Worklist.pop_back_val();
    auto It = Visited.insert(N);
    if (!It.second)
      continue;

    N->finalize(LLVMCtx);

    for (auto SubLoop : N->getSubLoops())
      Worklist.push_back(SubLoop);

    for (auto Followup : N->getFollowups())
      Worklist.push_back(Followup);
  }
}
