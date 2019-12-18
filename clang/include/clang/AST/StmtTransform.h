//===--- StmtTransform.h - Code transformation AST nodes --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  Transformation directive statement and clauses for the AST.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_STMTTRANSFROM_H
#define LLVM_CLANG_AST_STMTTRANSFROM_H

#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/Transform.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {

/// Represents a clause of a \p TransformExecutableDirective.
class TransformClause {
public:
  enum Kind {
    UnknownKind,
#define TRANSFORM_CLAUSE(Keyword, Name) Name##Kind,
#define TRANSFORM_CLAUSE_LAST(Keyword, Name) Name##Kind, LastKind = Name##Kind
#include "clang/AST/TransformClauseKinds.def"
  };

  static bool isValidForTransform(Transform::Kind TransformKind,
                                  TransformClause::Kind ClauseKind);
  static Kind getClauseKind(Transform::Kind TransformKind, llvm::StringRef Str);
  static llvm::StringRef getClauseKeyword(TransformClause::Kind ClauseKind);

private:
  Kind ClauseKind;
  SourceRange LocRange;

protected:
  TransformClause(Kind K, SourceRange Range) : ClauseKind(K), LocRange(Range) {}
  TransformClause(Kind K) : ClauseKind(K) {}

public:
  Kind getKind() const { return ClauseKind; }

  SourceRange getRange() const { return LocRange; }
  SourceLocation getBeginLoc() const { return LocRange.getBegin(); }
  SourceLocation getEndLoc() const { return LocRange.getEnd(); }
  void setRange(SourceRange L) { LocRange = L; }
  void setRange(SourceLocation BeginLoc, SourceLocation EndLoc) {
    LocRange = SourceRange(BeginLoc, EndLoc);
  }

  using child_iterator = Stmt::child_iterator;
  using const_child_iterator = Stmt::const_child_iterator;
  using child_range = Stmt::child_range;
  using const_child_range = Stmt::const_child_range;

  child_range children();
  const_child_range children() const {
    auto Children = const_cast<TransformClause *>(this)->children();
    return const_child_range(Children.begin(), Children.end());
  }

  static llvm::StringRef getClauseName(Kind K);

  void print(llvm::raw_ostream &OS, const PrintingPolicy &Policy) const;
};

class TransformClauseImpl : public TransformClause {
protected:
  TransformClauseImpl(Kind ClauseKind, SourceRange Range)
      : TransformClause(ClauseKind, Range) {}
  explicit TransformClauseImpl(Kind ClauseKind) : TransformClause(ClauseKind) {}

public:
  void print(llvm::raw_ostream &OS, const PrintingPolicy &Policy) const {
    llvm_unreachable("implement the print function");
  }

  child_range children() {
    // By default, clauses have no children.
    return child_range(child_iterator(), child_iterator());
  }
};

class FullClause final : public TransformClauseImpl {
private:
  explicit FullClause(SourceRange Range)
      : TransformClauseImpl(TransformClause::FullKind, Range) {}
  FullClause() : TransformClauseImpl(TransformClause::FullKind) {}

public:
  static bool classof(const FullClause *T) { return true; }
  static bool classof(const TransformClause *T) {
    return T->getKind() == FullKind;
  }

  static FullClause *create(ASTContext &Context, SourceRange Range) {
    return new (Context) FullClause(Range);
  }
  static FullClause *createEmpty(ASTContext &Context) {
    return new (Context) FullClause();
  }

  void print(llvm::raw_ostream &OS, const PrintingPolicy &Policy) const;
};

class PartialClause final : public TransformClauseImpl {
private:
  Stmt *Factor;

  PartialClause(SourceRange Range, Expr *Factor)
      : TransformClauseImpl(TransformClause::PartialKind, Range),
        Factor(Factor) {}
  PartialClause() : TransformClauseImpl(TransformClause::PartialKind) {}

public:
  static bool classof(const PartialClause *T) { return true; }
  static bool classof(const TransformClause *T) {
    return T->getKind() == PartialKind;
  }

