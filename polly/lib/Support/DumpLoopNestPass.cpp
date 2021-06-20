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


  /// Cache to not require reloading file for each SCoP in the same TU.
  static llvm::json::Array *LoopNests = nullptr;

static void loopToJson(const isl::schedule_node &Node ) {

}

static void runDumpLoopnest(Scop &S, StringRef Filename, bool IsSuffix) {
  std::string Dumpfile;
  if (IsSuffix) {
    auto *M = S.getFunction().getParent();
    StringRef ModuleName = M->getName();
    StringRef Stem = sys::path::stem(ModuleName);
    Dumpfile = (Twine(Stem) + Filename + ".json").str();
  } else {
    Dumpfile = Filename.str();
  }
  LLVM_DEBUG(dbgs() << "Dumping loopnest to " << Dumpfile << '\n');

  std::unique_ptr<ToolOutputFile> Out;
  std::error_code EC;
  Out.reset(new ToolOutputFile(Dumpfile, EC, sys::fs::OF_None));
  if (EC) {
    errs() << EC.message() << '\n';
    return;
  }


  auto& Sched = S.getSchedule();




  Out->keep();
}

class DumpLoopnestWrapperPass : public ScopPass {
private:
  DumpLoopnestWrapperPass(const DumpLoopnestWrapperPass &) = delete;
  const DumpLoopnestWrapperPass &
  operator=(const DumpLoopnestWrapperPass &) = delete;

  std::string Filename;


public:
  static char ID;


  explicit DumpLoopnestWrapperPass()
      : ScopPass(ID), Filename("-dump"){}

  explicit DumpLoopnestWrapperPass(std::string Filename)
      : ScopPass(ID), Filename(std::move(Filename)) {}


  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }


  bool runOnScop(Scop &S) override {
    runDumpLoopnest(S, Filename);
    return false;
  }

};


char DumpLoopnestWrapperPass::ID;
} // namespace

Pass *polly::createDumpLoopnestWrapperPass(std::string Filename, bool IsSuffix) {
  return new DumpLoopnestWrapperPass(std::move(Filename),IsSuffix);
}


llvm::PreservedAnalyses run(Scop &S, ScopAnalysisManager &SAM, ScopStandardAnalysisResults &SAR, SPMUpdater &U) {
  runDumpLoopnest(S);
  return PreservedAnalyses::all();
}



INITIALIZE_PASS_BEGIN(DumpLoopnestWrapperPass, "polly-dump-loopnest", "Polly - Dump Loop Nest", false, false)
INITIALIZE_PASS_END(DumpLoopnestWrapperPass,   "polly-dump-loopnest", "Polly - Dump Loop Nest", false, false)
