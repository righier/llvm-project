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

  LoopStack.initBuild(getContext(), getLLVMContext(), DebugInfo,
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

void CGTransformedTree::getOrCreateAccessGroups(
    llvm::LLVMContext &LLVMCtx,
    llvm::SmallVectorImpl<llvm::MDNode *> &AccessGroups) {
  if (getOriginal()) {
    if (!AccessGroup)
      AccessGroup = MDNode::getDistinct(LLVMCtx, {});
    AccessGroups.push_back(AccessGroup);
    return;
  }

  getBasedOn()->getOrCreateAccessGroups(LLVMCtx, AccessGroups);
}

llvm::MDNode *CGTransformedTree::makeLoopID(llvm::LLVMContext &Ctx,
                                            bool HasAllDisableNonforced) {
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
    if (TransformedBy->isAllRole(Role)) {
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
    if (TransformedBy->isAllRole(FollowupNode->FollowupRole)) {
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
  if (L->BeginLoc || L->EndLoc)
    L->markNondefault();
}

void CGTransformedTreeBuilder::inheritLoopAttributes(CGTransformedTree *Dst,
                                                     CGTransformedTree *Src,
                                                     bool IsAll,
                                                     bool IsSuccessor) {
  Dst->BeginLoc = Src->BeginLoc;
  Dst->EndLoc = Src->EndLoc;

  for (auto A : Src->Attributes) {
    // TOOD: Check for duplicates?
    Dst->Attributes.push_back(A);
  }

  // We currently assume that every transformation of a parallel loop also
  // results in a parallel loop.
  Dst->ParallelAccessGroups.insert(Src->ParallelAccessGroups.begin(),
                                   Src->ParallelAccessGroups.end());
}

void CGTransformedTreeBuilder::markParallel(CGTransformedTree *L) {
  getBase().markParallel(L);

  // Has it already been marked parallel?
  // Avoid redundant metadata if it was.
  if (!L->ParallelAccessGroups.empty())
    return;

  SmallVector<llvm::MDNode *, 8> AccGroups;
  assert(L->ParallelAccessGroups.empty() &&
         "Should not have parallel access groups if was empty before");

  L->getOrCreateAccessGroups(LLVMCtx, AccGroups);
  L->ParallelAccessGroups.insert(AccGroups.begin(), AccGroups.end());
}

void CGTransformedTreeBuilder::applyUnroll(LoopUnrollingTransform *Trans,
                                           CGTransformedTree *OriginalLoop) {
  if (Trans->isExplicitEnable() && !(Trans->isLegacy() && Trans->isFull()))
    OriginalLoop->addAttribute(LLVMCtx, false, "llvm.loop.unroll.enable");

  if (Trans->isFull()) {
    OriginalLoop->addAttribute(LLVMCtx, false, "llvm.loop.unroll.full");
  } else {
    int UnrollFactor = Trans->getFactor();
    if (UnrollFactor > 0)
      OriginalLoop->addAttribute(LLVMCtx, false, "llvm.loop.unroll.count",
                                 UnrollFactor);
  }

  for (CGTransformedTree *F : OriginalLoop->Followups) {
    switch (F->FollowupRole) {
    case LoopUnrollingTransform::FollowupAll:
      OriginalLoop->FollowupAttributes.emplace_back(LLVMLoopUnrollFollowupAll,
                                                    F);
      if (Trans->isLegacy())
        F->addAttribute(LLVMCtx, true, "llvm.loop.unroll.disable");
      else
        F->markDisableHeuristic();
      break;
    case LoopUnrollingTransform::FollowupUnrolled:
      OriginalLoop->FollowupAttributes.emplace_back(
          LLVMLoopUnrollFollowupUnrolled, F);
      break;
    case LoopUnrollingTransform::FollowupRemainder:
      OriginalLoop->FollowupAttributes.emplace_back(
          LLVMLoopUnrollFollowupRemainder, F);
      break;
    }
  }

  OriginalLoop->markNondefault();
  if (!Trans->isLegacy())
    OriginalLoop->markDisableHeuristic();
}

void CGTransformedTreeBuilder::applyUnrollAndJam(
    LoopUnrollAndJamTransform *Trans, CGTransformedTree *OuterLoop,
    CGTransformedTree *InnerLoop) {
  int Factor = Trans->getFactor();
  if (Factor > 0)
    OuterLoop->addAttribute(LLVMCtx, false, "llvm.loop.unroll_and_jam.count",
                            Factor);

  if (Trans->isExplicitEnable())
    OuterLoop->addAttribute(LLVMCtx, false, "llvm.loop.unroll_and_jam.enable");

  for (CGTransformedTree *F : OuterLoop->Followups) {
    switch (F->FollowupRole) {
    case LoopUnrollAndJamTransform::FollowupAll:
      OuterLoop->FollowupAttributes.emplace_back(
          "llvm.loop.unroll_and_jam.followup_all", F);
      if (!Trans->isLegacy())
        F->markDisableHeuristic();
      break;
    case LoopUnrollAndJamTransform::FollowupOuter:
      OuterLoop->FollowupAttributes.emplace_back(
          "llvm.loop.unroll_and_jam.followup_outer", F);
      if (!Trans->isLegacy())
        F->markDisableHeuristic();
      if (Trans->isLegacy())
        F->addAttribute(LLVMCtx, true, "llvm.loop.unroll_and_jam.disable");
      break;
    }
  }

  if (InnerLoop) {
    for (CGTransformedTree *F : InnerLoop->Followups) {
      switch (F->FollowupRole) {
      case LoopUnrollAndJamTransform::FollowupInner:
        if (!Trans->isLegacy())
          F->markDisableHeuristic();
        OuterLoop->FollowupAttributes.emplace_back(
            "llvm.loop.unroll_and_jam.followup_inner", F);
        break;
      }
    }
  }

  OuterLoop->markNondefault();
  if (!Trans->isLegacy())
    OuterLoop->markDisableHeuristic();
  if (InnerLoop && !Trans->isLegacy())
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
      if (!Trans->isLegacy())
        F->markDisableHeuristic();
      break;
    }
  }

  OriginalLoop->markNondefault();
  if (!Trans->isLegacy())
    OriginalLoop->markDisableHeuristic();
}

void CGTransformedTreeBuilder::applyVectorizeInterleave(
    LoopVectorizationInterleavingTransform *Trans,
    CGTransformedTree *MainLoop) {
  bool DisabledVectorization =
      !Trans->isVectorizationEnabled().getValueOr(true) ||
      Trans->getWidth() == 1;
  bool ForcedInterleaving = Trans->isInterleavingEnabled().getValueOr(false);
  Optional<bool> VecEnabled = Trans->isVectorizationEnabled();
  if (ForcedInterleaving)
    VecEnabled = true;

  if (Trans->isPredicateEnabled().hasValue() && !DisabledVectorization) {
    MainLoop->addAttribute(LLVMCtx, false,
                           "llvm.loop.vectorize.predicate.enable",
                           Trans->isPredicateEnabled().getValue());
    if (Trans->isLegacy())
      VecEnabled = true;
  }

  if (Trans->getWidth() > 0) {
#if 0
    if (Trans->isLegacy() && !VecEnabled.hasValue())
      MainLoop->addAttribute(LLVMCtx, false, "llvm.loop.vectorize.enable",
                             true);
#endif
    MainLoop->addAttribute(LLVMCtx, false, "llvm.loop.vectorize.width",
                           Trans->getWidth());
  }

  if (Trans->getInterleaveCount() > 0)
    MainLoop->addAttribute(LLVMCtx, false, "llvm.loop.interleave.count",
                           Trans->getInterleaveCount());

  if (VecEnabled.hasValue())
    MainLoop->addAttribute(LLVMCtx, false, "llvm.loop.vectorize.enable",
                           VecEnabled.getValue());

  for (CGTransformedTree *F : MainLoop->Followups) {
    switch (F->FollowupRole) {
    case LoopVectorizationInterleavingTransform ::FollowupAll:
      if (Trans->isLegacy())
        F->addAttribute(LLVMCtx, true, "llvm.loop.isvectorized");
      MainLoop->FollowupAttributes.emplace_back(
          "llvm.loop.vectorize.followup_all", F);
      if (!Trans->isLegacy())
        F->markDisableHeuristic();
      break;
    case LoopVectorizationInterleavingTransform ::FollowupVectorized:
      MainLoop->FollowupAttributes.emplace_back(
          "llvm.loop.vectorize.followup_vectorized", F);
      break;
    case LoopVectorizationInterleavingTransform ::FollowupEpilogue:
      MainLoop->FollowupAttributes.emplace_back(
          "llvm.loop.vectorize.followup_epilogue", F);
      break;
    }
  }

  MainLoop->markNondefault();
  if (!Trans->isLegacy())
    MainLoop->markDisableHeuristic();
}

void CGTransformedTreeBuilder::applyPipelining(LoopPipeliningTransform *Trans,
                                               CGTransformedTree *MainLoop) {
  if (Trans->getInitiationInterval() > 0)
    MainLoop->addAttribute(LLVMCtx, false,
                           "llvm.loop.pipeline.initiationinterval",
                           Trans->getInitiationInterval());

  MainLoop->markNondefault();
}

void  CGTransformedTreeBuilder:: applyOMPIfClauseVersioning(OMPIfClauseVersioningTransform* Trans, CGTransformedTree* MainLoop) {
  // Is applied by OpenMP's codegen.
}
