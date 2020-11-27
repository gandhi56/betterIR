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

  // determine the dfs order of basic blocks in this function
  std::vector<const BasicBlock*> workVec;
  for (const BasicBlock* bb : depth_first(&fn.getEntryBlock()))
    workVec.push_back(bb);

  while (!workVec.empty()){
    const BasicBlock* bb = workVec.back(); workVec.pop_back();
    getTermSuccInstr(bb);
    computeUsesDefs(bb);
    for (const BasicBlock* pred : predecessors(bb))
      workVec.push_back(pred);
  }

  // output code and live variables
  printLiveVariables(fn);

  return llvm::PreservedAnalyses::all();
}

void LivenessAnalysis::printLiveVariables(Function& fn){
  for (auto & b : fn){
    const BasicBlock* bb = &b;
    for (auto i = bb->begin(); i != bb->end(); ++i){
      const Instruction* instr = &*i;
      if (i == bb->begin()){
        errs() << "Live : ";
        for (auto var : liveIn[instr])
          errs() << *var << ' ';
        errs() << '\n';
      }
      errs() << '\n' << *instr << "\n\n";
      errs() << "Live : ";
      for (auto var : liveOut[instr])
        errs() << *var << ' ';
      errs() << '\n';
      if (instr->isTerminator())  errs() << '\n';
    }
  }
}

void LivenessAnalysis::getTermSuccInstr(const BasicBlock* bb){
  // set liveOut for the terminator instruction of this basic block
  for (const BasicBlock* succ : successors(bb))
    for (const Value* v : liveIn[&(*(succ->begin()))])
      liveOut[bb->getTerminator()].insert(v);
}

void LivenessAnalysis::computeUsesDefs(const BasicBlock* bb){
  // iterate over each instruction to compute uses and defs
  for (auto i = bb->rbegin(); i != bb->rend(); ++i){
    const Instruction* instr = &(*i);
    DenseSet<const Value*> uses, defs;

    // liveOut[terminator instruction] is the liveIn of the following instruction
    if (i != bb->rbegin()){
      auto tmp  = i;
      liveOut[instr] = liveIn[&*(--tmp)];
    }

    // if store instruction, the second operand is defined,
    //  store the operand in the defs set
    // otherwise store the instruction into the set
    const Value* lhsValue = (isa<StoreInst>(*instr) ? instr->getOperand(1) : dyn_cast<Value>(instr));
    defs.insert(lhsValue);

    // if store instruction, the first operand is used,
    //  store the operand in the uses set
    // otherwise store the operands of the instruction into the set
    //  if they are valid definitions of an instruction
    if (isa<StoreInst>(*instr)){
      auto* storeInstr = dyn_cast<StoreInst>(instr);
      errs() << "STORE USE " << *storeInstr->getOperand(0) << '\n';
      if (!isa<ConstantInt>(storeInstr->getOperand(0)))  uses.insert(storeInstr->getOperand(0));
    }
    else{
      /// FIXME: global variables, other instructions
      for (auto& op : instr->operands())
        if (isa<Instruction>(&op))  uses.insert(op);
    }

    // if a variable is live at the exit point of the instruction,
    //  then it is potentially live at the entry point of the instruction
    liveIn[instr] = liveOut[instr];

    //
//    if (!uses.empty() and (lhsValue->getName().empty() or liveOut[instr].find(lhsValue) != liveOut[instr].end())){
    if (!uses.empty()){
//      LLVM_DEBUG(dbgs() << "Uses set: ");
      for (auto var : uses){
//        LLVM_DEBUG(dbgs() << *var << " ");
        liveIn[instr].insert(var);
      }
//      LLVM_DEBUG(dbgs() << "\n");
    }

    // if this instruction kills variables that are live at the exit
    //  point of the instruction, then remove it from the liveIn set
    if (!defs.empty()){
//      LLVM_DEBUG(dbgs() << "Defs set (erase): ");
      for (auto var : defs){
//        LLVM_DEBUG(dbgs() << *var << " ");
        errs() << "deleting " << *var << " from liveIn of " << *instr << '\n';
        liveIn[instr].erase(var);
      }
//      LLVM_DEBUG(dbgs() << "\n");
    }

    // clear sets
    defs.clear();
    uses.clear();
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