#pragma once

#include "neuronc/sema/TypeSystem.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace neuron {
namespace nir {

// Forward declarations
class Value;
class Instruction;
class Block;
class Function;
class Module;

enum class ValueKind {
  Instruction,
  Argument,
  ConstantInt,
  ConstantFloat,
  ConstantString,
  ConstantNull,
  GlobalVariable,
  Block,
  Function,
  Unknown
};

// Base class for everything that represents a value in NIR
class Value {
public:
  Value(NTypePtr type, const std::string &name = "",
        ValueKind kind = ValueKind::Unknown)
      : m_type(type), m_name(name), m_kind(kind) {}
  virtual ~Value() = default;

  NTypePtr getType() const { return m_type; }
  const std::string &getName() const { return m_name; }
  void setName(const std::string &name) { m_name = name; }
  ValueKind getValueKind() const { return m_kind; }

  virtual void print() const = 0;

protected:
  NTypePtr m_type;
  std::string m_name;
  ValueKind m_kind;
};

// Types of NIR instructions
enum class InstKind {
  // LLVM-like structure
  Add,
  Sub,
  Mul,
  Div,
  FAdd,
  FSub,
  FMul,
  FDiv,
  Pow,
  NthRoot,
  Sqrt,
  Eq,
  Neq,
  Lt,
  Lte,
  Gt,
  Gte,
  Alloca,
  Load,
  Store,
  Cast,
  Call,
  Br,
  CondBr,
  Ret,
  GpuScopeBegin,
  GpuScopeEnd,

  // Neuron specific
  TensorAdd,
  TensorSub,
  TensorMul,
  TensorDiv,
  TensorMatMul,
  TensorMatMulAdd,  // MatMul + bias
  TensorLinearFused, // MatMul + bias + residual + activation
  TensorSlice,
  TensorFMA, // Fused Multiply-Add
  FieldAccess,
};

enum class ExecutionHint {
  Auto = 0,
  GpuPrefer = 1,
};

// Represents an instruction/operation that produces a value
class Instruction : public Value {
public:
  Instruction(InstKind kind, NTypePtr type, const std::string &name = "")
      : Value(type, name, ValueKind::Instruction), m_kind(kind),
        m_executionHint(ExecutionHint::Auto) {}

  InstKind getKind() const { return m_kind; }
  ExecutionHint getExecutionHint() const { return m_executionHint; }
  void setExecutionHint(ExecutionHint hint) { m_executionHint = hint; }

  void addOperand(Value *val) { m_operands.push_back(val); }
  void setOperand(size_t i, Value *val) { m_operands[i] = val; }
  const std::vector<Value *> &getOperands() const { return m_operands; }
  Value *getOperand(size_t i) const { return m_operands[i]; }

  void print() const override;

private:
  InstKind m_kind;
  ExecutionHint m_executionHint;
  std::vector<Value *> m_operands;
};

// Constants
class ConstantInt : public Value {
public:
  ConstantInt(int64_t val)
      : Value(NType::makeInt(), std::to_string(val), ValueKind::ConstantInt),
        m_value(val) {}
  int64_t getValue() const { return m_value; }
  void print() const override;

private:
  int64_t m_value;
};

class ConstantFloat : public Value {
public:
  ConstantFloat(double val)
      : Value(NType::makeFloat(), std::to_string(val),
              ValueKind::ConstantFloat),
        m_value(val) {}
  double getValue() const { return m_value; }
  void print() const override;

private:
  double m_value;
};

class ConstantString : public Value {
public:
  ConstantString(const std::string &val)
      : Value(NType::makeString(), "", ValueKind::ConstantString),
        m_value(val) {}
  std::string getValue() const { return m_value; }
  void print() const override;

private:
  std::string m_value;
};

class ConstantNull : public Value {
public:
  ConstantNull()
      : Value(NType::makePointer(NType::makeUnknown()), "null",
              ValueKind::ConstantNull) {}
  void print() const override;
};

// Block Reference (for branch operands)
class Block; // forward
class BlockRef : public Value {
public:
  BlockRef(Block *block)
      : Value(NType::makeVoid(), block ? "blockref" : "", ValueKind::Block),
        m_block(block) {}
  Block *getBlock() const { return m_block; }
  void print() const override;

private:
  Block *m_block;
};

// Basic Block
class Block {
public:
  Block(Function *parent, const std::string &name = "")
      : m_parent(parent), m_name(name) {}

  const std::string &getName() const { return m_name; }
  void addInstruction(std::unique_ptr<Instruction> inst) {
    m_instructions.push_back(std::move(inst));
  }
  const std::vector<std::unique_ptr<Instruction>> &getInstructions() const {
    return m_instructions;
  }
  std::vector<std::unique_ptr<Instruction>> &getInstructionsMut() {
    return m_instructions;
  }

  void print() const;

private:
  Function *m_parent;
  std::string m_name;
  std::vector<std::unique_ptr<Instruction>> m_instructions;
};

// Function arguments
class Argument : public Value {
public:
  Argument(NTypePtr type, const std::string &name, Function *parent)
      : Value(type, name, ValueKind::Argument), m_parent(parent) {}
  void print() const override;

private:
  Function *m_parent;
};

// Function
class Function {
public:
  Function(const std::string &name, NTypePtr returnType, bool isExtern = false)
      : m_name(name), m_returnType(returnType), m_isExtern(isExtern) {}

