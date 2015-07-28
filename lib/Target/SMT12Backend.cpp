#include "anteater/Core/IRBuilder.h"

#include <llvm/Pass.h>
#include <llvm/Support/InstVisitor.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>

#include <boost/bind.hpp>

#include <cstdio>

using namespace llvm;

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

template <>
void NameDecorator<Value>::out(raw_ostream & os) const {
  if (const ConstantInt * CI = dyn_cast<ConstantInt>(v)) {
    switch (CI->getBitWidth()) {
      case 1:
        os << (CI->isOne() ? "true" : "false");
        break;
        
      case 16: {
        char buf[32];
        snprintf(buf, sizeof(buf), "bvhex%04x", (unsigned short)(CI->getLimitedValue()));
        os << buf;
        break;
      }
        
      case 32: {
        char buf[32];
        snprintf(buf, sizeof(buf), "bvhex%08x", (unsigned int)(CI->getLimitedValue()));
        os << buf;
        break;
      }

      default:
        assert(0 && "Unimplemented");
    }
    
  } else if (const LoadInst * LI = dyn_cast<LoadInst>(v)) {
    os << NameDecorator(LI->getPointerOperand());
    
  } else if (isa<GlobalVariable>(v)) {
    os << v->getName();
    
  } else {
    os << "?" << v->getName();
  }
}

template<>
void NameDecorator<Type>::out(raw_ostream & os) const {
  if (const PointerType * pt = dyn_cast<PointerType>(v)) {
    os << NameDecorator<Type>(pt->getElementType());
  } else if (v->isIntegerTy(1)) {
    os << "Bool";
  } else if (v->isIntegerTy()) {
    os << "BitVec[" << cast<IntegerType>(v)->getBitWidth() << "]";
  } else {
    assert (0 && "Unspported Type");
  }
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
  void end();
  
 private:
  static NameDecorator<Value> tr(const Value * v) { return NameDecorator<Value>(v); }
  static void bailOut();
  void outInstruction(const char * op, unsigned op_count, const Instruction& I);
  void outInstNameBegin(const Instruction & I);
  void outInstNameEnd();

  raw_ostream & m_out;
  LLVMContext & m_context;
  unsigned m_pathenessCounter;
};

InstTranslator::InstTranslator(raw_ostream & out, LLVMContext & context)
    : m_out(out)
    , m_context(context)
    , m_pathenessCounter(0)
{}
  
void InstTranslator::visitBinaryOperator(BinaryOperator &I) {
  outInstNameBegin(I);
  switch (I.getOpcode()) {
    case Instruction::And: {
      const char * op = I.getOperand(0)->getType()->isIntegerTy(1) ? "and" : "bvand";
      outInstruction(op, 2, I);
    }
      break;
        
    case Instruction::Or: {
      const char * op = I.getOperand(0)->getType()->isIntegerTy(1) ? "or" : "bvor";
      outInstruction(op, 2, I);
    }
      break;
      
    case Instruction::Xor: {
      // LLVM uses Xor to implement not
      if (I.getOperand(1) == ConstantInt::getTrue(m_context)) {
        outInstruction("not", 1, I);
      }
      else {
        const char * op = I.getOperand(0)->getType()->isIntegerTy(1) ? "xor" : "bvxor";
        outInstruction(op, 2, I);
      }
    }
      break;
      
    default:
      bailOut();
  }
  
  outInstNameEnd();
}

void InstTranslator::visitICmpInst(ICmpInst &I) {
  outInstNameBegin(I);
  switch (I.getPredicate()) {
    case CmpInst::ICMP_EQ:
      outInstruction("=", 2, I);
      break;
      
    case CmpInst::ICMP_NE:
      m_out << "(not ";
      outInstruction("=", 2, I);
      m_out << ")";
      break;
      
    default:
      bailOut();
  }
  outInstNameEnd();
}

void InstTranslator::visitCallInst(CallInst &I) {
  if (!I.getCalledFunction()->getName().equals(anteater::IRBuilder::kAssertionFunctionName)
      && !I.getCalledFunction()->getName().equals(anteater::IRBuilder::kPreconditionFunctionName))
    bailOut();
  
  m_out << "(" << tr(I.getArgOperand(0)) << ")";
}


void InstTranslator::end() {
  for (unsigned i = 0; i < m_pathenessCounter; ++i)
    m_out << ")";
}

void InstTranslator::bailOut() {
  assert (0 && "Unspported Instruction");
}

void InstTranslator::outInstruction(const char * op, unsigned op_count, const Instruction& I) {
  m_out << "(" << op;
  
  for (unsigned i = 0; i < op_count; ++i)
    m_out << " " << tr(I.getOperand(i));
  
  m_out << ")";
}

void InstTranslator::outInstNameBegin(const Instruction & I) {
  m_out << "(let (" << NameDecorator<Value>(&I) << " ";
  ++m_pathenessCounter;
}

void InstTranslator::outInstNameEnd() {
  m_out << ")\n ";
}

} // end of anonymous namespace


NAMESPACE_ANTEATER_BEGIN

class SMT12Writer : public ModulePass {
public:
  explicit SMT12Writer(raw_ostream & out);
  const char * getPassName() const { return "SMT12 Writer"; }
  bool runOnModule(Module &);
private:
  void declareGlobalVariable(const GlobalVariable &);
  raw_ostream & m_out;
  static char ID;
};

char SMT12Writer::ID = 0;

SMT12Writer::SMT12Writer(raw_ostream & out)
    : ModulePass(ID)
    , m_out(out)
{}

bool SMT12Writer::runOnModule(Module& M) {
  Function * main = M.getFunction(IRBuilder::kMainFunctionName);
  Function * assertion = M.getFunction(IRBuilder::kAssertionFunctionName);
  
  if (!main || !assertion)
    return false;

  assert (assertion->hasOneUse());
  
  m_out << "(benchmark anteater\n"
      ":logic QF_BV\n"; 

  std::for_each(M.global_begin(), M.global_end(),
                boost::bind(&SMT12Writer::declareGlobalVariable, this, _1));

  m_out << ":formula ";
  
  InstTranslator tr(m_out, M.getContext());
  tr.visit(main);
  tr.end();

  m_out << "\n)\n";
  return true;
}

void SMT12Writer::declareGlobalVariable(const GlobalVariable & v) {
  m_out << ":extrafuns ((" << NameDecorator<Value>(&v)
      << " " << NameDecorator<Type>(v.getType()) << "))\n";
}

Pass * createSMT12Writer(raw_ostream * out) {
  return new SMT12Writer(*out);
}

NAMESPACE_ANTEATER_END
