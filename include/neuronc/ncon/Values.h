#pragma once

#include "neuron_tensor.h"

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace neuron::ncon {

struct Cell;
struct ClassObject;

using PointerHandle = std::shared_ptr<Cell>;
using ClassObjectHandle = std::shared_ptr<ClassObject>;
using TensorHandle = NeuronTensor *;
using ArrayIntHandle = std::shared_ptr<std::vector<int64_t>>;

struct VMValue {
  std::variant<std::monostate, int64_t, double, std::string, PointerHandle,
               ClassObjectHandle, TensorHandle, ArrayIntHandle>
      data;
};

struct Cell {
  uint32_t typeId = 0xFFFFFFFFu;
  VMValue value;
};

struct ClassObject {
  std::vector<PointerHandle> fields;
};

} // namespace neuron::ncon
