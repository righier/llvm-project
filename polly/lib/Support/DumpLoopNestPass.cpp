//===------ DumpLoopNestPass.cpp --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "polly/Support/DumpLoopNestPass.h"
#include "polly/ScheduleTreeTransform.h"
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
  static void loopToJson(const isl::schedule_node& Node, BandAttr* ParentAttr,
    std::vector<json::Value>& Subloops);

  static void iterateChildren(const isl::schedule_node &Node, BandAttr *ParentAttr,    std::vector<json::Value> &Subloops) {
    if (Node.has_children()) {
      auto C = Node.first_child();
      while (true) {
        loopToJson(C, ParentAttr, Subloops);
        if (!C.has_next_sibling())
          break;
        C = C.next_sibling();
      }
    }
  }

static void loopToJson(const isl::schedule_node &Node, BandAttr *ParentAttr,
                       std::vector<json::Value> &Subloops) {
  if (isBand(Node)) {
    assert(ParentAttr);

    auto *L = ParentAttr->OriginalLoop;
    assert(L);
    auto Header = L->getHeader();
    auto LoopId = L->getLoopID();
    auto BeginLoc = (LoopId && LoopId->getNumOperands() > 1)
                        ? LoopId->getOperand(1).get()
                        : nullptr;
    auto Start = dyn_cast_or_null<DILocation>(BeginLoc);

    json::Object Loop;
    if (Start) {
      Loop["filename"] = Start->getFilename();
      Loop["directory"] = Start->getDirectory();
      Loop["path"] = (Twine(Start->getDirectory()) +
                      llvm::sys::path::get_separator() + Start->getFilename())
                         .str();
      if (Start->getSource())
        Loop["source"] = Start->getSource().getValue().str();
      Loop["line"] = Start->getLine();
      Loop["column"] = Start->getColumn();
    }

    Loop["function"] = Header->getParent()->getName().str();
    {
      SmallVector<char, 255> Buf;
      raw_svector_ostream OS(Buf);
      Header->printAsOperand(OS, /*PrintType=*/false);
      Loop["header"] = Buf;
    }
#if 0
    {
      SmallVector<char, 255> Buf;
      raw_svector_ostream OS(Buf);
      RN->getEntry()->printAsOperand(OS, /*PrintType=*/false);
      Loop["entry"] = Buf;
    }
#endif
#if 0
    {
      BasicBlock *BBExit = RN->getEntry();
      if (RN->isSubRegion()) {
        auto *LocalRegion = RN->getNodeAs<Region>();
        BBExit = LocalRegion->getExit();
      }

      SmallVector<char, 255> Buf;
      raw_svector_ostream OS(Buf);
      BBExit->printAsOperand(OS, /*PrintType=*/false);
      Loop["exit"] = Buf;
    }
#endif

    std::vector<json::Value> Substmts;
    iterateChildren(Node,nullptr,Substmts);


  
    if (Substmts.empty()) {
      //Loop["subloops"] = json::Array();
    } else {
      Loop["subloops"] = json::Value(std::move(Substmts));
      Loop["perfectnest"] = Substmts.size() == 1;
    }

    Subloops.push_back(std::move(Loop));
  } else if (isBandMark(Node)) {
    assert(!ParentAttr);
    ParentAttr = getBandAttr(Node);
    iterateChildren(Node,ParentAttr,Subloops);
  }
  else if (isLeaf(Node)) {
    assert(Node.n_children()==0);
    json::Value Stmt{"Stmt"};
    Subloops.push_back(std::move(Stmt));
  } else {
    // Has to insert something
    iterateChildren(Node,ParentAttr,Subloops);
  }
}

