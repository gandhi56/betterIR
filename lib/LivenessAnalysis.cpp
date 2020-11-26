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

llvm::PreservedAnalyses
LivenessAnalysis::run(llvm::Function& fn, llvm::FunctionAnalysisManager&){

  // compute gen and kill sets for each basic block
  for (BasicBlock& bb : fn){
    computeGenKillVariables(&bb);
  }

  // compute liveIn and liveOut sets for each basic block
  bool changed;
  do{
    changed = false;

  } while (changed);

  return llvm::PreservedAnalyses::all();
}

void LivenessAnalysis::computeGenKillVariables(BasicBlock *bb){

  for (auto& inst : *bb){
    const Instruction* cInst = &inst;

    // store killed variable from cInst
    if (isa<StoreInst>(*cInst)){
      // if store instruction, the second operand is killed
      auto* storeInst = dyn_cast<StoreInst>(cInst);
      kill[bb].insert(storeInst->getOperand(1));
    }
    else{
      // otherwise the def of the instruction is killed
      kill[bb].insert(dyn_cast<Value>(cInst));
    }

    // store generated variable from cInst
    if (isa<StoreInst>(*cInst)){
      // record the operand into the gen set iff the operand is not a constant
      auto* var = cInst->getOperand(0);
      const Constant* constValue = dyn_cast<Constant>(var);
      if (!constValue){
        gen[bb].insert(var);
      }
    }
    else{
      // record all operands of the instruction in question
      for (size_t i = 0; i < cInst->getNumOperands(); ++i){
        gen[bb].insert(dyn_cast<Instruction>(cInst->getOperand(i)));

      }
    }
  }

  LLVM_DEBUG(dbgs() << "Kill set for basic block" << bb->getName() << '\n');
  for (auto& var : kill[bb]){
    LLVM_DEBUG(dbgs() << var << " ");
  }
  LLVM_DEBUG(dbgs() << "\n\n");

  LLVM_DEBUG(dbgs() << "Gen set for basic block" << bb->getName() << '\n');
  for (auto& var : gen[bb]){
    LLVM_DEBUG(dbgs() << var << " ");
  }
  LLVM_DEBUG(dbgs() << '\n');

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