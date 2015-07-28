/**
 * Thin wrapper for LLVM's IRBuilder
 **/

#include "SymbolicPacketImpl.h"
#include "anteater/Core/IRBuilder.h"

#include <llvm/Attributes.h>
#include <llvm/Module.h>
#include <llvm/PassManager.h>
#include <llvm/LLVMContext.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Analysis/Verifier.h>

#include <cstdio>
#include <map>

NAMESPACE_ANTEATER_BEGIN

using namespace llvm;

const char * const IRBuilder::kAssertionFunctionName = "anteater.assert";
const char * const IRBuilder::kMainFunctionName = "main";
const char * const IRBuilder::kIdTransformationFunctionName = "anteater.ident";
const char * const IRBuilder::kTrivialTrueFunctionName = "anteater.true";
const char * const IRBuilder::kPreconditionFunctionName = "anteater.precondition";
const char * const IRBuilder::kFilterFunctionName = "anteater.filter";
const char * const IRBuilder::kRecordTransformationFunctionName = "anteater.record_transformation";
const char * const IRBuilder::kEdgeFunctionPrefix = "e";

class IRBuilderImpl {
 public:
  IRBuilderImpl();
  ~IRBuilderImpl();
  
  void createMainFunction();
  void resetMainFunction();
  void deleteConstraintFunctionBody();

  void checkPoint();
  void rewindToCheckPoint();
  
  ir_value_t createEdgeFunction(SymbolicPacket * p_in, SymbolicPacket * p_out);
  ir_value_t createCallEdgeFunction(ir_value_t func, const SymbolicPacket * p_in, const SymbolicPacket * p_out);
  ir_value_t createCallFilterFunction();

  ir_value_t recordTransformationConstraint(unsigned hop, const SymbolicPacket * sym_packet_in, const SymbolicPacket * sym_packet_out, ir_value_t constraint);

  static inline ir_value_t pkt_handle(const SymbolicPacket * p) {
    return p->impl()->m_handle;
  }

  static inline void set_pkt_handle(SymbolicPacket * p, ir_value_t v) {
    p->impl()->m_handle = v;
  }

  Module * m_module;
  llvm::IRBuilder<> * m_builder;

  Function * m_mainFunction;
  Function * m_assertionFunction;
  Function * m_identicalTransformationFunction;
  Function * m_trivialTrueFunction;
  Function * m_preconditionFunction;
  Function * m_defaultFilterFunction;
  Function * m_transformationConstraintFunction;

  StructType * m_packetType;
  const Type * m_boolType;

  Value * m_boolTrue;
  Value * m_boolFalse;
  ReturnInst * m_retInstInMain;
  Instruction * m_checkPoint;
  std::map<std::pair<ir_value_t, ir_value_t>, ir_value_t> m_identTagMap;

 private:
  void createBasicFunctions();
  Function * createEdgeFunctionPrototype(const char *);
  void resetInsertPoint();

  unsigned m_edgeFunctionId;
};

IRBuilder::IRBuilder()
  : m_impl(new IRBuilderImpl())
{}

IRBuilder::~IRBuilder() { 
  delete m_impl;
}

void IRBuilder::createMainFunction() {
  m_impl->createMainFunction();
}

void IRBuilder::resetMainFunction() {
  m_impl->resetMainFunction();
}

void IRBuilder::deleteConstraintFunctionBody() {
  m_impl->deleteConstraintFunctionBody();
}

ir_value_t IRBuilder::createAndExpr(ir_value_t LHS, ir_value_t RHS) {
  if (LHS == RHS)
    return LHS;

  if (LHS == m_impl->m_boolFalse || RHS == m_impl->m_boolFalse)
    return m_impl->m_boolFalse;
  
  if (LHS == m_impl->m_boolTrue)
    return RHS;

  if (RHS == m_impl->m_boolTrue)
    return LHS;
  
  return m_impl->m_builder->CreateAnd(LHS, RHS);
}

ir_value_t IRBuilder::createOrExpr(ir_value_t LHS, ir_value_t RHS) {
  if (LHS == RHS)
    return LHS;

  if (LHS == m_impl->m_boolTrue || RHS == m_impl->m_boolTrue)
    return m_impl->m_boolTrue;

  if (LHS == m_impl->m_boolFalse)
    return RHS;

  if (RHS == m_impl->m_boolFalse)
    return LHS;

  return m_impl->m_builder->CreateOr(LHS, RHS);
}

