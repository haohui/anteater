#include "SymbolicPacketImpl.h"
#include "anteater/Core/IRBuilder.h"

#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/DerivedTypes.h>
#include <llvm/GlobalVariable.h>
#include <llvm/Function.h>
#include <llvm/Support/IRBuilder.h>

#include <boost/bind.hpp>

#include <cassert>

NAMESPACE_ANTEATER_BEGIN

SymbolicPacket::SymbolicPacket()
    : m_impl(new SymbolicPacketImpl())
{}

SymbolicPacket::~SymbolicPacket() {
  delete m_impl;
}

ir_value_t SymbolicPacket::get(unsigned type, IRBuilder * builder) const {
  return m_impl->get(type, builder);
}

void SymbolicPacket::instantiateToIR(IRBuilder * builder) {
  m_impl->instantiateToIR(builder);
}

void SymbolicPacket::appendField(unsigned length) {
  SymbolicPacketImpl::appendField(length);
}

void SymbolicPacket::clearFields() {
  SymbolicPacketImpl::clearFields();
}

SymbolicPacketImpl::Type::Type()
    : m_freezed(false)
    , m_type(NULL)
{}

ir_value_t SymbolicPacketImpl::get(unsigned type, IRBuilder * builder) const {
  assert (m_handle && type < Type::instance().m_fields.size());
  return builder->createStructGEP(m_handle, type);
}

void SymbolicPacketImpl::instantiateToIR(IRBuilder * builder) {
  assert (!m_handle);
  m_handle = builder->createVariable(builder->getPacketType(), "pkt");
}

void SymbolicPacketImpl::appendField(unsigned length) {
  assert (!Type::instance().m_freezed);
  Type::instance().m_fields.push_back(length);
}

void SymbolicPacketImpl::clearFields() {
  Type::instance().m_fields.clear();
  Type::instance().m_type = NULL;
  Type::instance().m_freezed = false;
}

llvm::StructType * SymbolicPacketImpl::Type::createPacketType(llvm::Module * M) {
  assert (Type::instance().m_fields.size());
  
  llvm::StructType * type = Type::instance().m_type;
  if (type)
    return type;
  
  Type::instance().m_freezed = true;
  std::vector<const llvm::Type *> params;
  llvm::LLVMContext & c = M->getContext();

  std::transform(Type::instance().m_fields.begin(),
                 Type::instance().m_fields.end(),
                 std::back_inserter(params),
                 boost::bind(llvm::IntegerType::get, boost::ref(c), _1));
  
  type = Type::instance().m_type = llvm::StructType::get(M->getContext(), params);
  return type;
}

void SymbolicPacketImpl::Type::generateIdenticalTransformationFunctionBody(ir_value_t func) {
  using namespace llvm;
  assert (isa<Function>(func));
  Function * f = cast<Function>(func);
  f->addFnAttr(Attribute::AlwaysInline);  
  f->arg_begin()->setName("p_in");
  (++f->arg_begin())->setName("p_out");

  BasicBlock * BB = BasicBlock::Create(f->getContext(), "entry", f);
  llvm::IRBuilder<> builder(BB);
  Value * arg_p_in = f->arg_begin(), * arg_p_out = ++(f->arg_begin());
  Value * res = NULL;
  for (size_t i = 0; i < Type::instance().m_type->getNumElements(); ++i) {
    Value * v = builder.CreateICmpEQ
        (builder.CreateLoad(builder.CreateStructGEP(arg_p_in, i)),
         builder.CreateLoad(builder.CreateStructGEP(arg_p_out, i)));
    res = res ? builder.CreateAnd(res, v) : v;
  }
  builder.CreateRet(res);
}


NAMESPACE_ANTEATER_END

///
/// C bindings
///
#define ANTEATER_EXTERN extern "C"

using namespace anteater;

ANTEATER_EXTERN SymbolicPacket * AnteaterSymbolicPacketCreate(void) {
  return new SymbolicPacket();
}

ANTEATER_EXTERN void AnteaterSymbolicPacketFree(SymbolicPacket * pkt) {
  delete pkt;
}

ANTEATER_EXTERN ir_value_t AnteaterSymbolicPacketGet(SymbolicPacket * pkt, unsigned idx, IRBuilder * builder) {
  return pkt->get(idx, builder);
}

ANTEATER_EXTERN void AnteaterSymbolicPacketInstantiateToIR(SymbolicPacket * pkt, IRBuilder * builder) {
  pkt->instantiateToIR(builder);
}

ANTEATER_EXTERN void AnteaterSymbolicPacketAppendField(unsigned length) {
  SymbolicPacket::appendField(length);
}

ANTEATER_EXTERN void AnteaterSymbolicPacketClearFields(void) {
  SymbolicPacket::clearFields();
}
