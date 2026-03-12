#pragma once

#include "neuronc/codegen/LLVMCodeGen.h"

namespace neuron {
namespace codegen::llvm_support {

struct LLVMTypeLoweringState {
  llvm::LLVMContext *context = nullptr;
  std::unordered_map<std::string, llvm::StructType *> *structMap = nullptr;
};

llvm::Type *toLLVMType(const LLVMTypeLoweringState &state, NTypePtr type);

} // namespace codegen::llvm_support
} // namespace neuron