ir_value_t IRBuilder::createBitwiseAndExpr(ir_value_t LHS, ir_value_t RHS) {
  return createAndExpr(LHS, RHS);
}

ir_value_t IRBuilder::createBitwiseEqualExpr(ir_value_t LHS, ir_value_t RHS) {
  return m_impl->m_builder->CreateICmpEQ(LHS, RHS);
}

ir_value_t IRBuilder::createBitwiseNeqExpr(ir_value_t LHS, ir_value_t RHS) {
  return m_impl->m_builder->CreateICmpNE(LHS, RHS);
}

ir_value_t IRBuilder::createXorExpr(ir_value_t LHS, ir_value_t RHS) {
  return m_impl->m_builder->CreateXor(LHS, RHS);
}

ir_value_t IRBuilder::createAssertion(ir_value_t v) {
  CallInst * CI =  m_impl->m_builder->CreateCall(m_impl->m_assertionFunction, v);
  CI->setOnlyReadsMemory();
  CI->doesNotThrow();
  return CI;
}

ir_value_t IRBuilder::createPrecondition(ir_value_t v) {
  CallInst * CI =  m_impl->m_builder->CreateCall(m_impl->m_preconditionFunction, v);
  CI->setOnlyReadsMemory();
  CI->doesNotThrow();
  return CI;
}

ir_value_t IRBuilder::createNotExpr(ir_value_t v) {
  return m_impl->m_builder->CreateNot(v);
}

ir_value_t IRBuilder::createImpliesExpr(ir_value_t v1, ir_value_t v2) {
  return createOrExpr(createNotExpr(v1), v2);
}

ir_value_t IRBuilder::createLoad(ir_value_t v) {
  return m_impl->m_builder->CreateLoad(v);
}

ir_value_t IRBuilder::createStructGEP(ir_value_t v, unsigned index) {
  return m_impl->m_builder->CreateStructGEP(v, index);
}

void IRBuilder::createRet(ir_value_t v) {
  m_impl->m_builder->CreateRet(v);
}

ir_value_t IRBuilder::createEdgeFunction(SymbolicPacket * p_in, SymbolicPacket * p_out) {
  return m_impl->createEdgeFunction(p_in, p_out);
}

ir_value_t IRBuilder::createCallEdgeFunction(ir_value_t func, const SymbolicPacket * p_in, const SymbolicPacket * p_out) {
  return m_impl->createCallEdgeFunction(func, p_in, p_out);
}

ir_value_t IRBuilder::createCallFilterFunction() {
  return m_impl->createCallFilterFunction();
}

bool IRBuilder::isCallEdgeInst(ir_value_t inst) {
  CallInst * CI = dyn_cast<CallInst>(inst);
  if (!CI)
    return false;
  return CI->getCalledFunction()->getName().startswith(kEdgeFunctionPrefix);
}

ir_value_t IRBuilder::recordTransformationConstraint(unsigned hop, const SymbolicPacket * sym_packet_in, const SymbolicPacket * sym_packet_out, ir_value_t c) {
  return m_impl->recordTransformationConstraint(hop, sym_packet_in, sym_packet_out, c);
}

void IRBuilder::checkPoint() {
  m_impl->checkPoint();
}

void IRBuilder::rewindToCheckPoint() {
  m_impl->rewindToCheckPoint();
}

ir_value_t IRBuilder::getConstantInt(long value, unsigned length) {
  return ConstantInt::get(m_impl->m_module->getContext(), APInt(length, value));
}

ir_value_t IRBuilder::getConstantBitVector(long value, unsigned length) {
  return ConstantInt::get(IntegerType::get(m_impl->m_module->getContext(), length), value);
}

ir_value_t IRBuilder::getConstantBool(bool value) {
  return value ? m_impl->m_boolTrue : m_impl->m_boolFalse;
}

ir_value_t IRBuilder::createVariable(ir_type_t ty, const char * name) {
  return new GlobalVariable(*(m_impl->m_module), ty, false, GlobalVariable::ExternalLinkage, NULL, name);
}