  const std::string &getName() const { return m_name; }
  NTypePtr getReturnType() const { return m_returnType; }

  Argument *addArgument(NTypePtr type, const std::string &name) {
    m_arguments.push_back(std::make_unique<Argument>(type, name, this));
    return m_arguments.back().get();
  }
  const std::vector<std::unique_ptr<Argument>> &getArguments() const {
    return m_arguments;
  }

  Block *createBlock(const std::string &name = "") {
    m_blocks.push_back(std::make_unique<Block>(this, name));
    return m_blocks.back().get();
  }
  const std::vector<std::unique_ptr<Block>> &getBlocks() const {
    return m_blocks;
  }

  bool isExtern() const { return m_isExtern; }

  void print() const;

private:
  std::string m_name;
  NTypePtr m_returnType;
  bool m_isExtern = false;
  std::vector<std::unique_ptr<Argument>> m_arguments;
  std::vector<std::unique_ptr<Block>> m_blocks;
};

// Global Variable
class GlobalVariable : public Value {
public:
  GlobalVariable(NTypePtr type, const std::string &name, Value *init = nullptr)
      : Value(type, name, ValueKind::GlobalVariable), m_initializer(init) {}
  Value *getInitializer() const { return m_initializer; }
  void print() const override;

private:
  Value *m_initializer;
};

class Class {
public:
  Class(const std::string &name) : m_name(name) {}
  const std::string &getName() const { return m_name; }

  struct Field {
    std::string name;
    NTypePtr type;
  };

  void addField(const std::string &name, NTypePtr type) {
    m_fields.push_back({name, type});
  }

  const std::vector<Field> &getFields() const { return m_fields; }

  int getFieldIndex(const std::string &name) const {
    for (int i = 0; i < (int)m_fields.size(); i++) {
      if (m_fields[i].name == name)
        return i;
    }
    return -1;
  }

private:
  std::string m_name;
  std::vector<Field> m_fields;
};

enum class ShaderBindingKind {
  Unknown = 0,
  Vec4 = 1,
  Texture2D = 2,
  Sampler = 3,
  Matrix4 = 4,
};

enum ShaderVertexLayoutMask : uint32_t {
  ShaderVertexLayoutPosition = 1u << 0,
  ShaderVertexLayoutUv = 1u << 1,
  ShaderVertexLayoutNormal = 1u << 2,
};

struct ShaderBindingDesc {
  std::string name;
  ShaderBindingKind kind = ShaderBindingKind::Unknown;
  std::string typeName;
  uint32_t slot = 0;
  uint32_t descriptorBinding = UINT32_MAX;
  uint32_t uniformOffset = UINT32_MAX;
  uint32_t uniformSize = 0;
};

struct ShaderStageParamDesc {
  std::string name;
  std::string typeName;
};

struct ShaderVaryingDesc {
  std::string name;
  std::string typeName;
  uint32_t location = 0;
};

struct ShaderDesc {
  std::string name;
  bool hasVertexStage = false;
  bool hasFragmentStage = false;
  uint32_t vertexLayoutMask = 0;
  uint32_t uniformBufferSize = 0;
  uint32_t mvpOffset = UINT32_MAX;
  std::vector<ShaderBindingDesc> bindings;
  std::vector<ShaderStageParamDesc> vertexInputs;
  std::vector<ShaderVaryingDesc> varyings;
  std::string vertexGlsl;
  std::string fragmentGlsl;
};

// Module
class Module {
public:
  Module(const std::string &name) : m_name(name) {}

  const std::string &getName() const { return m_name; }

  Function *createFunction(const std::string &name, NTypePtr retType,
                           bool isExtern = false) {
    m_functions.push_back(
        std::make_unique<Function>(name, retType, isExtern));
    return m_functions.back().get();
  }
  const std::vector<std::unique_ptr<Function>> &getFunctions() const {
    return m_functions;
  }

  GlobalVariable *createGlobal(NTypePtr type, const std::string &name,
                               Value *init = nullptr) {
    m_globals.push_back(std::make_unique<GlobalVariable>(type, name, init));
    return m_globals.back().get();
  }
  const std::vector<std::unique_ptr<GlobalVariable>> &getGlobals() const {
    return m_globals;
  }

  Class *createClass(const std::string &name) {
    m_classes.push_back(std::make_unique<Class>(name));
    return m_classes.back().get();
  }
  const std::vector<std::unique_ptr<Class>> &getClasses() const {
    return m_classes;
  }

  void addShader(ShaderDesc shader) { m_shaders.push_back(std::move(shader)); }
  const std::vector<ShaderDesc> &getShaders() const { return m_shaders; }
  const ShaderDesc *findShader(const std::string &name) const {
    for (const auto &shader : m_shaders) {
      if (shader.name == name) {
        return &shader;
      }
    }
    return nullptr;
  }

  void print() const;
  std::string toString() const;

private:
  std::string m_name;
  std::vector<std::unique_ptr<Function>> m_functions;
  std::vector<std::unique_ptr<GlobalVariable>> m_globals;
  std::vector<std::unique_ptr<Class>> m_classes;
  std::vector<ShaderDesc> m_shaders;
};

} // namespace nir
} // namespace neuron
