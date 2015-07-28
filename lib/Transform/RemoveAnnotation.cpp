#include "anteater/Core/IRBuilder.h"

#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/Instructions.h>

namespace {

using namespace llvm;

class RemoveAnnotation : public ModulePass {
 public:
  RemoveAnnotation();
  const char * getPassName() const { return "Remove Annotation from Anteater"; }
  bool runOnModule(Module & M);
  static char ID;
};
 
RemoveAnnotation::RemoveAnnotation()
    : ModulePass(ID)
{}

char RemoveAnnotation::ID = 0;

bool RemoveAnnotation::runOnModule(Module & M) {
  Function * f = M.getFunction(anteater::IRBuilder::kRecordTransformationFunctionName);
  if (!f)
    return false;

  Value::use_iterator it = f->use_begin();
  while (it != f->use_end()) {
    Value::use_iterator old_it = it++;
    cast<Instruction>(*old_it)->eraseFromParent();
  }

  return true;
}

RegisterPass<RemoveAnnotation> X("remove-annotation", "Remove annotation from Anteater",
                      false /* Only looks at CFG */,
                      false /* Analysis Pass */);
}