  static PartialClause *create(ASTContext &Context, SourceRange Range,
                               Expr *Factor) {
    return new (Context) PartialClause(Range, Factor);
  }
  static PartialClause *createEmpty(ASTContext &Context) {
    return new (Context) PartialClause();
  }

  child_range children() { return child_range(&Factor, &Factor + 1); }

  Expr *getFactor() const { return cast<Expr>(Factor); }
  void setFactor(Expr *E) { Factor = E; }

  void print(llvm::raw_ostream &OS, const PrintingPolicy &Policy) const;
};

class WidthClause final : public TransformClauseImpl {
private:
  Stmt *Width;

  WidthClause(SourceRange Range, Expr *Width)
      : TransformClauseImpl(TransformClause::WidthKind, Range), Width(Width) {}
  WidthClause() : TransformClauseImpl(TransformClause::WidthKind) {}

public:
  static bool classof(const WidthClause *T) { return true; }
  static bool classof(const TransformClause *T) {
    return T->getKind() == WidthKind;
  }

  static WidthClause *create(ASTContext &Context, SourceRange Range,
                             Expr *Width) {
    return new (Context) WidthClause(Range, Width);
  }
  static WidthClause *createEmpty(ASTContext &Context) {
    return new (Context) WidthClause();
  }

  child_range children() { return child_range(&Width, &Width + 1); }

  Expr *getWidth() const { return cast<Expr>(Width); }
  void setWidth(Expr *E) { Width = E; }

  void print(llvm::raw_ostream &OS, const PrintingPolicy &Policy) const;
};

class FactorClause final : public TransformClauseImpl {
private:
  Stmt *Factor;

  FactorClause(SourceRange Range, Expr *Factor)
      : TransformClauseImpl(TransformClause::FactorKind, Range),
        Factor(Factor) {}
  FactorClause() : TransformClauseImpl(TransformClause::FactorKind) {}

public:
  static bool classof(const FactorClause *T) { return true; }
  static bool classof(const TransformClause *T) {
    return T->getKind() == FactorKind;
  }

  static FactorClause *create(ASTContext &Context, SourceRange Range,
                              Expr *Factor) {
    return new (Context) FactorClause(Range, Factor);
  }
  static FactorClause *createEmpty(ASTContext &Context) {
    return new (Context) FactorClause();
  }

  child_range children() { return child_range(&Factor, &Factor + 1); }

  Expr *getFactor() const { return cast<Expr>(Factor); }
  void setFactor(Expr *E) { Factor = E; }

  void print(llvm::raw_ostream &OS, const PrintingPolicy &Policy) const;
};

/// Visitor pattern for transform clauses.
template <class ImplClass, template <typename> class Ptr, typename RetTy>
class TransformClauseVisitorBase {
protected:
  ImplClass &getDerived() { return *static_cast<ImplClass *>(this); }
  const ImplClass &getDerived() const {
    return *static_cast<const ImplClass *>(this);
  }

public:
#define PTR(CLASS) typename Ptr<CLASS>::type

#define TRANSFORM_CLAUSE(Keyword, Name)                                        \
  RetTy Visit##Name##Clause(PTR(Name##Clause) S) {                             \
    return getDerived().VisitTransformClause(S);                               \
  }
#include "clang/AST/TransformClauseKinds.def"

  RetTy Visit(PTR(TransformClause) C) {
    switch (C->getKind()) {
#define TRANSFORM_CLAUSE(Keyword, Name)                                        \
  case TransformClause::Kind::Name##Kind:                                      \
    return getDerived().Visit##Name##Clause(static_cast<PTR(Name##Clause)>(C));
#include "clang/AST/TransformClauseKinds.def"
    default:
      llvm_unreachable("Unknown transform clause kind!");
    }
  }

  // Base case
  RetTy VisitTransformClause(PTR(TransformClause) C) { return RetTy(); }

#undef PTR
};

template <class ImplClass, typename RetTy = void>
class TransformClauseVisitor
    : public TransformClauseVisitorBase<ImplClass, std::add_pointer, RetTy> {};

template <class ImplClass, typename RetTy = void>
class ConstTransformClauseVisitor
    : public TransformClauseVisitorBase<ImplClass, llvm::make_const_ptr,
                                        RetTy> {};

