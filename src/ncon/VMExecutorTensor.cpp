#include "VMInternal.h"

namespace neuron::ncon::detail {

bool Executor::executeTensorInstruction(const Frame &frame,
                                        const InstructionRecord &instruction,
                                        Opcode opcode, VMValue *outValue,
                                        std::string *outError) {
  auto fail = [&](const std::string &message) {
    if (outError != nullptr) {
      *outError = message;
    }
    return false;
  };

  if (opcode == Opcode::TensorSlice) {
    *outValue = operandValue(frame, operandAt(instruction, 0));
    return true;
  }

  NeuronTensor *a = toTensor(operandValue(frame, operandAt(instruction, 0)));
  NeuronTensor *b = instruction.operandCount >= 2
                        ? toTensor(operandValue(frame, operandAt(instruction, 1)))
                        : nullptr;
  if (a == nullptr && opcode != Opcode::TensorFMA) {
    return fail("tensor instruction received null lhs");
  }

  NeuronTensor *result = nullptr;
  switch (opcode) {
  case Opcode::TensorAdd:
    result = neuron_tensor_add(a, b);
    break;
  case Opcode::TensorSub:
    result = neuron_tensor_sub(a, b);
    break;
  case Opcode::TensorMul:
    result = neuron_tensor_mul(a, b);
    break;
  case Opcode::TensorDiv:
    result = neuron_tensor_div(a, b);
    break;
  case Opcode::TensorFMA: {
    NeuronTensor *c = toTensor(operandValue(frame, operandAt(instruction, 2)));
    result = neuron_tensor_fma(a, b, c);
    break;
  }
  case Opcode::TensorMatMul:
    result = neuron_tensor_matmul(a, b);
    break;
  case Opcode::TensorMatMulAdd: {
    NeuronTensor *bias = toTensor(operandValue(frame, operandAt(instruction, 2)));
    result = neuron_tensor_matmul_add(a, b, bias);
    break;
  }
  case Opcode::TensorLinearFused: {
    NeuronTensor *bias = toTensor(operandValue(frame, operandAt(instruction, 2)));
    NeuronTensor *residual =
        toTensor(operandValue(frame, operandAt(instruction, 3)));
    result = neuron_tensor_linear_fused(
        a, b, nullptr, bias, residual, NEURON_TENSOR_ACTIVATION_NONE, nullptr, 0);
    break;
  }
  default:
    break;
  }

  if (result == nullptr) {
    return fail("tensor runtime returned null");
  }
  outValue->data = result;
  return true;
}

} // namespace neuron::ncon::detail

