/**
 * Transform xor %1, %2 => (%1 and (not %2)) and ((not %1) and %2)
 **/

#include "anteater/config.h"
#include "anteater/Target/Backend.h"

#include <llvm/Pass.h>
#include <llvm/Support/IRBuilder.h>

NAMESPACE_ANTEATER_BEGIN

using namespace llvm;

class XorEliminationPass : public BasicBlockPass {
public:
  static char ID;
  
  XorEliminationPass(LLVMContext & context);
  const char * getPassName() const { return "Xor elimination"; }
  bool runOnBasicBlock(BasicBlock & BB);
  
private:
  LLVMContext & m_context;
  llvm::IRBuilder<> m_builder;
};


XorEliminationPass::XorEliminationPass(LLVMContext & context)
    : BasicBlockPass(ID)
    , m_context(context)
    , m_builder(context)
{}

bool XorEliminationPass::runOnBasicBlock(BasicBlock & BB) {
  Value * True = ConstantInt::getTrue(m_context);
  bool modified = false;
  BasicBlock::iterator it = BB.begin();

  while (it != BB.end()) {
    Instruction * I(it);
    if (BinaryOperator * BO = dyn_cast<BinaryOperator>(I)) {
      Value * a = BO->getOperand(0);
      Value * b = BO->getOperand(1); 
      if (BO->getOpcode() == Instruction::Xor && a != True && b != True) {
	modified = true;
	m_builder.SetInsertPoint(&BB, it);
	++it;
	Value * na = m_builder.CreateNot(a);
	Value * nb = m_builder.CreateNot(b);
	Value * e = m_builder.CreateOr(m_builder.CreateAnd(a, nb),
                                       m_builder.CreateAnd(na, b));
	BO->replaceAllUsesWith(e);
	BO->eraseFromParent();
      } else {
	++it;
      }
    } else {
      ++it;
    }
  }
  return modified;
}

char XorEliminationPass::ID = 0;
Pass * createXorEliminationPass(LLVMContext * context) {
  return new XorEliminationPass(*context);
}

NAMESPACE_ANTEATER_END
