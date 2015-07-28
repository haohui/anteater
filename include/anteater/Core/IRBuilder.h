/** -*- mode: c++ -*-
 * Thin wrapper for LLVM's IRBuilder
 **/

#ifndef ANTEATER_CORE_IRBUILDER_H
#define ANTEATER_CORE_IRBUILDER_H

#include "anteater/config.h"
#include "anteater/types.h"

NAMESPACE_ANTEATER_BEGIN

struct IRBuilderImpl;
class SymbolicPacket;

class IRBuilder {
public:
  static const char * const kAssertionFunctionName;
  static const char * const kMainFunctionName;
  static const char * const kIdTransformationFunctionName;
  static const char * const kTrivialTrueFunctionName;
  static const char * const kPreconditionFunctionName;
  static const char * const kFilterFunctionName;
  static const char * const kRecordTransformationFunctionName;
  static const char * const kEdgeFunctionPrefix;

  IRBuilder();
  ~IRBuilder();
  
  void createMainFunction();
  void resetMainFunction();
  void deleteConstraintFunctionBody();
  
  ir_value_t createAndExpr(ir_value_t LHS, ir_value_t RHS);
  ir_value_t createOrExpr(ir_value_t LHS, ir_value_t RHS);
  ir_value_t createBitwiseAndExpr(ir_value_t LHS, ir_value_t RHS);
  ir_value_t createBitwiseEqualExpr(ir_value_t LHS, ir_value_t RHS);
  ir_value_t createBitwiseNeqExpr(ir_value_t LHS, ir_value_t RHS);
  ir_value_t createXorExpr(ir_value_t LHS, ir_value_t RHS); 
  ir_value_t createAssertion(ir_value_t v);
  ir_value_t createPrecondition(ir_value_t v);
  ir_value_t createNotExpr(ir_value_t v);
  ir_value_t createImpliesExpr(ir_value_t v1, ir_value_t v2);
  ir_value_t createLoad(ir_value_t v);
  ir_value_t createStructGEP(ir_value_t v, unsigned index);
  void createRet(ir_value_t v);
  
  // Functions over an network edge, could be either a function of constraints
  // or functions for transformation
  ir_value_t createEdgeFunction(SymbolicPacket *, SymbolicPacket *);
  ir_value_t createCallEdgeFunction(ir_value_t func, const SymbolicPacket *, const SymbolicPacket *);
  ir_value_t createCallFilterFunction();
  static bool isCallEdgeInst(ir_value_t inst);
  ir_value_t recordTransformationConstraint(unsigned hop, const SymbolicPacket * sym_packet_in, const SymbolicPacket * sym_packet_out, ir_value_t constraint);
  
  void checkPoint();
  void rewindToCheckPoint();
  
  ir_value_t getConstantInt(long value, unsigned length);
  ir_value_t getConstantBitVector(long value, unsigned length);
  ir_value_t getConstantBool(bool value);
  ir_value_t createVariable(ir_type_t ty, const char * name);
  ir_type_t  getBitVectorType(unsigned bits);
  ir_type_t  getPacketType() const;
  ir_value_t getIdenticalTransformationFunction() const;
  ir_value_t getTrivialTrueFunction() const;
  
  int dumpModule(const char * filename) const;
  
private:
  IRBuilderImpl * m_impl;
};

NAMESPACE_ANTEATER_END

#endif