ir_type_t IRBuilder::getBitVectorType(unsigned bits) {
  return IntegerType::get(m_impl->m_module->getContext(), bits);
}

ir_type_t IRBuilder::getPacketType() const {
  return m_impl->m_packetType;
}

ir_value_t IRBuilder::getIdenticalTransformationFunction() const {
  return m_impl->m_identicalTransformationFunction;
}

ir_value_t IRBuilder::getTrivialTrueFunction() const {
  return m_impl->m_trivialTrueFunction;
}

int IRBuilder::dumpModule(const char * filename) const {
  std::string ErrorInfo;
  tool_output_file Out(filename, ErrorInfo, raw_fd_ostream::F_Binary);
  if (!ErrorInfo.empty()) {
    return -1;
  }

  PassManager Passes;
  //  Passes.add(createVerifierPass());
  Passes.add(createBitcodeWriterPass(Out.os()));
  Passes.run(*m_impl->m_module);
  Out.keep();
  return 0;
}

////////////////////////////////////////////////////////////////////////
///                                                                  ///
/// IRBuilderImpl                                                    ///
///                                                                  ///
////////////////////////////////////////////////////////////////////////
IRBuilderImpl::IRBuilderImpl()
    : m_module(new Module("anteater", getGlobalContext()))
    , m_builder(new llvm::IRBuilder<>(m_module->getContext()))
    , m_edgeFunctionId(0)
{
  LLVMContext & c = m_module->getContext();
  m_packetType    = SymbolicPacketImpl::Type::instance().createPacketType(m_module);
  m_boolType      = IntegerType::get(c, 1);
  m_boolTrue      = ConstantInt::getTrue(c);
  m_boolFalse     = ConstantInt::getFalse(c);
  
  createBasicFunctions();
}

IRBuilderImpl::~IRBuilderImpl() {
  delete m_builder;
  delete m_module;
}

void IRBuilderImpl::createMainFunction() {
  m_mainFunction = Function::Create(FunctionType::get(Type::getVoidTy(m_module->getContext()), false), Function::ExternalLinkage, IRBuilder::kMainFunctionName, m_module);

  BasicBlock * BB = BasicBlock::Create(m_module->getContext(), "entry", m_mainFunction);
  m_builder->SetInsertPoint(BB);
  m_retInstInMain = m_builder->CreateRetVoid();
  resetInsertPoint();
}

void IRBuilderImpl::resetMainFunction() {
  m_mainFunction->deleteBody();
  m_identTagMap.clear();
  BasicBlock * BB = BasicBlock::Create(m_module->getContext(), "entry", m_mainFunction);
  m_builder->SetInsertPoint(BB);
  m_retInstInMain = m_builder->CreateRetVoid();
  resetInsertPoint();  
}

void IRBuilderImpl::deleteConstraintFunctionBody() {
  for (Module::iterator it = m_module->begin(), end = m_module->end(); it != end; ++it) {
    if (it->getNameStr() != IRBuilder::kMainFunctionName)
      it->deleteBody();
  }
}

ir_value_t IRBuilderImpl::createEdgeFunction(SymbolicPacket * p_in, SymbolicPacket * p_out) {
  char buf[32];
  snprintf(buf, sizeof(buf), "e%d", ++m_edgeFunctionId);
  
  Function * func = createEdgeFunctionPrototype(buf);
  func->addFnAttr(Attribute::AlwaysInline);  
  func->arg_begin()->setName("p_in");
  (++func->arg_begin())->setName("p_out");
  set_pkt_handle(p_in, func->arg_begin());
  set_pkt_handle(p_out, ++func->arg_begin());
  
  BasicBlock * BB = BasicBlock::Create(m_module->getContext(), "entry", func);
  m_builder->SetInsertPoint(BB);
  return func;
}

