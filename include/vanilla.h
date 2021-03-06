//
// Created by anshil on 2020-11-14.
//

#ifndef BETTERIR_LIB_VANILLA_H
#define BETTERIR_LIB_VANILLA_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

struct Vanilla: public llvm::PassInfoMixin<Vanilla>{
  llvm::PreservedAnalyses run(llvm::Function& fn,
                              llvm::FunctionAnalysisManager&);
};

#endif //BETTERIR_LIB_VANILLA_H
