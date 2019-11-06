//===---- CGLoopInfo.cpp - LLVM CodeGen for loop metadata -*- C++ -*-------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CGLoopInfo.h"
#include "CGTransform.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"

using namespace clang::CodeGen;
using namespace llvm;

LoopInfo::LoopInfo(llvm::BasicBlock *Header, CGTransformedTree *TreeNode)
    : Header(Header) {
  if (TreeNode) {
    LoopMD = TreeNode->makeLoopID(Header->getContext(), false);
    AccGroup = TreeNode->getAccessGroupOrNull();
  }
}

LoopInfoStack::~LoopInfoStack() {
  for (auto N : AllNodes)
    delete N;
  for (auto T : AllTransforms)
    delete T;
}

void LoopInfoStack::initBuild(clang::ASTContext &ASTCtx,
                              llvm::LLVMContext &LLVMCtx, CGDebugInfo *DbgInfo,
                              clang::Stmt *Body) {
  CGTransformedTreeBuilder Builder(ASTCtx, LLVMCtx, AllNodes, AllTransforms,
                                   DbgInfo);
  TransformedStructure = Builder.computeTransformedStructure(Body, StmtToTree);
}

void LoopInfoStack::push(BasicBlock *Header, const clang::Stmt *LoopStmt) {
  auto Loop = getAssociatedLoop(LoopStmt);
  auto TreeNode = StmtToTree.lookup(Loop);
  Active.emplace_back(new LoopInfo(Header, TreeNode));
}

void LoopInfoStack::pop() {
  assert(!Active.empty() && "No active loops to pop");
  Active.pop_back();
}

void LoopInfoStack::InsertHelper(Instruction *I) const {
  if (I->mayReadOrWriteMemory()) {
    SmallVector<Metadata *, 4> AccessGroups;
    for (const auto &AL : Active) {
      // Here we assume that every loop that has an access group is parallel.
      if (MDNode *Group = AL->getAccessGroup())
        AccessGroups.push_back(Group);
    }
    MDNode *UnionMD = nullptr;
    if (AccessGroups.size() == 1)
      UnionMD = cast<MDNode>(AccessGroups[0]);
    else if (AccessGroups.size() >= 2)
      UnionMD = MDNode::get(I->getContext(), AccessGroups);
    I->setMetadata("llvm.access.group", UnionMD);
  }

  if (!hasInfo())
    return;

  const LoopInfo &L = getInfo();
  if (!L.getLoopID())
    return;

  if (I->isTerminator()) {
    for (BasicBlock *Succ : successors(I))
      if (Succ == L.getHeader()) {
        I->setMetadata(llvm::LLVMContext::MD_loop, L.getLoopID());
        break;
      }
    return;
  }
}
