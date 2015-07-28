/// Crazy simple GVN

#include "anteater/Core/IRBuilder.h"

#include <llvm/Module.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/Support/InstIterator.h>

#include <boost/unordered_map.hpp>

#define DEBUG_TYPE "BareGVN"

NAMESPACE_ANTEATER_BEGIN

STATISTIC(NumBareGVNCalls,  "Number of edge calls removed by Bare GVN");

using namespace llvm;

class ValueSignature {
public:
  ValueSignature(CallInst * CI);
  bool operator==(const ValueSignature & rhs) const;
  typedef SmallVector<Value *, 2> ArgList;
  Function * m_function;
  ArgList m_args;
};

struct ValueSignatureHash : public std::unary_function<ValueSignature, std::size_t> {
  std::size_t operator()(const ValueSignature & v) const {
    size_t seed = reinterpret_cast<uintptr_t>(v.m_function) >> 2;
    for(ValueSignature::ArgList::const_iterator it = v.m_args.begin(), end = v.m_args.end(); it != end; ++it)
      boost::hash_combine(seed, reinterpret_cast<uintptr_t>(*it) >> 2);

    return seed;
  }
};

class BareGVN : public FunctionPass {
public:
  BareGVN();
  const char * getPassName() const { return "Bare GVN"; }
  static char ID;
  bool runOnFunction(Function & F);
private:
  typedef boost::unordered_map<ValueSignature, Value*, ValueSignatureHash> EdgeCallMap;
  EdgeCallMap m_edgeCallValue;
};

char BareGVN::ID = 0;

BareGVN::BareGVN()
  : FunctionPass(ID)
{}

bool BareGVN::runOnFunction(Function & F) {
  assert (F.getBasicBlockList().size () == 1);

  Function * record_func = F.getParent()->getFunction(IRBuilder::kRecordTransformationFunctionName);
  
  inst_iterator it = inst_begin(F);
  while (it != inst_end(F)) {
    CallInst * CI = dyn_cast<CallInst>(&*(it++));
    if (CI) {
      if (CI->getCalledFunction() == record_func && CI->getArgOperand(1) == CI->getArgOperand(2)) {
	++NumBareGVNCalls;
        CI->eraseFromParent();
        continue;
      }
      
      ValueSignature info(CI);
      EdgeCallMap::iterator VI = m_edgeCallValue.find(info);
      if (VI != m_edgeCallValue.end()) {
	++NumBareGVNCalls;
	CI->replaceAllUsesWith(VI->second);
        CI->eraseFromParent();
        
      } else {
	m_edgeCallValue[info] = CI;
      }
    }
  }
  return true;
}

ValueSignature::ValueSignature(CallInst * CI)
  : m_function(CI->getCalledFunction())
{
  for (unsigned i = 0; i < CI->getNumArgOperands(); ++i) {
    m_args.push_back(CI->getArgOperand(i));
  }
}

bool ValueSignature::operator==(const ValueSignature & rhs) const {
  return m_function == rhs.m_function && m_args == rhs.m_args;
}

namespace {
  RegisterPass<BareGVN> X("bare-gvn", "Bare Global Value Numbering for Anteater", false, false);
}

NAMESPACE_ANTEATER_END
