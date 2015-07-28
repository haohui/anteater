#include "anteater/Core/IRBuilder.h"

#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>

namespace {

using namespace llvm;

class EliminateRedundantPrecondition : public ModulePass {
 public:
  EliminateRedundantPrecondition();
  const char * getPassName() const { return "redundant precondition elimination"; }
  bool runOnModule(Module & M);
  static char ID;
};

EliminateRedundantPrecondition::EliminateRedundantPrecondition()
    : ModulePass(ID)
{}

char EliminateRedundantPrecondition::ID = 0;

bool EliminateRedundantPrecondition::runOnModule(Module & M) {
  Function * precondition_function = M.getFunction(anteater::IRBuilder::kPreconditionFunctionName);
  if (!precondition_function)
    return false;

  bool changed = false;
  // If we ended up with trivial precondition, remove it
  Value::use_iterator it = precondition_function->use_begin();
  while (it != precondition_function->use_end()) {
    assert (isa<CallInst>(*it));
    CallInst * CI = cast<CallInst>(*it);
    if (CI->getArgOperand(0) == ConstantInt::getTrue(M.getContext())) {
      changed = true;
      ++it;
      CI->eraseFromParent();
    } else {
      ++it;
    }
  }

  return changed;
}

RegisterPass<EliminateRedundantPrecondition> X("sym-pkt-rpe", "Eliminate Redundant Precondition",
                                               false /* Only looks at CFG */,
                                               false /* Analysis Pass */);
}
