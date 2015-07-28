/*-*- mode: c++; -*- **/

#ifndef ANTEATER_TARGET_BACKEND_H
#define ANTEATER_TARGET_BACKEND_H

#include "anteater/types.h"

NAMESPACE_ANTEATER_BEGIN

llvm::Pass * createXorEliminationPass(llvm::LLVMContext * context);
llvm::Pass * createYicesWriter(llvm::raw_ostream * out);
llvm::Pass * createSMT12Writer(llvm::raw_ostream * out);
llvm::Pass * createAnteaterInstructionNamerPass();

NAMESPACE_ANTEATER_END

#endif
