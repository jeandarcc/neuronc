#pragma once

#include "neuronc/nir/Optimizer.h"

#include <unordered_map>

namespace neuron::nir::detail {

void replaceOperands(Instruction *inst,
                     const std::unordered_map<Value *, Value *> &replacements,
                     bool *changed);

bool isAlloca(Value *value);
bool isZero(Value *value);
bool isOne(Value *value);

} // namespace neuron::nir::detail