/// Represents
///
///   #pragma clang transform
///
/// in the AST.
class TransformExecutableDirective final
    : public Stmt,
      private llvm::TrailingObjects<TransformExecutableDirective,
                                    TransformClause *> {
public:
  friend TransformClause;
  friend TrailingObjects;

private:
  SourceRange LocRange;
  Stmt *Associated = nullptr;
  Transform::Kind TransKind = Transform::Kind::UnknownKind;
  unsigned NumClauses;

protected:
  explicit TransformExecutableDirective(SourceRange LocRange, Stmt *Associated,
                                        ArrayRef<TransformClause *> Clauses,
                                        Transform::Kind TransKind)
      : Stmt(Stmt::TransformExecutableDirectiveClass), LocRange(LocRange),
        Associated(Associated), TransKind(TransKind),
        NumClauses(Clauses.size()) {
    setClauses(Clauses);
  }
  explicit TransformExecutableDirective(unsigned NumClauses)
      : Stmt(Stmt::StmtClass::TransformExecutableDirectiveClass),
        NumClauses(NumClauses) {}

  size_t numTrailingObjects(OverloadToken<TransformClause *>) const {
    return NumClauses;
  }

public:
  static bool classof(const TransformExecutableDirective *T) { return true; }
  static bool classof(const Stmt *T) {
    return T->getStmtClass() == Stmt::TransformExecutableDirectiveClass;
  }

  static TransformExecutableDirective *
  create(ASTContext &Ctx, SourceRange Range, Stmt *Associated,
         ArrayRef<TransformClause *> Clauses, Transform::Kind TransKind);
  static TransformExecutableDirective *createEmpty(ASTContext &Ctx,
                                                   unsigned NumClauses);

  SourceRange getRange() const { return LocRange; }
  SourceLocation getBeginLoc() const { return LocRange.getBegin(); }
  SourceLocation getEndLoc() const { return LocRange.getEnd(); }
  void setRange(SourceRange Loc) { LocRange = Loc; }
  void setRange(SourceLocation BeginLoc, SourceLocation EndLoc) {
    LocRange = SourceRange(BeginLoc, EndLoc);
  }

  Stmt *getAssociated() const { return Associated; }
  void setAssociated(Stmt *S) { Associated = S; }

  Transform::Kind getTransformKind() const { return TransKind; }

  child_range children() { return child_range(&Associated, &Associated + 1); }
  const_child_range children() const {
    return const_child_range(&Associated, &Associated + 1);
  }

  unsigned getNumClauses() const { return NumClauses; }
  MutableArrayRef<TransformClause *> clauses() {
    return llvm::makeMutableArrayRef(getTrailingObjects<TransformClause *>(),
                                     NumClauses);
  }
  ArrayRef<TransformClause *> clauses() const {
    return llvm::makeArrayRef(getTrailingObjects<TransformClause *>(),
                              NumClauses);
  }

  void setClauses(llvm::ArrayRef<TransformClause *> List) {
    assert(List.size() == NumClauses);
    for (auto p : llvm::zip_first(List, clauses()))
      std::get<1>(p) = std::get<0>(p);
  }

  auto getClausesOfKind(TransformClause::Kind Kind) const {
    return llvm::make_filter_range(clauses(), [Kind](TransformClause *Clause) {
      return Clause->getKind() == Kind;
    });
  }

  template <typename SpecificClause> auto getClausesOfKind() const {
    return llvm::map_range(
        llvm::make_filter_range(clauses(),
                                [](TransformClause *Clause) {
                                  return isa<SpecificClause>(Clause);
                                }),
        [](TransformClause *Clause) { return cast<SpecificClause>(Clause); });
  }

  template <typename SpecificClause>
  SpecificClause *getFirstClauseOfKind() const {
    auto Range = getClausesOfKind<SpecificClause>();
    if (Range.begin() == Range.end())
      return nullptr;
    return *Range.begin();
  }
};

const Stmt *getAssociatedLoop(const Stmt *S);
} // namespace clang

#endif /* LLVM_CLANG_AST_STMTTRANSFROM_H */
