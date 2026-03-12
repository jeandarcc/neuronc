#include "VMInternal.h"

namespace neuron::ncon::detail {

ExecutionStatus Executor::executeFunction(
    uint32_t functionId, const std::vector<VMValue> &args, VMValue *outValue,
    const std::function<std::optional<HotReloadCommand>()> *poller,
    std::string *outError) {
  if (functionId == kInvalidIndex || functionId >= program().functions.size()) {
    if (outError != nullptr) {
      *outError = "invalid ncon function id";
    }
    return ExecutionStatus::Failed;
  }

  const FunctionRecord &function = program().functions[functionId];
  Frame frame;
  frame.functionId = functionId;
  frame.slots.resize(function.slotCount);
  for (size_t i = 0; i < args.size() && i < function.argTypeIds.size(); ++i) {
    frame.slots[i] = coerce(args[i], function.argTypeIds[i]);
  }

  uint32_t ip = function.blockCount == 0
                    ? 0
                    : program().blocks[function.blockBegin].instructionBegin;

  while (ip < program().instructions.size()) {
    if (!m_pendingPatch.has_value() && poller != nullptr && *poller) {
      m_pendingPatch = (*poller)();
    }
    if (m_pendingPatch.has_value()) {
      return ExecutionStatus::PatchRequested;
    }

    const InstructionRecord &instruction = program().instructions[ip];
    const Opcode opcode = static_cast<Opcode>(instruction.opcode);

    auto assign = [&](VMValue value) {
      if (instruction.dstSlot != kInvalidIndex &&
          instruction.dstSlot < frame.slots.size()) {
        frame.slots[instruction.dstSlot] = std::move(value);
      }
    };

    switch (opcode) {
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Div:
    case Opcode::Eq:
    case Opcode::Neq:
    case Opcode::Lt:
    case Opcode::Lte:
    case Opcode::Gt:
    case Opcode::Gte: {
      const VMValue lhs = operandValue(frame, operandAt(instruction, 0));
      const VMValue rhs = operandValue(frame, operandAt(instruction, 1));
      const TypeRecord *type = typeAt(instruction.typeId);
      VMValue result;
      const bool useFloat =
          (type != nullptr &&
           (type->kind == TypeKind::Float || type->kind == TypeKind::Double)) ||
          isFloatingValue(lhs) || isFloatingValue(rhs);
      if (opcode == Opcode::Eq || opcode == Opcode::Neq) {
        const bool equal = toString(lhs) == toString(rhs);
        result.data = int64_t{(opcode == Opcode::Eq) ? equal : !equal};
      } else if (opcode == Opcode::Lt || opcode == Opcode::Lte ||
                 opcode == Opcode::Gt || opcode == Opcode::Gte) {
        const double a = toDouble(lhs);
        const double b = toDouble(rhs);
        bool comparison = false;
        if (opcode == Opcode::Lt) {
          comparison = a < b;
        } else if (opcode == Opcode::Lte) {
          comparison = a <= b;
        } else if (opcode == Opcode::Gt) {
          comparison = a > b;
        } else {
          comparison = a >= b;
        }
        result.data = int64_t{comparison};
      } else if (opcode == Opcode::Add &&
                 ((type != nullptr && type->kind == TypeKind::String) ||
                  isStringValue(lhs) || isStringValue(rhs))) {
        result.data = toString(lhs) + toString(rhs);
      } else if (useFloat) {
        const double a = toDouble(lhs);
        const double b = toDouble(rhs);
        if (opcode == Opcode::Add) {
          result.data = a + b;
        } else if (opcode == Opcode::Sub) {
          result.data = a - b;
        } else if (opcode == Opcode::Mul) {
          result.data = a * b;
        } else {
          result.data = b == 0.0 ? 0.0 : a / b;
        }
      } else {
        const int64_t a = toInt(lhs);
        const int64_t b = toInt(rhs);
        if (opcode == Opcode::Add) {
          result.data = a + b;
        } else if (opcode == Opcode::Sub) {
          result.data = a - b;
        } else if (opcode == Opcode::Mul) {
          result.data = a * b;
        } else {
          result.data = b == 0 ? int64_t{0} : a / b;
        }
      }
      assign(result);
      break;
    }
    case Opcode::Alloca: {
      auto cell = std::make_shared<Cell>();
      const TypeRecord *type = typeAt(instruction.typeId);
      cell->typeId = type != nullptr ? type->pointeeTypeId : kInvalidIndex;
      cell->value = defaultValue(cell->typeId);
      VMValue result;
      result.data = cell;
      assign(result);
      break;
    }
    case Opcode::Load: {
      const PointerHandle cell =
          toPointer(operandValue(frame, operandAt(instruction, 0)));
      if (!cell) {
        if (outError != nullptr) {
          *outError = "ncon load from null pointer";
        }
        return ExecutionStatus::Failed;
      }
      assign(cell->value);
      break;
    }
    case Opcode::Store: {
      VMValue source = operandValue(frame, operandAt(instruction, 0));
      const PointerHandle cell =
          toPointer(operandValue(frame, operandAt(instruction, 1)));
      if (!cell) {
        if (outError != nullptr) {
          *outError = "ncon store to null pointer";
        }
        return ExecutionStatus::Failed;
      }
      cell->value = coerce(source, cell->typeId);
      break;
    }
    case Opcode::Cast: {
      VMValue result;
      if (!tryCastValue(operandValue(frame, operandAt(instruction, 0)),
                        instruction.typeId, &result, outError)) {
        return ExecutionStatus::Failed;
      }
      assign(result);
      break;
    }
    case Opcode::FieldAccess: {
      const PointerHandle objectCell =
          toPointer(operandValue(frame, operandAt(instruction, 0)));
      const int64_t fieldIndex =
          toInt(operandValue(frame, operandAt(instruction, 1)));
      if (!objectCell) {
        if (outError != nullptr) {
          *outError = "ncon field access on null object pointer";
        }
        return ExecutionStatus::Failed;
      }
      auto object = std::get_if<ClassObjectHandle>(&objectCell->value.data);
      if (object == nullptr || !*object || fieldIndex < 0 ||
          static_cast<size_t>(fieldIndex) >= (*object)->fields.size()) {
        if (outError != nullptr) {
          *outError = "ncon field access out of range";
        }
        return ExecutionStatus::Failed;
      }
      VMValue result;
      result.data = (*object)->fields[static_cast<size_t>(fieldIndex)];
      assign(result);
      break;
    }
    case Opcode::Call: {
      if (instruction.operandCount == 0) {
        if (outError != nullptr) {
          *outError = "ncon call instruction missing callee";
        }
        return ExecutionStatus::Failed;
      }
      const VMValue calleeValue = operandValue(frame, operandAt(instruction, 0));
      const std::string callee = toString(calleeValue);
      std::vector<VMValue> callArgs;
      for (uint32_t i = 1; i < instruction.operandCount; ++i) {
        callArgs.push_back(operandValue(frame, operandAt(instruction, i)));
      }

      VMValue result;
      if (callee == "thread") {
        if (!callArgs.empty()) {
          const std::string target = toString(callArgs[0]);
          auto fnIt = m_functionIdsByName.find(target);
          if (fnIt != m_functionIdsByName.end()) {
            std::vector<VMValue> threadArgs;
            const FunctionRecord &targetFn = program().functions[fnIt->second];
            if (targetFn.argCount == 1) {
              VMValue nullPtr;
              nullPtr.data = PointerHandle{};
              threadArgs.push_back(nullPtr);
            }
            const ExecutionStatus threadStatus =
                executeFunction(fnIt->second, threadArgs, nullptr, poller, outError);
            if (threadStatus != ExecutionStatus::Ok) {
              return threadStatus;
            }
          }
        }
        result.data = int64_t{0};
        assign(result);
        break;
      }

      if (m_runtime->isBuiltin(callee)) {
        if (!m_runtime->invokeBuiltin(program(), callee, callArgs, &result,
                                      outError)) {
          return ExecutionStatus::Failed;
        }
        assign(result);
        break;
      }
      if (m_runtime->isNativeCall(callee)) {
        if (!m_runtime->invokeNative(callee, callArgs, &result, outError)) {
          return ExecutionStatus::Failed;
        }
        assign(result);
        break;
      }

      auto fnIt = m_functionIdsByName.find(callee);
      if (fnIt == m_functionIdsByName.end()) {
        if (outError != nullptr && outError->empty()) {
          *outError = "ncon call target not found: " + callee;
        }
        return ExecutionStatus::Failed;
      }
      const ExecutionStatus callStatus =
          executeFunction(fnIt->second, callArgs, &result, poller, outError);
      if (callStatus != ExecutionStatus::Ok) {
        return callStatus;
      }
      assign(result);
      break;
    }
    case Opcode::Br: {
      const uint32_t blockId = operandAt(instruction, 0).value;
      ip = program().blocks[blockId].instructionBegin;
      continue;
    }
    case Opcode::CondBr: {
      const bool condition = truthy(operandValue(frame, operandAt(instruction, 0)));
      const uint32_t trueBlock = operandAt(instruction, 1).value;
      const uint32_t falseBlock = operandAt(instruction, 2).value;
      ip = program().blocks[condition ? trueBlock : falseBlock].instructionBegin;
      continue;
    }
    case Opcode::Ret: {
      if (outValue != nullptr) {
        *outValue = instruction.operandCount == 0
                        ? defaultValue(instruction.typeId)
                        : operandValue(frame, operandAt(instruction, 0));
      }
      return ExecutionStatus::Ok;
    }
    case Opcode::TensorAdd:
    case Opcode::TensorSub:
    case Opcode::TensorMul:
    case Opcode::TensorDiv:
    case Opcode::TensorFMA:
    case Opcode::TensorMatMul:
    case Opcode::TensorMatMulAdd:
    case Opcode::TensorLinearFused:
    case Opcode::TensorSlice: {
      VMValue result;
      if (!executeTensorInstruction(frame, instruction, opcode, &result, outError)) {
        return ExecutionStatus::Failed;
      }
      assign(result);
      break;
    }
    default:
      if (outError != nullptr) {
        *outError = "unsupported opcode at runtime: " + opcodeName(opcode);
      }
      return ExecutionStatus::Failed;
    }

    ++ip;
  }

  if (outValue != nullptr) {
    *outValue = defaultValue(program().functions[functionId].returnTypeId);
  }
  return ExecutionStatus::Ok;
}

} // namespace neuron::ncon::detail

