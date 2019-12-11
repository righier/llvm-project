//===---- AnalysisTransform.h --------------------------------- -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Extract the transformation to apply from a #pragma clang transform AST node.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSISTRANSFORM_H
#define LLVM_CLANG_ANALYSIS_ANALYSISTRANSFORM_H

#include "clang/AST/StmtTransform.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/Transform.h"

namespace clang {

static bool isTemplateDependent(Expr *E) {
  return E->isValueDependent() || E->isTypeDependent() ||
         E->isInstantiationDependent() || E->containsUnexpandedParameterPack();
}

/// Extract which transformation to apply from a TransformExecutableDirective
/// and its clauses.
template <typename Derived> struct ExtractTransform {
  Derived &getDerived() { return *static_cast<Derived *>(this); }
  const Derived &getDerived() const {
    return *static_cast<const Derived *>(this);
  }

  ASTContext &ASTCtx;
  const TransformExecutableDirective *Directive;
  bool AnyError = false;
  bool TemplateDependent = false;
  ExtractTransform(ASTContext &ASTCtx,
                   const TransformExecutableDirective *Directive)
      : ASTCtx(ASTCtx), Directive(Directive) {
    assert(Directive);
  }

  auto DiagError(SourceLocation Loc, unsigned DiagID) {
    AnyError = true;
    return getDerived().Diag(Loc, DiagID);
  }

  template <typename ClauseTy> ClauseTy *assumeSingleClause() {
    ClauseTy *Result = nullptr;
    for (ClauseTy *C : Directive->getClausesOfKind<ClauseTy>()) {
      if (Result) {
        DiagError(C->getBeginLoc(), diag::err_sema_transform_clause_one_max)
            << TransformClause::getClauseKeyword(C->getKind()) << C->getRange();
        break;
      }

      Result = C;
    }

    return Result;
  }

  llvm::Optional<int64_t> evalIntArg(Expr *E, int MinVal) {
    if (isTemplateDependent(E)) {
      TemplateDependent = true;
      return None;
    }
    Expr::EvalResult Res;
    E->EvaluateAsInt(Res, ASTCtx);
    if (!Res.Val.isInt()) {
      DiagError(E->getExprLoc(),
                diag::err_sema_transform_clause_arg_expect_int);
      return None;
    }
    llvm::APSInt Val = Res.Val.getInt();
    int64_t Int = Val.getSExtValue();
    if (Int < MinVal) {
      DiagError(E->getExprLoc(), diag::err_sema_transform_clause_arg_min_val)
          << MinVal << SourceRange(E->getBeginLoc(), E->getEndLoc());
      return None;
    }
    return Int;
  }

  void allowedClauses(ArrayRef<TransformClause::Kind> ClauseKinds) {
#ifndef NDEBUG
    for (TransformClause *C : Directive->clauses()) {
      assert(llvm::find(ClauseKinds, C->getKind()) != ClauseKinds.end() &&
             "Parser must have rejected unknown clause");
    }
#endif
  }

  static std::unique_ptr<Transform> wrap(Transform *Trans) {
    return std::unique_ptr<Transform>(Trans);
  }

  std::unique_ptr<Transform> createTransform() {
    Transform::Kind Kind = Directive->getTransformKind();
    SourceRange Loc = Directive->getRange();

    switch (Kind) {
    case Transform::LoopUnrollKind: {
      allowedClauses({TransformClause::FullKind, TransformClause::PartialKind});
      FullClause *Full = assumeSingleClause<FullClause>();
      PartialClause *Partial = assumeSingleClause<PartialClause>();
      if (Full && Partial)
        DiagError(Full->getBeginLoc(),
                  diag::err_sema_transform_unroll_full_or_partial);

      if (AnyError)
        return nullptr;

      if (Full) {
        return wrap(LoopUnrollTransform::createFull(Loc));
      } else if (Partial) {
        llvm::Optional<int64_t> Factor = evalIntArg(Partial->getFactor(), 2);
        if (AnyError || !Factor.hasValue())
          return nullptr;
        return wrap(LoopUnrollTransform::createPartial(Loc, Factor.getValue()));
      }

      return wrap(LoopUnrollTransform::createHeuristic(Loc));
    }

    case Transform::LoopUnrollAndJamKind: {
      allowedClauses({TransformClause::PartialKind});
      PartialClause *Partial = assumeSingleClause<PartialClause>();

      if (AnyError)
        return nullptr;

      if (Partial) {
        llvm::Optional<int64_t> Factor = evalIntArg(Partial->getFactor(), 2);
        if (AnyError || !Factor.hasValue())
          return nullptr;
        return wrap(
            LoopUnrollAndJamTransform::createPartial(Loc, Factor.getValue()));
      }

      return wrap(LoopUnrollAndJamTransform::createHeuristic(Loc));
    }

    case clang::Transform::LoopDistributionKind:
      allowedClauses({});
      return wrap(LoopDistributionTransform::create(Loc));

    case clang::Transform::LoopVectorizationKind: {
      allowedClauses({TransformClause::WidthKind});
      WidthClause *Width = assumeSingleClause<WidthClause>();
      if (AnyError)
        return nullptr;

      int64_t Simdlen = 0;
      if (Width) {
        llvm::Optional<int64_t> WidthInt = evalIntArg(Width->getWidth(), 2);
        if (AnyError || !WidthInt.hasValue())
          return nullptr;
        Simdlen = WidthInt.getValue();
      }

      return wrap(LoopVectorizationTransform::create(Loc, Simdlen));
    }

    case clang::Transform::LoopInterleavingKind: {
      allowedClauses({TransformClause::FactorKind});
      FactorClause *Factor = assumeSingleClause<FactorClause>();
      if (AnyError)
        return nullptr;

      int64_t InterleaveFactor = 0;
      if (Factor) {
        llvm::Optional<int64_t> FactorInt = evalIntArg(Factor->getFactor(), 2);
        if (AnyError || !FactorInt.hasValue())
          return nullptr;
        InterleaveFactor = FactorInt.getValue();
      }

      return wrap(LoopInterleavingTransform::create(Loc, InterleaveFactor));
    }
    default:
      llvm_unreachable("unimplemented");
    }
  }
};

} // namespace clang
#endif /* LLVM_CLANG_ANALYSIS_ANALYSISTRANSFORM_H */
