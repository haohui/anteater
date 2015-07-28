#include "anteater/Core/IRBuilder.h"

#include <llvm/Pass.h>
#include <llvm/Support/InstVisitor.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>

#include <boost/bind.hpp>

using namespace llvm;

static cl::opt<bool>
g_debugInfo("g", cl::desc("Enable debugging support"),
            cl::init(false));

static cl::opt<bool>
g_individualAssertion("individual-assertion",
                      cl::desc("Evaluate each assertion in different logic context(i.e., wrapping them with push/pop)"),
                      cl::init(false));

namespace {

template <class T>
struct NameDecorator {
  explicit NameDecorator(const T * const v) : v(v) {}
  const T * const v;
  void out(raw_ostream & os) const;
};

template <class T>
raw_ostream & operator<< (raw_ostream & os, const NameDecorator<T> & n) {
  n.out(os);
  return os;
}

static void outDeclaration(raw_ostream & os, const Value * v) {
  os << "(define " << NameDecorator<Value>(v) << "::" << NameDecorator<Type>(v->getType()) << ")\n";
}

template <>
void NameDecorator<Value>::out(raw_ostream & os) const {
  if (const ConstantInt * CI = dyn_cast<ConstantInt>(v)) {
    // boolean true or false
    if (CI->getBitWidth() == 1)
      os << (CI->isOne() ? "true" : "false");
    else {
      os << "(mk-bv " << CI->getBitWidth() << " ";
      CI->getValue().print(os, false);
      os << ")";
    }
  } else if (const LoadInst * LI = dyn_cast<LoadInst>(v)) {
    os << NameDecorator(LI->getPointerOperand());
  } else
    os << v->getName();
}

template<>
void NameDecorator<Type>::out(raw_ostream & os) const {
  if (const PointerType * pt = dyn_cast<PointerType>(v)) {
    os << NameDecorator<Type>(pt->getElementType());
  } else if (v->isIntegerTy()) {
    if (v->isIntegerTy(1))
      os << "bool";
    else
      os << "(bitvector " << cast<IntegerType>(v)->getBitWidth() << ")";
  } else
    assert (0 && "Unspported Type");
}

class InstTranslator : public InstVisitor<InstTranslator> {
public:
  InstTranslator(raw_ostream & out, LLVMContext & context);
  
  void visitBinaryOperator(BinaryOperator &I);
  void visitICmpInst(ICmpInst &I);
  void visitCallInst(CallInst &I);
  
  void visitLoadInst(LoadInst &I) {}
  void visitTerminatorInst(TerminatorInst &I) {}
  void visitInstruction(Instruction &I) { bailOut(); }
  
private:
  static NameDecorator<Value> tr(const Value * v) { return NameDecorator<Value>(v); }
  void bailOut();  
  void outInstruction(const char * op, unsigned op_count, const Instruction& I);
  
  raw_ostream & m_out;
  LLVMContext & m_context;
  unsigned m_assertionCounter;
};

InstTranslator::InstTranslator(raw_ostream & out, LLVMContext & context)
    : m_out(out)
    , m_context(context)
    , m_assertionCounter(1)
{}

void InstTranslator::visitBinaryOperator(BinaryOperator &I) {
  switch (I.getOpcode()) {
    case Instruction::And: {
      const char * op = I.getOperand(0)->getType()->isIntegerTy(1) ? "and" : "bv-and";
      outInstruction(op, 2, I);
    }
      break;
      
    case Instruction::Or:
      outInstruction("or", 2, I);
      break;
      
    case Instruction::Xor: {
      // LLVM uses Xor to implement not
      if (I.getOperand(1) == ConstantInt::getTrue(m_context))
	outInstruction("not", 1, I);
      else
	bailOut();
    }
      break;
      
    default:
      bailOut();
  }
}
  
void InstTranslator::visitICmpInst(ICmpInst &I) {
  switch (I.getPredicate()) {
    case CmpInst::ICMP_EQ:
      outInstruction("=", 2, I);
      break;
      
    case CmpInst::ICMP_NE:
      outInstruction("/=", 2, I);
      break;
      
    default:
      bailOut();
  }
}
    
void InstTranslator::visitCallInst(CallInst &I) {
  if (!I.getCalledFunction()->getName().equals(anteater::IRBuilder::kAssertionFunctionName)
      && !I.getCalledFunction()->getName().equals(anteater::IRBuilder::kPreconditionFunctionName))
    bailOut();
  
  bool output_push_pop = g_individualAssertion && I.getCalledFunction()->getName().equals(anteater::IRBuilder::kAssertionFunctionName);

  if (output_push_pop)
    m_out << "(push)\n";
  
  if (g_debugInfo) {
    m_out << ";; id: " << m_assertionCounter << "\n"
          << "(assert+ " << tr(I.getArgOperand(0)) << ")\n"
          << "(check)\n";
    m_out << "(retract " << m_assertionCounter++ << ")\n";
    
  } else {
    m_out << "(assert " << tr(I.getArgOperand(0)) << ")\n"
          << "(check)\n";
  }
  
  if (output_push_pop)
    m_out << "(pop)\n";
  
}
  
void InstTranslator::bailOut() {
  assert (0 && "Unspported Instruction");
}
  
void InstTranslator::outInstruction(const char * op, unsigned op_count, const Instruction& I) {
  if (g_debugInfo) {
    outDeclaration(m_out, &I);
    m_out << ";; id: " << m_assertionCounter++ << "\n"
          <<  "(assert+ (= " << tr(&I) << " (" << op;
    
    for (unsigned i = 0; i < op_count; ++i)
      m_out << " " << tr(I.getOperand(i));
    
    m_out << ")))\n";
    
  } else {
    m_out << "(define " << NameDecorator<Value>(&I) << "::"
          << NameDecorator<Type>((&I)->getType())
          << " " << " (" << op;
    
    for (unsigned i = 0; i < op_count; ++i)
      m_out << " " << tr(I.getOperand(i));
    
    m_out << "))\n";
  }
}
  
} // end of anonymous namespace

NAMESPACE_ANTEATER_BEGIN

class YicesWriter : public ModulePass {
public:
  static char ID;
  explicit YicesWriter(raw_ostream & out);
  const char * getPassName() const { return "Yices Writer"; }
  bool runOnModule(Module &);
private:
  raw_ostream & m_out;
};

char YicesWriter::ID = 0;

YicesWriter::YicesWriter(raw_ostream & out)
    : ModulePass(ID)
    , m_out(out)
{}

bool YicesWriter::runOnModule(Module & M) {
  Function * main = M.getFunction(IRBuilder::kMainFunctionName);
  if (!main)
    return false;
  
  for (Module::global_iterator it = M.global_begin(), end = M.global_end(); it != end; ++it)
    outDeclaration(m_out, &*it);
  
  InstTranslator tr(m_out, M.getContext());
  tr.visit(main);

  return true;
}


Pass * createYicesWriter(raw_ostream * out) {
  return new YicesWriter(*out);
}

NAMESPACE_ANTEATER_END
