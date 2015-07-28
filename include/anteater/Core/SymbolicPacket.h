#ifndef ANTEATER_CORE_SYMBOLIC_PACKET_H
#define ANTEATER_CORE_SYMBOLIC_PACKET_H

#include "anteater/types.h"

NAMESPACE_ANTEATER_BEGIN

class SymbolicPacketImpl;
class IRBuilder;
class SymbolicPacket {
 public:
  SymbolicPacket();
  ~SymbolicPacket();
  ir_value_t get(unsigned, IRBuilder *) const;
  
  //
  // Generate a global variable in the IR, before referencing the packet
  //
  void instantiateToIR(IRBuilder *);
  
  //
  // Add a new field into symbolic packet during initialization
  //
  static void appendField(unsigned length);
  static void clearFields();

  SymbolicPacketImpl * impl() const { return m_impl; }
 private:
  SymbolicPacketImpl * m_impl;
};

NAMESPACE_ANTEATER_END

#endif
