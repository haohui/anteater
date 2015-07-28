//===- InstructionNamer.cpp - Give anonymous instructions names -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is a little utility pass that gives instructions names to make SMT solver
// happy. Derived from LLVM
//
//===----------------------------------------------------------------------===//

#include "anteater/config.h"

#include "llvm/Transforms/Scalar.h"
#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Type.h"

using namespace llvm;

namespace {

struct AnteaterInstNamer : public FunctionPass {
  static char ID;
  explicit AnteaterInstNamer() : FunctionPass(ID) {
    initializeInstNamerPass(*PassRegistry::getPassRegistry());
  }
    
  void getAnalysisUsage(AnalysisUsage &Info) const {
    Info.setPreservesAll();
  }

  bool runOnFunction(Function &F) {
    for (Function::arg_iterator AI = F.arg_begin(), AE = F.arg_end();
         AI != AE; ++AI) {
      if (!AI->hasName() && !AI->getType()->isVoidTy())
        AI->setName("arg");
    }

    for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB) {
      if (!BB->hasName())
        BB->setName("bb");
        
      for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I)
        if ((!I->hasName() || I->getName().startswith(".")) && !I->getType()->isVoidTy())
          I->setName("t");
    }
    return true;
  }
};
  
char AnteaterInstNamer::ID = 0;

RegisterPass<AnteaterInstNamer> X("anteater-instnamer", "Assign names to anonymous instructions", false, false);

}


NAMESPACE_ANTEATER_BEGIN

Pass * createAnteaterInstructionNamerPass() {
  return new AnteaterInstNamer();
}

NAMESPACE_ANTEATER_END
