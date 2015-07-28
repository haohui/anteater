/// Replace the aggregate type of packets into scalars

#include <llvm/Pass.h>
#include <llvm/Module.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Instructions.h>
#include <llvm/Constants.h>

#include <cstdio>

namespace {

using namespace llvm;

class PacketScalarReplacement : public ModulePass {
 public:
  PacketScalarReplacement();
  const char * getPassName() const { return "Packet Scalar Replacement"; }
  bool runOnModule(Module & M);
  static char ID;
 private:
  void replace(Module * M, GlobalVariable * GV);
  void generateScalars(Module * m, GlobalVariable * GV, std::vector<Value *> * scalars);

};

PacketScalarReplacement::PacketScalarReplacement()
    : ModulePass(ID)
{}

char PacketScalarReplacement::ID = 0;

bool PacketScalarReplacement::runOnModule(Module & M) {
  for (Module::global_iterator it = M.global_begin(), end = M.global_end(); it != end; ++it) {
    if (GlobalVariable * GV = dyn_cast<GlobalVariable>(it)) {
      if (isa<StructType>(GV->getType()->getElementType())) {
        replace(&M, GV);
      }
    }
  }
  return true;
}

void PacketScalarReplacement::replace(Module * M, GlobalVariable * GV) {
  std::vector<Value *> scalars;
  generateScalars(M, GV, &scalars);
  for (Value::use_iterator it = GV->use_begin(), end = GV->use_end(); it != end; ++it) {
    assert (isa<ConstantExpr>(*it));
    ConstantExpr * CE = cast<ConstantExpr>(*it);
    assert (CE->getOpcode() == Instruction::GetElementPtr);
    ConstantInt * const_index = dyn_cast<ConstantInt>((*it)->getOperand(2));
    assert(const_index);
    const APInt & apint_index = const_index->getValue();
    size_t idx = apint_index.getLimitedValue();
    CE->replaceAllUsesWith(scalars.at(idx));
  }
  
}

void PacketScalarReplacement::generateScalars(llvm::Module * m, GlobalVariable * GV, std::vector<Value *> * scalars) {
  using namespace llvm;
  scalars->clear();
  const llvm::StructType * type = cast<StructType>(GV->getType()->getElementType());
  
  for (unsigned i = 0; i < type->getNumElements(); ++i) {
    char name[64];
    snprintf(name, sizeof(name), "%s.%d", GV->getNameStr().c_str(), i);
    scalars->push_back(new GlobalVariable(*m, type->getElementType(i), false, GlobalVariable::ExternalLinkage, NULL, name));
  }
}

static RegisterPass<PacketScalarReplacement> X("packet-scalar-repl", "Scalar Replacement for Symbolic Packet",
                      false /* Only looks at CFG */,
                      false /* Analysis Pass */);

}
