#pragma once

#include "neuronc/sema/TypeSystem.h"

#include <cstdint>
#include <string>
#include <vector>

namespace neuron::ncon {

inline constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

enum class Opcode : uint16_t {
#define X(name, value) name = value,
#include "neuronc/ncon/Opcodes.def"
#undef X
};

enum class OperandKind : uint16_t {
  Slot = 1,
  Constant = 2,
  Block = 3,
  Function = 4,
  Type = 5,
  String = 6,
  Global = 7,
};

enum class ConstantKind : uint16_t {
  Int = 1,
  Float = 2,
  String = 3,
};

struct TypeRecord {
  neuron::TypeKind kind = neuron::TypeKind::Unknown;
  uint32_t nameStringId = kInvalidIndex;
  uint32_t pointeeTypeId = kInvalidIndex;
  uint32_t returnTypeId = kInvalidIndex;
  std::vector<uint32_t> genericTypeIds;
  std::vector<uint32_t> paramTypeIds;
  std::vector<uint32_t> fieldNameStringIds;
  std::vector<uint32_t> fieldTypeIds;
};

struct ConstantRecord {
  ConstantKind kind = ConstantKind::Int;
  uint32_t typeId = kInvalidIndex;
  int64_t intValue = 0;
  double floatValue = 0.0;
  uint32_t stringId = kInvalidIndex;
};

struct GlobalRecord {
  uint32_t nameStringId = kInvalidIndex;
  uint32_t typeId = kInvalidIndex;
  uint32_t initializerConstantId = kInvalidIndex;
};

struct FunctionRecord {
  uint32_t nameStringId = kInvalidIndex;
  uint32_t returnTypeId = kInvalidIndex;
  uint32_t blockBegin = 0;
  uint32_t blockCount = 0;
  uint32_t argCount = 0;
  uint32_t slotCount = 0;
  uint32_t flags = 0;
  std::vector<uint32_t> argTypeIds;
};

struct BlockRecord {
  uint32_t nameStringId = kInvalidIndex;
  uint32_t instructionBegin = 0;
  uint32_t instructionCount = 0;
};

struct InstructionRecord {
  uint16_t opcode = 0;
  uint16_t flags = 0;
  uint32_t dstSlot = kInvalidIndex;
  uint32_t typeId = kInvalidIndex;
  uint32_t operandBegin = 0;
  uint32_t operandCount = 0;
  uint32_t imm0 = 0;
  uint32_t imm1 = 0;
};

struct OperandRecord {
  OperandKind kind = OperandKind::Slot;
  uint32_t value = 0;
};

struct Program {
  std::string moduleName;
  uint32_t entryFunctionId = kInvalidIndex;
  std::vector<std::string> strings;
  std::vector<TypeRecord> types;
  std::vector<ConstantRecord> constants;
  std::vector<GlobalRecord> globals;
  std::vector<FunctionRecord> functions;
  std::vector<BlockRecord> blocks;
  std::vector<InstructionRecord> instructions;
  std::vector<OperandRecord> operands;
};

std::string opcodeName(Opcode opcode);

} // namespace neuron::ncon
