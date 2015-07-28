#ifndef ANTEATER_TYPES_H
#define ANTEATER_TYPES_H

#include "config.h"

namespace llvm {
  class raw_ostream;
  class Constant;
  class LLVMContext;
  class Module;
  class Pass;
  class Type;
  class Value;
}

NAMESPACE_ANTEATER_BEGIN

typedef llvm::Value * ir_value_t;
typedef const llvm::Type * ir_type_t;

NAMESPACE_ANTEATER_END
#endif
