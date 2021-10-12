//===------ DumpLoopNestPass.cpp --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "polly/Support/DumpLoopNestPass.h"
#include "polly/ScheduleTreeTransform.h"
#include "polly/Support/GICHelper.h"
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
static void loopToJson(const isl::schedule_node &Node, BandAttr *ParentAttr,
                       std::vector<json::Value> &Subloops, bool &SingleLoop);

static void iterateChildren(const isl::schedule_node &Node,
                            BandAttr *ParentAttr,
                            std::vector<json::Value> &Subloops,
                            bool &SingleLoop) {
  SingleLoop = false;
  if (Node.has_children()) {
    auto C = Node.first_child();
    loopToJson(C, ParentAttr, Subloops, SingleLoop);
    while (C.has_next_sibling()) {
      C = C.next_sibling();
      SingleLoop = false;
      bool Dummy = false;
      loopToJson(C, ParentAttr, Subloops, Dummy);
    }
  }
}

static void assignFromLoc(json::Object &Obj, DebugLoc Loc, Function *Func) {
  if (Func)
    Obj["function"] = Func->getName().str();

  if (!Loc)
    return;

  Obj["filename"] = Loc->getFilename();
  Obj["directory"] = Loc->getDirectory();
  Obj["path"] = (Twine(Loc->getDirectory()) + llvm::sys::path::get_separator() +
                 Loc->getFilename())
                    .str();
  if (Loc->getSource())
    Obj["source"] = Loc->getSource().getValue().str();
  Obj["line"] = Loc->getLine();
  Obj["column"] = Loc->getColumn();
}

static void loopToJson(const isl::schedule_node &Node, BandAttr *ParentAttr,
                       std::vector<json::Value> &Subloops, bool &SingleLoop) {
  if (isBand(Node)) {
    assert(ParentAttr);

    auto *L = ParentAttr->OriginalLoop;
    assert(L);
    auto Header = L->getHeader();
    auto LoopId = L->getLoopID();
    auto BeginLoc = (LoopId && LoopId->getNumOperands() > 1)
                        ? LoopId->getOperand(1).get()
                        : nullptr;
    DebugLoc Start = dyn_cast_or_null<DILocation>(BeginLoc);

    json::Object Loop;
    Loop["kind"] = "loop";
    assignFromLoc(Loop, Start, Header->getParent());

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
    bool SubSingleLoop = false;
    iterateChildren(Node, nullptr, Substmts, SubSingleLoop);

    if (Substmts.empty()) {
      // Loop["subloops"] = json::Array();
    } else {
      Loop["children"] = json::Value(std::move(Substmts));
      Loop["perfectnest"] = SubSingleLoop;
    }

    Subloops.push_back(std::move(Loop));
    SingleLoop = true;
  } else if (isBandMark(Node)) {
    assert(!ParentAttr);
    ParentAttr = getBandAttr(Node);
    iterateChildren(Node, ParentAttr, Subloops, SingleLoop);
  } else if (isLeaf(Node)) {
    assert(Node.n_children() == 0);

    json::Object JStmt;
    JStmt["kind"] = "stmt";

    auto Dom = Node.get_domain();

    DebugLoc Loc;
    ScopStmt *Stmt = nullptr;
    Dom.foreach_set([&](isl::set Set) -> isl::stat {
      Stmt = reinterpret_cast<ScopStmt *>(Set.get_tuple_id().get_user());
      for (auto I : Stmt->getInstructions()) {
        Loc = I->getDebugLoc();
        if (Loc)
          return isl::stat::error();
      }

      auto BB = Stmt->getBasicBlock();
      for (auto &I : *BB) {
        Loc = I.getDebugLoc();
        if (Loc)
          return isl::stat::error();
      }

      return isl::stat::ok();
    });
    assignFromLoc(JStmt, Loc,
                  Stmt ? &Stmt->getParent()->getFunction() : nullptr);

    Subloops.push_back(std::move(JStmt));
    SingleLoop = false;
  } else {
    // Has to insert something
    iterateChildren(Node, ParentAttr, Subloops, SingleLoop);
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
  } else {
    LLVM_DEBUG(dbgs() << "Dumping loopnest to " << Dumpfile << '\n');
  }
  auto &Loopnests = Cache[Dumpfile];

  auto Sched = S.getScheduleTree();
  bool Dummy;
  std::vector<json::Value> ToplevelLoops;
  loopToJson(Sched.get_root(), nullptr, ToplevelLoops, Dummy);

  json::Array TL(std::move(ToplevelLoops));
  json::Object Scop;
  Scop["kind"] = "scop";
  Scop["function"] = S.getFunction().getName();
  Scop["children"] = json::Value(std::move(TL));
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
    Output["scops"] = std::move(A);
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
      errs() << "  error closing file after writing!\n";
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
  bool IsSuffix;
  LoopnestCacheTy Cache;

public:
  static char ID;

  explicit DumpLoopnestWrapperPass() : ScopPass(ID), IsSuffix(true) {}

  explicit DumpLoopnestWrapperPass(std::string Filename, bool IsSuffix)
      : ScopPass(ID), Filename(std::move(Filename)), IsSuffix(IsSuffix) {}

  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.addRequired<ScopInfoRegionPass>();
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

INITIALIZE_PASS_BEGIN(DumpLoopnestWrapperPass, "polly-loopnest-dumper",
                      "Polly - Dump Loop Nest", false, false)
INITIALIZE_PASS_END(DumpLoopnestWrapperPass, "polly-loopnest-dumper",
                    "Polly - Dump Loop Nest", false, false)
