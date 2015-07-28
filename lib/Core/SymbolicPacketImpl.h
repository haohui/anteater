#ifndef ANTEATER_LIB_CORE_SYMBOLIC_PACKET_IMPL_H
#define ANTEATER_LIB_CORE_SYMBOLIC_PACKET_IMPL_H

#include "anteater/Core/SymbolicPacket.h"

#include <boost/utility.hpp>
#include <vector>

namespace llvm {
class StructType;
class Module;
}

NAMESPACE_ANTEATER_BEGIN

class SymbolicPacketImpl {
public:
  ir_value_t get(unsigned, IRBuilder *) const;
  void instantiateToIR(IRBuilder *);
  static void appendField(unsigned length);
  static void clearFields();   

  ir_value_t m_handle;

  class Type : boost::noncopyable {
   public:
    typedef std::vector<unsigned> FieldMap;
    static Type & instance() {
      static Type inst;
      return inst;
    }
    
    void appendField(unsigned length);
    void clearFields();
    void generateIdenticalTransformationFunctionBody(ir_value_t);
    llvm::StructType * createPacketType(llvm::Module * M);
    
    FieldMap m_fields;
    bool m_freezed;
    llvm::StructType * m_type;
   private:
    Type();
  };
};

NAMESPACE_ANTEATER_END

#endif