static void runDumpLoopnest(Scop &S, LoopnestCacheTy &Cache, StringRef Filename,
                            bool IsSuffix) {
  std::string Dumpfile;
  if (IsSuffix) {
    auto *M = S.getFunction().getParent();
    StringRef ModuleName = M->getName();
    StringRef Stem = sys::path::stem(ModuleName);
    Dumpfile = (Twine(Stem) + Filename + ".json").str();
  } else {
    Dumpfile = Filename.str();
  }
 
  if (Cache.count(Dumpfile)) {
    LLVM_DEBUG(dbgs() << "Adding loopnest to " << Dumpfile << '\n');
  }
  else {
    LLVM_DEBUG(dbgs() << "Dumping loopnest to " << Dumpfile << '\n');
  }
  auto &Loopnests = Cache[Dumpfile];


  auto Sched = S.getScheduleTree();
  std::vector<json::Value> ToplevelLoops;
  loopToJson(Sched.get_root(), nullptr, ToplevelLoops);

  json::Array TL(std::move(ToplevelLoops));
  json::Object Scop;
  Scop["toplevel"] =  json::Value(std::move(TL));
  Loopnests.push_back(std::move(Scop));
}

static void saveLoopnestCache(LoopnestCacheTy &Cache) {
  for (auto &P : Cache) {
    auto Dumpfile = P.first();
    auto Loopnests = P.second;
    LLVM_DEBUG(dbgs() << "Writing loopnest to " << Dumpfile << '\n');

    json::Array A;
    for (auto X : Loopnests) {
      A.push_back(X);
    }

    json::Object Output;
    Output["loopnests"] = std::move(A);
    json::Value V = json::Value(std::move(Output));

    errs() << "Writing LoopNest to '" << Dumpfile << "'.\n";
    std::error_code EC;
    ToolOutputFile F(Dumpfile, EC, llvm::sys::fs::OF_TextWithCRLF);
    auto &OS = F.os();
    if (EC) {
      errs() << "  error opening file for writing!\n";
      F.os().clear_error();
      return;
    }

    OS << formatv("{0:3}", V);
    OS.close();
    if (OS.has_error()) {
      errs() << "  error opening file for writing!\n";
      OS.clear_error();
      return;
    }

    F.keep();
  }
}

class DumpLoopnestWrapperPass : public ScopPass {
private:
  DumpLoopnestWrapperPass(const DumpLoopnestWrapperPass &) = delete;
  const DumpLoopnestWrapperPass &
  operator=(const DumpLoopnestWrapperPass &) = delete;

  std::string Filename;
  bool IsSuffix = false;
  LoopnestCacheTy Cache;

public:
  static char ID;

  explicit DumpLoopnestWrapperPass() : ScopPass(ID) {}

  explicit DumpLoopnestWrapperPass(std::string Filename, bool IsSuffix)
      : ScopPass(ID), Filename(std::move(Filename)), IsSuffix(false) {}

  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

  bool runOnScop(Scop &S) override {
    runDumpLoopnest(S, Cache, Filename, IsSuffix);
    return false;
  }

  ~DumpLoopnestWrapperPass() { saveLoopnestCache(Cache); }
};

char DumpLoopnestWrapperPass::ID;
} // namespace

Pass *polly::createDumpLoopnestWrapperPass(std::string Filename,
                                           bool IsSuffix) {
  return new DumpLoopnestWrapperPass(std::move(Filename), IsSuffix);
}

llvm::PreservedAnalyses DumpLoopnestPass::run(Scop &S, ScopAnalysisManager &SAM,
                                              ScopStandardAnalysisResults &SAR,
                                              SPMUpdater &U) {
  runDumpLoopnest(S, Cache, Filename, IsSuffix);
  return PreservedAnalyses::all();
}

DumpLoopnestPass::~DumpLoopnestPass() { saveLoopnestCache(Cache); }

INITIALIZE_PASS_BEGIN(DumpLoopnestWrapperPass, "polly-dump-loopnest",
                      "Polly - Dump Loop Nest", false, false)
INITIALIZE_PASS_END(DumpLoopnestWrapperPass, "polly-dump-loopnest",
                    "Polly - Dump Loop Nest", false, false)