ir_value_t IRBuilderImpl::createCallEdgeFunction(ir_value_t func, const SymbolicPacket * p_in, const SymbolicPacket * p_out) {
  if (func == m_trivialTrueFunction
      || (func == m_identicalTransformationFunction && p_in == p_out)) {
    return m_boolTrue;
    
  } else if (func == m_identicalTransformationFunction) {

    std::pair<ir_value_t, ir_value_t> key(pkt_handle(p_in), pkt_handle(p_out));
    if (m_identTagMap.find(key) == m_identTagMap.end()) {
      CallInst * CI = m_builder->CreateCall2(func, pkt_handle(p_in), pkt_handle(p_out));
      CI->setOnlyReadsMemory();
      CI->doesNotThrow();
      m_identTagMap[key] = CI;
    }

    return m_identTagMap[key];
    
  } else {
    CallInst * CI = m_builder->CreateCall2(func, pkt_handle(p_in), pkt_handle(p_out));
    CI->setOnlyReadsMemory();
    CI->doesNotThrow();
    return CI;
  }
}

ir_value_t IRBuilderImpl::createCallFilterFunction() {
  CallInst * CI = m_builder->CreateCall(m_defaultFilterFunction);
  CI->setOnlyReadsMemory();
  CI->doesNotThrow();
  return CI;
}

ir_value_t IRBuilderImpl::recordTransformationConstraint(unsigned hop, const SymbolicPacket * sym_packet_in, const SymbolicPacket * sym_packet_out, ir_value_t constraint) {
  CallInst * CI = m_builder->CreateCall4(m_transformationConstraintFunction,
                                         ConstantInt::get(m_module->getContext(), APInt(32, hop)), pkt_handle(sym_packet_in), pkt_handle(sym_packet_out),
                                         constraint);
  CI->setOnlyReadsMemory();
  CI->doesNotThrow();
  return CI;

}


void IRBuilderImpl::checkPoint() {
  BasicBlock::iterator it(m_retInstInMain);
  m_checkPoint = --it;
}

void IRBuilderImpl::rewindToCheckPoint() {
  BasicBlock::iterator begin(m_checkPoint), end(m_retInstInMain);
  ++begin;
  for (BasicBlock::iterator it = begin; it != end; ++it)
    it->dropAllReferences();
  
  for (BasicBlock::iterator it = begin; it != end;)
    (it++)->eraseFromParent();

  resetInsertPoint();
}


void IRBuilderImpl::createBasicFunctions() {
  LLVMContext & c = m_module->getContext();
  const Type * void_type = Type::getVoidTy(c);
  
  {
    const FunctionType * t = FunctionType::get(void_type, std::vector<const Type*>(1, m_boolType), false);

    m_assertionFunction = Function::Create(t, Function::ExternalLinkage,
                                     IRBuilder::kAssertionFunctionName, m_module);
    m_assertionFunction->setOnlyReadsMemory();

    m_preconditionFunction = Function::Create(t, Function::ExternalLinkage,
                                              IRBuilder::kPreconditionFunctionName, m_module);
    m_preconditionFunction->setOnlyReadsMemory();
  }

  {
    m_trivialTrueFunction = createEdgeFunctionPrototype(IRBuilder::kTrivialTrueFunctionName);
    m_identicalTransformationFunction = createEdgeFunctionPrototype(IRBuilder::kIdTransformationFunctionName);
    SymbolicPacketImpl::Type::instance().generateIdenticalTransformationFunctionBody(m_identicalTransformationFunction);
  }
  
  {
    std::vector<const Type *> params;
    params.push_back(IntegerType::get(c, 32));
    params.push_back(PointerType::getUnqual(m_packetType));
    params.push_back(PointerType::getUnqual(m_packetType));
    params.push_back(m_boolType);

    m_transformationConstraintFunction = Function::Create(FunctionType::get(void_type, params, false),
                                                          Function::ExternalLinkage, IRBuilder::kRecordTransformationFunctionName, m_module);
    m_transformationConstraintFunction->setOnlyReadsMemory();
  }
  
  {
    m_defaultFilterFunction = Function::Create(FunctionType::get(m_boolType, false),
                                               Function::WeakAnyLinkage, IRBuilder::kFilterFunctionName, m_module);
    m_defaultFilterFunction->setOnlyReadsMemory();
    BasicBlock * BB = BasicBlock::Create(c, "entry", m_defaultFilterFunction);
    ReturnInst::Create(c, m_boolFalse, BB);
  }
}

