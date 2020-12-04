//
// Created by anshil on 2020-11-26.
//

#include "LivenessAnalysis.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define PASS_NAME "liveness"
#define DEBUG_TYPE "liveness"

#define debug_def_sets
#define debug_use_sets

PreservedAnalyses
LivenessAnalysis::run(llvm::Function& fn, llvm::FunctionAnalysisManager&){

  computeGenKillVariables(&fn);

  // compute liveIn and liveOut sets for each basic block
  bool changed = true;
  int iter = 0;
  while (changed) {
    errs() << "iteration " << iter++ << "...\n";

    // track whether this iteration changed the liveIn set or not
    changed = false;

    // iterate over each basic block
    for (BasicBlock& bb : fn){

      // compute liveOut set for the current basic block
      VarSet tmpSet = liveOut[&bb];
      liveOut[&bb].clear();
      for (BasicBlock* nextBB : successors(&bb)){
        for (auto& liveVar : liveIn[nextBB]){
          liveOut[&bb].insert(liveVar);
        }
      }
      if (tmpSet != liveOut[&bb]) changed = true;

      // compute liveIn set for the current basic block
      tmpSet = liveIn[&bb];
      liveIn[&bb].clear();
      liveIn[&bb] = use[&bb];
      for (auto& var : liveOut[&bb]){
        if (def[&bb].find(var) == std::end(def[&bb])){
          liveIn[&bb].insert(var);
        }
      }

      // check if the liveIn set has reached a fixed point
      if (liveIn[&bb] != tmpSet)  changed = true;
    }
  }

  for (auto& bb : fn){
    errs() << "Live in set for basic block " << bb.getName() << " with #instructions = " << bb.getInstList().size() << '\n';
    debugPrintVarSet(liveIn[&bb]);
    errs() << '\n';

    errs() << "\nLive out set for basic block " << bb.getName() << '\n';
    debugPrintVarSet(liveOut[&bb]);
    errs() << "\n---------------------------------\n";
  }
  return llvm::PreservedAnalyses::all();
}

void LivenessAnalysis::computeGenKillVariables(Function *fn) {
  std::vector<BasicBlock *> workList;
  for (auto &bb : depth_first(&fn->getEntryBlock())) {
    workList.push_back(bb);
  }

  for (auto bb : workList) {
    for (auto &inst : *bb) {
      const Instruction *cInst = &inst;

      if (isa<StoreInst>(cInst)) {
        // a new variable was killed
        def[bb].insert(cInst->getOperand(1));
      } else if (isa<LoadInst>(cInst)) {
        // insert each operand into the use set of this basic block
        for (auto &op : cInst->operands()) {
          use[bb].insert(op);
        }
      }
    }

#ifdef debug_def_sets
      LLVM_DEBUG(dbgs() << "Def set for basic block " << bb->getName() << '\n');
      debugPrintVarSet(def[bb]);
      LLVM_DEBUG(dbgs() << "\n");
#endif

#ifdef debug_use_sets
      LLVM_DEBUG(dbgs() << "Use set for basic block " << bb->getName() << '\n');
      debugPrintVarSet(use[bb]);
      LLVM_DEBUG(dbgs() << "--------------------------------------\n");
#endif
    errs() << "// ======================================== //\n";
  }
}

void LivenessAnalysis::debugPrintVarSet(LivenessAnalysis::VarSet& s){
  for (auto& var : s){
    errs() << *var << "\n";
  }
}

// --------------------------------------------------------------------------------
// Pass Manager registration
// --------------------------------------------------------------------------------

llvm::PassPluginLibraryInfo getLivenessPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, PASS_NAME, LLVM_VERSION_STRING,
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "liveness") {
                    FPM.addPass(LivenessAnalysis());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLivenessPluginInfo();
}