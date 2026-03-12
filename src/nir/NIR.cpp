#include "neuronc/nir/NIR.h"

#include <iostream>
#include <ostream>
#include <sstream>

namespace neuron {
namespace nir {

namespace {

const char *instructionMnemonic(InstKind kind) {
  switch (kind) {
  case InstKind::Add:
    return "add";
  case InstKind::Sub:
    return "sub";
  case InstKind::Mul:
    return "mul";
  case InstKind::Div:
    return "div";
  case InstKind::FAdd:
    return "fadd";
  case InstKind::FSub:
    return "fsub";
  case InstKind::FMul:
    return "fmul";
  case InstKind::FDiv:
    return "fdiv";
  case InstKind::Pow:
    return "pow";
  case InstKind::NthRoot:
    return "nthroot";
  case InstKind::Sqrt:
    return "sqrt";
  case InstKind::Eq:
    return "eq";
  case InstKind::Neq:
    return "neq";
  case InstKind::Lt:
    return "lt";
  case InstKind::Lte:
    return "lte";
  case InstKind::Gt:
    return "gt";
  case InstKind::Gte:
    return "gte";
  case InstKind::Alloca:
    return "alloca";
  case InstKind::Load:
    return "load";
  case InstKind::Store:
    return "store";
  case InstKind::Cast:
    return "cast";
  case InstKind::Call:
    return "call";
  case InstKind::Br:
    return "br";
  case InstKind::CondBr:
    return "condbr";
  case InstKind::Ret:
    return "ret";
  case InstKind::GpuScopeBegin:
    return "gpu_scope_begin";
  case InstKind::GpuScopeEnd:
    return "gpu_scope_end";
  case InstKind::TensorAdd:
    return "tensor_add";
  case InstKind::TensorSub:
    return "tensor_sub";
  case InstKind::TensorMul:
    return "tensor_mul";
  case InstKind::TensorDiv:
    return "tensor_div";
  case InstKind::TensorMatMul:
    return "tensor_matmul";
  case InstKind::TensorMatMulAdd:
    return "tensor_matmul_add";
  case InstKind::TensorLinearFused:
    return "tensor_linear_fused";
  case InstKind::TensorSlice:
    return "tensor_slice";
  case InstKind::TensorFMA:
    return "tensor_fma";
  case InstKind::FieldAccess:
    return "field_access";
  }
  return "inst";
}

void renderValue(std::ostream &out, const Value *value) {
  if (value == nullptr) {
    out << "<null>";
    return;
  }

  switch (value->getValueKind()) {
  case ValueKind::Argument:
  case ValueKind::Instruction:
    out << "%" << value->getName();
    return;
  case ValueKind::ConstantInt:
    out << static_cast<const ConstantInt *>(value)->getValue();
    return;
  case ValueKind::ConstantFloat:
    out << static_cast<const ConstantFloat *>(value)->getValue();
    return;
  case ValueKind::ConstantString:
    out << "\"" << static_cast<const ConstantString *>(value)->getValue() << "\"";
    return;
  case ValueKind::ConstantNull:
    out << "null";
    return;
  case ValueKind::Block: {
    const auto *blockRef = static_cast<const BlockRef *>(value);
    if (blockRef->getBlock() != nullptr) {
      out << "%" << blockRef->getBlock()->getName();
    } else {
      out << "%<null_block>";
    }
    return;
  }
  default:
    out << value->getName();
    return;
  }
}

void renderInstruction(std::ostream &out, const Instruction &inst) {
  if (inst.getType() && inst.getType()->kind != TypeKind::Void) {
    out << "  %" << inst.getName() << " = ";
  } else {
    out << "  ";
  }

  out << instructionMnemonic(inst.getKind()) << " ";
  if (inst.getType()) {
    out << inst.getType()->toString() << " ";
  }

  const auto &operands = inst.getOperands();
  for (std::size_t index = 0; index < operands.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    renderValue(out, operands[index]);
  }
  out << '\n';
}

void renderArgument(std::ostream &out, const Argument &argument) {
  out << argument.getType()->toString() << " %" << argument.getName();
}

void renderBlock(std::ostream &out, const Block &block) {
  out << block.getName() << ":\n";
  for (const auto &inst : block.getInstructions()) {
    renderInstruction(out, *inst);
  }
}

void renderFunction(std::ostream &out, const Function &function) {
  out << "define "
      << (function.getReturnType() ? function.getReturnType()->toString() : "void")
      << " @" << function.getName() << "(";
  const auto &arguments = function.getArguments();
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    if (index > 0) {
      out << ", ";
    }
    renderArgument(out, *arguments[index]);
  }
  out << ") {\n";
  for (const auto &block : function.getBlocks()) {
    renderBlock(out, *block);
  }
  out << "}\n";
}

void renderGlobal(std::ostream &out, const GlobalVariable &global) {
  out << "@" << global.getName() << " = global " << global.getType()->toString();
  if (global.getInitializer() != nullptr) {
    out << " ";
    renderValue(out, global.getInitializer());
  }
  out << '\n';
}

std::string renderModule(const Module &module) {
  std::ostringstream out;
  out << "; ModuleID = '" << module.getName() << "'\n";
  for (const auto &shader : module.getShaders()) {
    out << "; shader " << shader.name << " stages=";
    out << (shader.hasVertexStage ? "V" : "-");
    out << (shader.hasFragmentStage ? "F" : "-");
    if (!shader.bindings.empty()) {
      out << " bindings=";
      for (std::size_t i = 0; i < shader.bindings.size(); ++i) {
        if (i > 0) {
          out << ",";
        }
        out << shader.bindings[i].name << ":" << shader.bindings[i].typeName;
      }
    }
    out << '\n';
  }
  if (!module.getShaders().empty()) {
    out << '\n';
  }
  for (const auto &global : module.getGlobals()) {
    renderGlobal(out, *global);
  }
  if (!module.getGlobals().empty()) {
    out << '\n';
  }

  for (const auto &function : module.getFunctions()) {
    renderFunction(out, *function);
    out << '\n';
  }
  return out.str();
}

} // namespace

void Instruction::print() const {
  renderInstruction(std::cout, *this);
}

void ConstantInt::print() const {
  std::cout << m_value;
}

void ConstantFloat::print() const {
  std::cout << m_value;
}

void ConstantString::print() const {
  std::cout << "\"" << m_value << "\"";
}

void ConstantNull::print() const {
  std::cout << "null";
}

void BlockRef::print() const {
  if (m_block) {
    std::cout << "%" << m_block->getName();
  } else {
    std::cout << "%<null_block>";
  }
}

void Argument::print() const {
  renderArgument(std::cout, *this);
}

void Block::print() const {
  renderBlock(std::cout, *this);
}

void Function::print() const {
  renderFunction(std::cout, *this);
}

void GlobalVariable::print() const {
  renderGlobal(std::cout, *this);
}

void Module::print() const {
  std::cout << toString();
}

std::string Module::toString() const {
  return renderModule(*this);
}

} // namespace nir
} // namespace neuron
