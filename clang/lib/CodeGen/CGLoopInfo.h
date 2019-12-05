//===---- CGLoopInfo.h - LLVM CodeGen for loop metadata -*- C++ -*---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the internal state used for llvm translation for loop statement
// metadata.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_CODEGEN_CGLOOPINFO_H
#define LLVM_CLANG_LIB_CODEGEN_CGLOOPINFO_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace llvm {
class BasicBlock;
class Instruction;
class MDNode;
class LLVMContext;
} // end namespace llvm

namespace clang {
class ASTContext;
class Stmt;
class Transform;

namespace CodeGen {
class CGTransformedTree;
class CGDebugInfo;

/// Information used when generating a structured loop.
class LoopInfo {
public:
  /// Construct a new LoopInfo for the loop with entry Header.
  LoopInfo(llvm::BasicBlock *Header, CGTransformedTree *TreeNode);

  /// Get the loop id metadata for this loop.
  llvm::MDNode *getLoopID() const { return LoopMD; }

  /// Get the header block of this loop.
  llvm::BasicBlock *getHeader() const { return Header; }


  /// Return this loop's access group or nullptr if it does not have one.
  llvm::MDNode *getAccessGroup() const { return AccGroup; }


private:
  /// The metadata node containing this loop's properties. It is assigned to the
  /// terminators of all loop latches.
  llvm::MDNode *LoopMD = nullptr;

  /// Header block of this loop.
  llvm::BasicBlock *Header;

  /// The metadata node to be assigned to all memory accesses within the loop.
  llvm::MDNode *AccGroup = nullptr;
};

/// A stack of loop information corresponding to loop nesting levels.
/// This stack can be used to prepare attributes which are applied when a loop
/// is emitted.
class LoopInfoStack {
  LoopInfoStack(const LoopInfoStack &) = delete;
  void operator=(const LoopInfoStack &) = delete;

public:
  LoopInfoStack() {}
  ~LoopInfoStack();

  CGTransformedTree* lookupTransformedNode(const Stmt *S);
  static  CGTransformedTree* getFollowupAtIdx(CGTransformedTree* TN, int FollowupIdx);

  void initAsOutlined(LoopInfoStack &ParentLIS) {
    StmtToTree = ParentLIS.StmtToTree;
  }

  void initBuild(ASTContext &ASTCtx, llvm::LLVMContext &LLVMCtx,
                 CGDebugInfo *DbgInfo, Stmt *Body);

  /// Begin a new structured loop.
  void push(llvm::BasicBlock *Header, const Stmt *LoopStmt);

  /// End the current loop.
  void pop();

  /// Function called by the CodeGenFunction when an instruction is
  /// created.
  void InsertHelper(llvm::Instruction *I) const;

private:
  /// Returns true if there is LoopInfo on the stack.
  bool hasInfo() const { return !Active.empty(); }
  /// Return the LoopInfo for the current loop. HasInfo should be called
  /// first to ensure LoopInfo is present.
  const LoopInfo &getInfo() const { return *Active.back(); }
  /// Stack of active loops.
  llvm::SmallVector<std::unique_ptr<LoopInfo>, 4> Active;

  llvm::SmallVector<CGTransformedTree *, 64> AllNodes;
  llvm::SmallVector<Transform *, 64> AllTransforms;
  llvm::DenseMap<Stmt *, CGTransformedTree *> StmtToTree;

  CGTransformedTree *TransformedStructure = nullptr;
};

} // end namespace CodeGen
} // end namespace clang

#endif
