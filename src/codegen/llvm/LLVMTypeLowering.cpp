#include "neuronc/codegen/llvm/LLVMTypeLowering.h"

#include <iostream>

namespace neuron {
namespace codegen::llvm_support {

llvm::Type *toLLVMType(const LLVMTypeLoweringState &state, NTypePtr type) {
  if (!type) {
    return llvm::Type::getVoidTy(*state.context);
  }

  switch (type->kind) {
  case TypeKind::Void:
    return llvm::Type::getVoidTy(*state.context);
  case TypeKind::Int:
    return llvm::Type::getInt64Ty(*state.context);
  case TypeKind::Float:
    return llvm::Type::getFloatTy(*state.context);
  case TypeKind::Double:
    return llvm::Type::getDoubleTy(*state.context);
  case TypeKind::Bool:
    return llvm::Type::getInt1Ty(*state.context);
  case TypeKind::String:
  case TypeKind::Dynamic:
  case TypeKind::Dictionary:
    return llvm::PointerType::get(*state.context, 0);
  case TypeKind::Enum:
    return llvm::Type::getInt64Ty(*state.context);
  case TypeKind::Pointer:
  case TypeKind::Tensor:
  case TypeKind::Descriptor:
    return llvm::PointerType::get(*state.context, 0);
  case TypeKind::Class:
    if (state.structMap != nullptr && state.structMap->count(type->className)) {
      return (*state.structMap)[type->className];
    }
    std::cerr << "LLVMCodeGen: Class type not found in struct map: "
              << type->className << std::endl;
    return llvm::Type::getInt64Ty(*state.context);
  default:
    return llvm::Type::getInt64Ty(*state.context);
  }
}

} // namespace codegen::llvm_support
} // namespace neuron

