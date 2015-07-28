#include "anteater/Core/IRBuilder.h"

#include <llvm/Pass.h>
#include <llvm/Instructions.h>
#include <llvm/Function.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/IRReader.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SystemUtils.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <set>
#include <map>
#include <cstdio>

using namespace llvm;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode file>"),
    cl::init(""), cl::value_desc("filename"));

static llvm::cl::opt<unsigned>
packet_aa_depth("packet-aa-depth", llvm::cl::desc("The maximum depth which packet-aa is going to explore"), llvm::cl::value_desc("filename"), llvm::cl::init(3));

namespace {

using anteater::IRBuilder;

class PacketAliasingAnalysis : public ModulePass {
 public:
  PacketAliasingAnalysis();
  const char * getPassName() const { return "Packet Must-Alias Analysis"; }
  bool runOnModule(Module & M);
  static char ID;
 private:
  void testCandidate(Module & M);
  void collectTransformationConstraint(Module & M);
  void removeFunction(Function * F);
  void generateTestBitCode(Module & M, unsigned hop);
  std::set<unsigned long> m_activeTransformationConstriant;
  std::set<std::pair<std::string, std::string> > m_aliasingCandidates;
};

char PacketAliasingAnalysis::ID = 0;

PacketAliasingAnalysis::PacketAliasingAnalysis()
    : ModulePass(ID)
{}

bool PacketAliasingAnalysis::runOnModule(Module & M) {
  Value::use_iterator it = M.getFunction(IRBuilder::kAssertionFunctionName)->use_begin();
  while (it != M.getFunction(IRBuilder::kAssertionFunctionName)->use_end()) {
    Value::use_iterator old_it = it++;
    cast<CallInst>(*old_it)->eraseFromParent();
  }
  
  collectTransformationConstraint(M);
  
  testCandidate(M);

  return true;
}

void PacketAliasingAnalysis::collectTransformationConstraint(Module & M) {
  Function * f = M.getFunction(IRBuilder::kRecordTransformationFunctionName);
  if (!f)
    return;
  
  for (Value::use_iterator it = f->use_begin(), end = f->use_end(); it != end; ++it) {
    if (CallInst * CI = dyn_cast<CallInst>(*it)) {
      unsigned long hop = cast<ConstantInt>(CI->getArgOperand(0))->getValue().getLimitedValue();
      m_activeTransformationConstriant.insert(hop);
    }
  }
}

void PacketAliasingAnalysis::testCandidate(Module & M) {
  unsigned counter = 0;
  for (std::set<unsigned long>::const_iterator it = m_activeTransformationConstriant.begin(), end = m_activeTransformationConstriant.end(); it != end && counter < packet_aa_depth; ++it, ++counter) {
    generateTestBitCode(M, *it);
  }
}

void PacketAliasingAnalysis::generateTestBitCode(Module & M, unsigned hop) {
  
  ValueToValueMapTy VMap;
  Module * new_m = CloneModule(&M, VMap);

  Function * assertion_function = new_m->getFunction(IRBuilder::kAssertionFunctionName);
  Function * precondition_function = new_m->getFunction(IRBuilder::kPreconditionFunctionName);

  // Recreate the function prototype deleted by ADCE
  if (!precondition_function) {
    const Type * void_type = Type::getVoidTy(new_m->getContext());
    const Type * i1_type = Type::getInt1Ty(new_m->getContext());
    const FunctionType * assertion_function_type = FunctionType::get(void_type, std::vector<const Type*>(1, i1_type), false);
    precondition_function = Function::Create(assertion_function_type, Function::ExternalLinkage, IRBuilder::kPreconditionFunctionName, new_m);
    precondition_function->setOnlyReadsMemory();
  }
  
  Value * pkt1 = NULL, * pkt2 = NULL;
  // Replace all record_transformation to precondition
  {
    Function * f = new_m->getFunction(IRBuilder::kRecordTransformationFunctionName);
    Value::use_iterator it = f->use_begin();
    while (it != f->use_end()) {
      Value::use_iterator old_it = it++;
      if (CallInst * CI = dyn_cast<CallInst>(*old_it)) {
        unsigned long h = cast<ConstantInt>(CI->getArgOperand(0))->getValue().getLimitedValue();
        if (h == hop) {
          pkt1 = CI->getArgOperand(1);
          pkt2 = CI->getArgOperand(2);
          CallInst * assertion = CallInst::Create(assertion_function, CI->getArgOperand(3), "", CI);
          assertion->setOnlyReadsMemory();
          assertion->doesNotThrow();
          CI->replaceAllUsesWith(assertion);
        }
        CI->eraseFromParent();
      }
    }
  }
  
  {
    BasicBlock::iterator it = new_m->getFunction(IRBuilder::kMainFunctionName)->front().begin();
    Function * identity_function = new_m->getFunction(IRBuilder::kIdTransformationFunctionName);

    Value * pkt[] = {pkt1, pkt2};
    CallInst * ident_call = CallInst::Create(identity_function, pkt, pkt + 2, "", it);
    ident_call->setOnlyReadsMemory();
    ident_call->doesNotThrow();
    
    Value * not_expr = BinaryOperator::CreateNot(ident_call, "", it);
    CallInst::Create(precondition_function, not_expr, "", it);
  }
 
  PassManager PM;
  PM.add(createAggressiveDCEPass());
  PM.run(*new_m);

  char buf[256];
  snprintf(buf, sizeof(buf), "%s-%s.bc", pkt1->getNameStr().c_str(), pkt2->getNameStr().c_str());
  std::string ErrorInfo;
  tool_output_file result_file(buf, ErrorInfo, raw_fd_ostream::F_Binary);
  WriteBitcodeToFile(new_m, result_file.os());
  result_file.keep();
}

}

int main(int argc, char * argv[]) {
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  LLVMContext &Context = getGlobalContext();
  
  cl::ParseCommandLineOptions(argc, argv,
    "Hint Generator\n");

  SMDiagnostic Err;

  // Load the input module...
  std::auto_ptr<Module> M;
  M.reset(ParseIRFile(InputFilename, Err, Context));

  if (M.get() == 0) {
    Err.Print(argv[0], errs());
    return 1;
  }

  PassManager PM;
  PM.add(new PacketAliasingAnalysis());
  PM.run(*M.get());
  
  return 0;
}