Function * IRBuilderImpl::createEdgeFunctionPrototype(const char * name) {
  PointerType * p_packet_type = PointerType::get(m_packetType, 0);
  Function * func = Function::Create(
      FunctionType::get(m_boolType, std::vector<const Type*>(2, p_packet_type), false),
      Function::ExternalLinkage, name, m_module);
  
  func->setDoesNotAlias(1);
  func->setDoesNotAlias(2);
  func->setOnlyReadsMemory();
  func->setDoesNotThrow();
  return func;
}

void IRBuilderImpl::resetInsertPoint() {
  m_builder->SetInsertPoint(m_retInstInMain->getParent(), m_retInstInMain);
}


NAMESPACE_ANTEATER_END

///
/// C Bindings
///

using namespace anteater;

ANTEATER_EXTERN anteater::IRBuilder * AnteaterIRBuilderCreate(void) {
  return new anteater::IRBuilder();
}

ANTEATER_EXTERN void AnteaterIRBuilderFree(anteater::IRBuilder * b) {
  delete b;
}

ANTEATER_EXTERN void AnteaterIRBuilderCreateMainFunction(anteater::IRBuilder * b) {
  b->createMainFunction();
}

ANTEATER_EXTERN void AnteaterIRBuilderClearMainFunction(anteater::IRBuilder * b) {
  b->resetMainFunction();
}

ANTEATER_EXTERN void AnteaterIRBuilderClearConstraintFunctions(anteater::IRBuilder * b) {
  b->deleteConstraintFunctionBody();
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderCreateAnd(anteater::IRBuilder * b, ir_value_t lhs, ir_value_t rhs) {
  return b->createAndExpr(lhs, rhs);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderCreateOr(anteater::IRBuilder * b, ir_value_t lhs, ir_value_t rhs) {
  return b->createOrExpr(lhs, rhs);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderCreateEQ(anteater::IRBuilder * b, ir_value_t lhs, ir_value_t rhs) {
  return b->createBitwiseEqualExpr(lhs, rhs);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderCreateXor(anteater::IRBuilder * b, ir_value_t lhs, ir_value_t rhs) {
  return b->createXorExpr(lhs, rhs);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderCreateAssertion(anteater::IRBuilder * b, ir_value_t v) {
  return b->createAssertion(v);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderCreateNot(anteater::IRBuilder * b, ir_value_t v) {
  return b->createNotExpr(v);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderCreateLoad(anteater::IRBuilder * b, ir_value_t v) {
  return b->createLoad(v);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderCreateEdgeFunction(anteater::IRBuilder * b, SymbolicPacket * p_in, SymbolicPacket * p_out) {
  return b->createEdgeFunction(p_in, p_out);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderCreateCallEdgeFunction(anteater::IRBuilder * b, ir_value_t func, const SymbolicPacket * p_in, const SymbolicPacket * p_out) {
  return b->createCallEdgeFunction(func, p_in, p_out);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderCreateCallFilterFunction(anteater::IRBuilder * b) {
  return b->createCallFilterFunction();
}

ANTEATER_EXTERN void AnteaterIRBuilderCreateRet(anteater::IRBuilder * b, ir_value_t v) {
  b->createRet(v);
}

ANTEATER_EXTERN void AnteaterIRBuilderCheckPointMainFunction(anteater::IRBuilder * b) {
  b->checkPoint();
}

ANTEATER_EXTERN void AnteaterIRBuilderRewindToCheckPoint(anteater::IRBuilder * b) {
  b->rewindToCheckPoint();
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderGetConstantInt(anteater::IRBuilder * b, long value, unsigned length) {
  return b->getConstantInt(value, length);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderGetConstantBitVector(anteater::IRBuilder * b, long value, unsigned length) {
  return b->getConstantBitVector(value, length);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderGetConstantBool(anteater::IRBuilder * b, bool value) {
  return b->getConstantBool(value);
}

ANTEATER_EXTERN ir_value_t AnteaterIRBuilderGetIdenticalTransformationFunction(const anteater::IRBuilder * b) {
  return b->getIdenticalTransformationFunction();
}
  
ANTEATER_EXTERN int AnteaterIRBuilderDumpModule(const anteater::IRBuilder * b, const char * filename) {
  return b->dumpModule(filename);
}

