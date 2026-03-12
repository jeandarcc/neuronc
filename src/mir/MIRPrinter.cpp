#include "neuronc/mir/MIRPrinter.h"

#include <ostream>
#include <sstream>

namespace neuron::mir {

namespace {

std::string formatOperand(const Operand &operand) {
  switch (operand.kind) {
  case OperandKind::Temp: return operand.text;
  case OperandKind::Variable: return "$" + operand.text;
  case OperandKind::Literal: return operand.text;
  case OperandKind::Label: return "^" + operand.text;
  case OperandKind::None: return "_";
  }
  return "_";
}

void printOperandList(std::ostream &out, const std::vector<Operand> &operands) {
  for (std::size_t i = 0; i < operands.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << formatOperand(operands[i]);
  }
}

void printNote(std::ostream &out, const std::string &note) {
  if (!note.empty()) {
    out << " ; " << note;
  }
}

void printInstruction(std::ostream &out, const Instruction &inst) {
  switch (inst.kind) {
  case InstKind::Constant:
    out << inst.result << " = const " << formatOperand(inst.operands.front());
    break;
  case InstKind::Copy:
    out << inst.result << " = copy " << formatOperand(inst.operands.front());
    break;
  case InstKind::Move:
    out << inst.result << " = move " << formatOperand(inst.operands.front());
    break;
  case InstKind::Borrow:
    out << inst.result << " = borrow " << formatOperand(inst.operands.front());
    break;
  case InstKind::Deref:
    out << inst.result << " = deref " << formatOperand(inst.operands.front());
    break;
  case InstKind::Bind:
    out << "bind $" << inst.result << " <- " << formatOperand(inst.operands.front());
    break;
  case InstKind::Assign:
    out << "assign $" << inst.result << " <- " << formatOperand(inst.operands.front());
    break;
  case InstKind::Unary:
    out << inst.result << " = " << inst.op << " " << formatOperand(inst.operands.front());
    break;
  case InstKind::Binary:
    out << inst.result << " = " << formatOperand(inst.operands[0]) << " " << inst.op
        << " " << formatOperand(inst.operands[1]);
    break;
  case InstKind::Call:
    out << inst.result << " = call " << formatOperand(inst.callee) << "(";
    printOperandList(out, inst.operands);
    out << ")";
    break;
  case InstKind::Member:
    out << inst.result << " = member " << formatOperand(inst.operands.front()) << "."
        << inst.op;
    break;
  case InstKind::Index:
    out << inst.result << " = index " << formatOperand(inst.operands.front()) << "[";
    for (std::size_t i = 1; i < inst.operands.size(); ++i) {
      if (i != 1) {
        out << ", ";
      }
      out << formatOperand(inst.operands[i]);
    }
    out << "]";
    break;
  case InstKind::Slice:
    out << inst.result << " = slice " << formatOperand(inst.operands[0]) << "["
        << formatOperand(inst.operands[1]) << ".." << formatOperand(inst.operands[2])
        << "]";
    break;
  case InstKind::Typeof:
    out << inst.result << " = typeof " << formatOperand(inst.operands.front());
    break;
  case InstKind::Cast:
    out << inst.result << " = cast " << formatOperand(inst.operands.front()) << " as "
        << inst.op;
    break;
  case InstKind::StoreMember:
    out << "store_member " << formatOperand(inst.operands[0]) << "." << inst.op
        << " <- " << formatOperand(inst.operands[1]);
    break;
  case InstKind::StoreIndex:
    out << "store_index " << formatOperand(inst.operands.front()) << "[";
    for (std::size_t i = 1; i + 1 < inst.operands.size(); ++i) {
      if (i != 1) {
        out << ", ";
      }
      out << formatOperand(inst.operands[i]);
    }
    out << "] <- " << formatOperand(inst.operands.back());
    break;
  case InstKind::Jump:
    out << "jump " << inst.targets.front();
    break;
  case InstKind::Branch:
    out << "branch " << formatOperand(inst.operands.front()) << " ? "
        << inst.targets[0] << " : " << inst.targets[1];
    break;
  case InstKind::Return:
    out << "return";
    if (!inst.operands.empty()) {
      out << " " << formatOperand(inst.operands.front());
    }
    break;
  case InstKind::Unsupported:
    if (!inst.result.empty()) {
      out << inst.result << " = ";
    }
    out << "unsupported";
    break;
  }
  printNote(out, inst.note);
}

} // namespace

void print(std::ostream &out, const Module &module) {
  out << "module " << module.name << "\n";
  for (const auto &function : module.functions) {
    out << "\nfunc " << function.name << "(";
    for (std::size_t i = 0; i < function.parameters.size(); ++i) {
      if (i != 0) {
        out << ", ";
      }
      out << "$" << function.parameters[i];
    }
    out << ") {\n";
    for (const auto &block : function.blocks) {
      out << "  bb " << block.name << ":\n";
      if (block.instructions.empty()) {
        out << "    ; empty\n";
      }
      for (const auto &inst : block.instructions) {
        out << "    ";
        printInstruction(out, inst);
        out << "\n";
      }
      if (!block.successors.empty()) {
        out << "    successors: ";
        for (std::size_t i = 0; i < block.successors.size(); ++i) {
          if (i != 0) {
            out << ", ";
          }
          out << block.successors[i];
        }
        out << "\n";
      }
    }
    out << "}\n";
  }
}

std::string printToString(const Module &module) {
  std::ostringstream out;
  print(out, module);
  return out.str();
}

} // namespace neuron::mir
