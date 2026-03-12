#include "OptimizerInternal.h"

namespace neuron::nir::detail {

void replaceOperands(Instruction *inst,
                     const std::unordered_map<Value *, Value *> &replacements,
                     bool *changed) {
  if (inst == nullptr) {
    return;
  }
  for (size_t i = 0; i < inst->getOperands().size(); ++i) {
    Value *op = inst->getOperand(i);
    auto it = replacements.find(op);
    if (it != replacements.end() && it->second != nullptr && it->second != op) {
      inst->setOperand(i, it->second);
      if (changed != nullptr) {
        *changed = true;
      }
    }
  }
}

bool isAlloca(Value *value) {
  auto *inst = dynamic_cast<Instruction *>(value);
  return inst != nullptr && inst->getKind() == InstKind::Alloca;
}

bool isZero(Value *value) {
  if (auto *ci = dynamic_cast<ConstantInt *>(value)) {
    return ci->getValue() == 0;
  }
  if (auto *cf = dynamic_cast<ConstantFloat *>(value)) {
    return cf->getValue() == 0.0;
  }
  return false;
}

bool isOne(Value *value) {
  if (auto *ci = dynamic_cast<ConstantInt *>(value)) {
    return ci->getValue() == 1;
  }
  if (auto *cf = dynamic_cast<ConstantFloat *>(value)) {
    return cf->getValue() == 1.0;
  }
  return false;
}

} // namespace neuron::nir::detail

