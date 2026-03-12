#pragma once

#include "neuronc/lexer/Token.h"

#include <string>
#include <vector>

namespace neuron::mir {

enum class OperandKind {
  Temp,
  Variable,
  Literal,
  Label,
  None,
};

struct Operand {
  OperandKind kind = OperandKind::None;
  std::string text;

  static Operand temp(std::string name) {
    return Operand{OperandKind::Temp, std::move(name)};
  }
  static Operand variable(std::string name) {
    return Operand{OperandKind::Variable, std::move(name)};
  }
  static Operand literal(std::string text) {
    return Operand{OperandKind::Literal, std::move(text)};
  }
  static Operand label(std::string text) {
    return Operand{OperandKind::Label, std::move(text)};
  }
};

enum class InstKind {
  Constant,
  Copy,
  Move,
  Borrow,
  Deref,
  Bind,
  Assign,
  Unary,
  Binary,
  Call,
  Member,
  Index,
  Slice,
  Typeof,
  Cast,
  StoreMember,
  StoreIndex,
  Jump,
  Branch,
  Return,
  Unsupported,
};

struct Instruction {
  InstKind kind = InstKind::Unsupported;
  SourceLocation location;
  std::string result;
  std::string op;
  Operand callee;
  std::vector<Operand> operands;
  std::vector<std::string> targets;
  std::string note;
};

struct BasicBlock {
  std::string name;
  std::vector<Instruction> instructions;
  std::vector<std::string> successors;
  bool terminated = false;
};

struct LocalInfo {
  std::string name;
  std::string sourceName;
  SourceLocation location;
  int scopeDepth = 0;
  bool isParameter = false;
};

struct Function {
  std::string name;
  std::vector<std::string> parameters;
  std::vector<LocalInfo> locals;
  std::vector<BasicBlock> blocks;
};

struct Module {
  std::string name;
  std::vector<Function> functions;
};

} // namespace neuron::mir
