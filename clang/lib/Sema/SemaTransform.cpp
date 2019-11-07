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

#include "clang/Sema/SemaTransform.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtTransform.h"
#include "clang/Basic/Transform.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"

using namespace clang;

static bool isTemplateDependent(Expr *E) {
  return E->isValueDependent() || E->isTypeDependent() ||
         E->isInstantiationDependent() || E->containsUnexpandedParameterPack();
}

Sema::TransformResult
Sema::ActOnTransform(Transform::Kind Kind,
                     llvm::ArrayRef<TransformClause *> Clauses,
                     SourceRange Loc) {
  SmallVector<TransformClause *, 2> ClauseByKind[TransformClause::LastKind + 1];
  for (TransformClause *Clause : Clauses) {
    ClauseByKind[Clause->getKind()].push_back(Clause);
  }

  switch (Kind) {
  case Transform::LoopUnrollingKind: {
    FullClause *Full = nullptr;
    PartialClause *Partial = nullptr;
    for (TransformClause *Clause : Clauses) {
      switch (Clause->getKind()) {
      case TransformClause::FullKind:
        if (Full || Partial) {
          Diag(Clause->getBeginLoc(),
               diag::err_sema_transform_unroll_full_or_partial);
          continue;
        }
        Full = cast<FullClause>(Clause);
        break;
      case TransformClause::PartialKind:
        if (Full || Partial) {
          Diag(Clause->getBeginLoc(),
               diag::err_sema_transform_unroll_full_or_partial);
          continue;
        }
        Partial = cast<PartialClause>(Clause);
        break;
      default:
        llvm_unreachable("Clause not supported by unroll");
      }
    }

    assert(!Full || !Partial);

    if (Full)
      return LoopUnrollingTransform::createFull(Loc, false, true, true);
    else if (Partial) {
      Expr *FactorExpr = Partial->getFactor();
      if (isTemplateDependent(FactorExpr))
        return {};
      Expr::EvalResult FactorRes;
      FactorExpr->EvaluateAsInt(FactorRes, Context);
      if (!FactorRes.Val.isInt())
        return TransformError(
            Diag(FactorExpr->getExprLoc(),
                 diag::err_sema_transform_unroll_factor_expect_int));
      llvm::APSInt FactorVal = FactorRes.Val.getInt();
      int64_t FactorInt = FactorVal.getSExtValue();
      return LoopUnrollingTransform::createPartial(Loc, false, true, true,
                                                   FactorInt);
    }

    return LoopUnrollingTransform::create(Loc, false, true, true);
  }

  case Transform::LoopUnrollAndJamKind: {
    PartialClause *Partial = nullptr;
    for (TransformClause *Clause : Clauses) {
      switch (Clause->getKind()) {
      case TransformClause::PartialKind:
        if (Partial) {
          Diag(Clause->getBeginLoc(),
               diag::err_sema_transform_unroll_partial_once);
          continue;
        }
        Partial = cast<PartialClause>(Clause);
        break;
      default:
        llvm_unreachable("Clause not supported by unroll-and-jam");
      }
    }

    if (Partial) {
      Expr *FactorExpr = Partial->getFactor();
      if (isTemplateDependent(FactorExpr))
        return {};
      Expr::EvalResult FactorRes;
      FactorExpr->EvaluateAsInt(FactorRes, Context);
      llvm::APSInt FactorVal = FactorRes.Val.getInt();
      int64_t FactorInt = FactorVal.getSExtValue();
      return LoopUnrollAndJamTransform::createPartial(Loc, false, true,
                                                      FactorInt);
    }

    return LoopUnrollAndJamTransform::create(Loc, false, true);
  }

  case clang::Transform::LoopDistributionKind:
    assert(Clauses.size() == 0 && "distribute has no clauses");
    return LoopDistributionTransform::create(Loc, false);

  case clang::Transform::LoopVectorizationKind: {
    WidthClause *Width = nullptr;
    for (TransformClause *Clause : Clauses) {
      switch (Clause->getKind()) {
      case TransformClause::WidthKind:
        if (Width) {
          Diag(Clause->getBeginLoc(),
               diag::err_sema_transform_vectorize_width_once);
          continue;
        }
        Width = cast<WidthClause>(Clause);
        break;

      default:
        llvm_unreachable("Clause not supported by distribute");
      }
    }

    int64_t WidthInt = 0;
    if (Width) {
      if (isTemplateDependent(Width->getWidth()))
        return {};

      Expr::EvalResult WidthRes;
      Width->getWidth()->EvaluateAsInt(WidthRes, Context);
      llvm::APSInt WidthVal = WidthRes.Val.getInt();
      WidthInt = WidthVal.getSExtValue();
    }

    return LoopVectorizationTransform::Create(Loc, true, WidthInt, None);
  }

  case clang::Transform::LoopInterleavingKind: {
    FactorClause *Factor = nullptr;

    for (TransformClause *Clause : Clauses) {
      switch (Clause->getKind()) {
      case TransformClause::FactorKind:
        if (Factor) {
          Diag(Clause->getBeginLoc(),
               diag::err_sema_transform_interleave_factor_once);
          continue;
        }
        Factor = cast<FactorClause>(Clause);
        break;
      default:
        llvm_unreachable("Clause not supported by distribute");
      }
    }

    int64_t FactorInt = 0;
    if (Factor) {
      if (isTemplateDependent(Factor->getFactor()))
        return {};

      Expr::EvalResult FactorRes;
      Factor->getFactor()->EvaluateAsInt(FactorRes, Context);
      llvm::APSInt FactorVal = FactorRes.Val.getInt();
      FactorInt = FactorVal.getSExtValue();
    }

    return LoopInterleavingTransform::Create(Loc, true, FactorInt);
  }
  default:
    llvm_unreachable("unimplemented");
  }
StmtResult
Sema::ActOnLoopTransformDirective(Transform::Kind Kind,
                                  llvm::ArrayRef<TransformClause *> Clauses,
                                  Stmt *AStmt, SourceRange Loc) {
  const Stmt *Loop = getAssociatedLoop(AStmt);
  if (!Loop)
    return StmtError(
        Diag(Loc.getBegin(), diag::err_sema_transform_expected_loop));

  if (!Trans) {
    TransformResult Transform = ActOnTransform(Kind, Clauses, Loc);
    Trans = Transform.isUsable() ? Transform.get() : nullptr;
  }

  return TransformExecutableDirective::create(Context, Loc, AStmt, Trans,
                                              Clauses, Kind);
}

TransformClause *Sema::ActOnFullClause(SourceRange Loc) {
  return FullClause::create(Context, Loc);
}

TransformClause *Sema::ActOnPartialClause(SourceRange Loc, Expr *Factor) {
  if (!Factor->isValueDependent() && Factor->isEvaluatable(getASTContext())) {
    llvm::APSInt ValueAPS = Factor->EvaluateKnownConstInt(getASTContext());
    if (!ValueAPS.sge(2)) {
      Diag(Loc.getBegin(), diag::err_sema_transform_unroll_partial_ge_two);
      return nullptr;
    }
  }
  return PartialClause::create(Context, Loc, Factor);
}

TransformClause *Sema::ActOnWidthClause(SourceRange Loc, Expr *Width) {
  return WidthClause::create(Context, Loc, Width);
}

TransformClause *Sema::ActOnFactorClause(SourceRange Loc, Expr *Factor) {
  if (!Factor->isValueDependent() && Factor->isEvaluatable(getASTContext())) {
    llvm::APSInt ValueAPS = Factor->EvaluateKnownConstInt(getASTContext());
    if (!ValueAPS.sge(2)) {
      Diag(Loc.getBegin(), diag::err_sema_transform_interleave_factor_ge_two);
      return nullptr;
    }
  }
  return FactorClause::create(Context, Loc, Factor);
}

void Sema::HandleLoopTransformations(FunctionDecl *FD) {
  if (!FD || FD->isInvalidDecl())
    return;

  // Note: this is called on a template-code and the instantiated code.
  llvm::DenseMap<Stmt *, SemaTransformedTree *> StmtToTree;
  llvm::SmallVector<SemaTransformedTree *, 64> AllNodes;
  SemaTransformedTreeBuilder Builder(getASTContext(), AllNodes, *this);
  Builder.computeTransformedStructure(FD->getBody(), StmtToTree);

  for (auto N : AllNodes)
    delete N;
}
