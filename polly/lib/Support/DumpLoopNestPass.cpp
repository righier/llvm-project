//===------ DumpLoopNestPass.cpp --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "polly/Support/DumpLoopNestPass.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/ToolOutputFile.h"

#define DEBUG_TYPE "polly-dump-loopnest"

using namespace llvm;
using namespace polly;

namespace {

static void runDumpLoopnest(llvm::Module &M, StringRef Filename, bool IsSuffix) {
  std::string Dumpfile;
  if (IsSuffix) {
    StringRef ModuleName = M.getName();
    StringRef Stem = sys::path::stem(ModuleName);
    Dumpfile = (Twine(Stem) + Filename + ".ll").str();
  } else {
    Dumpfile = Filename.str();
  }
  LLVM_DEBUG(dbgs() << "Dumping module to " << Dumpfile << '\n');

  std::unique_ptr<ToolOutputFile> Out;
  std::error_code EC;
  Out.reset(new ToolOutputFile(Dumpfile, EC, sys::fs::OF_None));
  if (EC) {
    errs() << EC.message() << '\n';
    return;
  }

  M.print(Out->os(), nullptr);
  Out->keep();
}

class DumpLoopnestWrapperPass : public ModulePass {
private:
  DumpLoopnestWrapperPass(const DumpModuleWrapperPass &) = delete;
  const DumpLoopnestWrapperPass &
  operator=(const DumpLoopnestWrapperPass &) = delete;

  std::string Filename;
  bool IsSuffix;

public:
  static char ID;

  /// This constructor is used e.g. if using opt -polly-dump-module.
  ///
  /// Provide a default suffix to not overwrite the original file.
  explicit DumpLoopnestWrapperPass()
      : ModulePass(ID), Filename("-dump"), IsSuffix(true) {}

  explicit DumpLoopnestWrapperPass(std::string Filename, bool IsSuffix)
      : ModulePass(ID), Filename(std::move(Filename)), IsSuffix(IsSuffix) {}

  /// @name ModulePass interface
  ///@{
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

  virtual bool runOnModule(llvm::Module &M) override {
    runDumpModule(M, Filename, IsSuffix);
    return false;
  }
  ///@}
};

char DumpModuleWrapperPass::ID;
} // namespace

ModulePass *polly::createDumpModuleWrapperPass(std::string Filename,
                                               bool IsSuffix) {
  return new DumpModuleWrapperPass(std::move(Filename), IsSuffix);
}

llvm::PreservedAnalyses DumpModulePass::run(llvm::Module &M,
                                            llvm::ModuleAnalysisManager &AM) {
  runDumpModule(M, Filename, IsSuffix);
  return PreservedAnalyses::all();
}



INITIALIZE_PASS_BEGIN(DumpLoopnestWrapperPass, "polly-dump-loopnest", "Polly - Dump Loop Nest", false, false)
INITIALIZE_PASS_END(DumpLoopnestWrapperPass,   "polly-dump-loopnest", "Polly - Dump Loop Nest", false, false)
