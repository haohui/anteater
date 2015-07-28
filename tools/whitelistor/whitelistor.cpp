/// Read a list of prefixes, then generate a function that has the constraint for all these prefixes.

#include "anteater/Core/IRBuilder.h"
#include "anteater/Core/SymbolicPacket.h"

#include "SymbolicPacketImpl.h"


#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/ToolOutputFile.h>

#include <boost/bind.hpp>

#include <arpa/inet.h>
#include <cstdio>

using namespace llvm;

namespace {

cl::opt<unsigned>
g_dstIPIndex(cl::ValueRequired, "dst_ip_idx", cl::desc("The index of the destination IP field"), cl::value_desc("integer"));

cl::list<unsigned>
g_packetLayout(cl::ValueRequired, "pkt_layout", cl::desc("The layout of symbolic packet"), cl::value_desc("integer+"));

cl::opt<std::string>
g_outputFileName(cl::ValueRequired, "o", cl::desc("Output filename"), cl::value_desc("filename"));

}

class WhitelistFunctionCreator {
 public:
  WhitelistFunctionCreator(Module *);
  void addPrefix(uint32_t addr, size_t width);
  void finalize();
  void dumpModule(const std::string & filename) const;
 private:
  StructType * getPacketType();
  Module * m_module;
  Function * m_filterFunction;
  Value * m_constraint;
  Value * m_ip;
  llvm::IRBuilder<> * m_builder;
};

int main(int argc, char * argv[]) {
  cl::ParseCommandLineOptions(argc, argv,
				    "Whitelist generator to filter out prefixes\n");

  Module * m = new Module("anteater", getGlobalContext());
  WhitelistFunctionCreator c(m);
  char buf[256];
  while (fgets(buf, sizeof(buf), stdin)) {
    char * slash = strstr(buf, "/");
    size_t width = strtol(slash + 1, NULL, 10);
    *slash = 0;
    in_addr_t addr = ntohl(inet_addr(buf));
    c.addPrefix(addr, width);
  }
  c.finalize();
  c.dumpModule(g_outputFileName);
  return 0;
}


WhitelistFunctionCreator::WhitelistFunctionCreator(Module * module)
    : m_module(module)
    , m_constraint(NULL)
{
  m_filterFunction = Function::Create
      (FunctionType::get(IntegerType::get(m_module->getContext(), 1), std::vector<const Type*>(), false),
       Function::ExternalLinkage, anteater::IRBuilder::kFilterFunctionName, m_module);
  m_filterFunction->setOnlyReadsMemory();
  
  BasicBlock * BB = BasicBlock::Create(m_module->getContext(), "entry", m_filterFunction);
  m_builder = new llvm::IRBuilder<>(m_module->getContext());
  m_builder->SetInsertPoint(BB);

  GlobalVariable * pkt = new GlobalVariable(*m_module, getPacketType(), false, GlobalVariable::ExternalLinkage, NULL, "pkt");
  m_ip = m_builder->CreateLoad(m_builder->CreateStructGEP(pkt, g_dstIPIndex));
}

void WhitelistFunctionCreator::dumpModule(const std::string & filename) const {
  std::string ErrorInfo;
  tool_output_file Out(filename.c_str(), ErrorInfo, raw_fd_ostream::F_Binary);
  if (!ErrorInfo.empty()) {
    return;
  }
  
  WriteBitcodeToFile(m_module, Out.os());
  Out.keep();
}

void WhitelistFunctionCreator::addPrefix(uint32_t addr, size_t width) {
  uint32_t mask = 0xffffffffUL << (32 - width);
  Value * v = m_builder->CreateICmpEQ
      (ConstantInt::get(m_module->getContext(), APInt(32, addr)),
       m_builder->CreateAnd(m_ip, ConstantInt::get(m_module->getContext(), APInt(32, mask)))
       );
  m_constraint = m_constraint ? m_builder->CreateOr(m_constraint, v) : v;
}

void WhitelistFunctionCreator::finalize() {
  assert (m_constraint);
  m_builder->CreateRet(m_constraint);
}

StructType * WhitelistFunctionCreator::getPacketType() {
  std::for_each(g_packetLayout.begin(), g_packetLayout.end(),
                boost::bind(anteater::SymbolicPacket::appendField, _1));
  return anteater::SymbolicPacketImpl::Type::instance().createPacketType(m_module);
}
