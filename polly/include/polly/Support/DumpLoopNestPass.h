//===------ DumpLoopNestPass.h ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef POLLY_SUPPORT_DUMPLOOPNESTPASS_H
#define POLLY_SUPPORT_DUMPLOOPNESTPASS_H

#include "llvm/IR/PassManager.h"
#include <string>
#include "polly/ScopPass.h"



namespace llvm {
  class Pass;
  class PassRegistry;
} // namespace llvm





namespace polly {
  llvm::Pass  *createDumpLoopnestWrapperPass(std::string Filename ,bool IsSuffix  );

/// A pass that prints the module into a file.
struct DumpLoopnestPass : llvm::PassInfoMixin<DumpLoopnestPass> {
  std::string Filename;
  bool IsSuffix;

  DumpLoopnestPass(std::string Filename, bool IsSuffix) : Filename(std::move(Filename)), IsSuffix(IsSuffix) {}

  llvm::PreservedAnalyses run(Scop &S, ScopAnalysisManager &SAM, ScopStandardAnalysisResults &SAR, SPMUpdater &U);
};
} // namespace polly




namespace llvm {
class PassRegistry;
void initializeDumpLoopnestWrapperPassPass(llvm::PassRegistry &);
} // namespace llvm




#endif /* POLLY_SUPPORT_DUMPLOOPNESTPASS_H */
