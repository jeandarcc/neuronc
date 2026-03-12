#pragma once

#include "neuronc/codegen/LLVMCodeGen.h"

namespace neuron {
namespace codegen::llvm_support {

bool compileModuleToObject(llvm::Module *module, const std::string &filename,
                           const LLVMCodeGenOptions &options,
                           std::string *outError = nullptr);

} // namespace codegen::llvm_support
} // namespace neuron

