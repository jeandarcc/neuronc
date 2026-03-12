#include "neuronc/mir/MIRBuilder.h"

#include <algorithm>

namespace neuron::mir {

std::unique_ptr<Module> MIRBuilder::build(ASTNode *root,
                                          const std::string &moduleName) {
  m_module = std::make_unique<Module>();
  m_module->name = moduleName;
  m_errors.clear();
  m_scopes.clear();
  m_loopStack.clear();
  m_nameVersions.clear();
  m_tempCounter = 0;
  m_blockCounter = 0;
  m_scopeDepth = -1;
  if (root == nullptr || root->type != ASTNodeType::Program) {
    m_errors.push_back("MIRBuilder requires a ProgramNode root.");
    return std::move(m_module);
  }
  buildProgram(static_cast<ProgramNode *>(root));
  return std::move(m_module);
}

Function &MIRBuilder::function() { return m_module->functions[m_currentFunction]; }
BasicBlock &MIRBuilder::block() { return function().blocks[m_currentBlock]; }

std::size_t MIRBuilder::createBlock(const std::string &hint) {
  std::string name = hint;
  if (name.empty()) {
    name = "bb";
  }
  if (!(hint == "entry" && function().blocks.empty())) {
    name += "_" + std::to_string(m_blockCounter++);
  }
  function().blocks.push_back(BasicBlock{name});
  return function().blocks.size() - 1;
}

void MIRBuilder::switchTo(std::size_t blockIndex) { m_currentBlock = blockIndex; }
void MIRBuilder::pushScope() {
  m_scopes.emplace_back();
  m_scopeDepth = static_cast<int>(m_scopes.size()) - 1;
}
void MIRBuilder::popScope() {
  if (!m_scopes.empty()) m_scopes.pop_back();
  m_scopeDepth = static_cast<int>(m_scopes.size()) - 1;
}

bool MIRBuilder::isDeclared(const std::string &name) const {
  for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
    if (it->find(name) != it->end()) {
      return true;
    }
  }
  return false;
}

bool MIRBuilder::isDeclaredInCurrentScope(const std::string &name) const {
  return !m_scopes.empty() && m_scopes.back().find(name) != m_scopes.back().end();
}

std::string MIRBuilder::resolveName(const std::string &name) const {
  for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
    const auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return name;
}

void MIRBuilder::registerLocal(const std::string &lowered,
                               const std::string &sourceName,
                               const SourceLocation &location,
                               bool isParameter) {
  for (const auto &local : function().locals) {
    if (local.name == lowered) {
      return;
    }
  }
  function().locals.push_back(
      LocalInfo{lowered, sourceName, location, std::max(0, m_scopeDepth),
                isParameter});
}

std::string MIRBuilder::declare(const std::string &name,
                                const SourceLocation &location,
                                bool isParameter) {
  if (m_scopes.empty()) pushScope();
  const auto existing = m_scopes.back().find(name);
  if (existing != m_scopes.back().end()) {
    return existing->second;
  }
  const int version = m_nameVersions[name]++;
  const std::string lowered =
      version == 0 ? name : name + "#" + std::to_string(version);
  m_scopes.back().emplace(name, lowered);
  registerLocal(lowered, name, location, isParameter);
  return lowered;
}

Operand MIRBuilder::nextTemp() {
  return Operand::temp("%t" + std::to_string(m_tempCounter++));
}

Operand MIRBuilder::constantTemp(const SourceLocation &location, std::string value) {
  Operand temp = nextTemp();
  emit({InstKind::Constant, location, temp.text, "", {}, {Operand::literal(std::move(value))}});
  return temp;
}

Operand MIRBuilder::copyTemp(const SourceLocation &location, const std::string &name) {
  Operand temp = nextTemp();
  emit({InstKind::Copy, location, temp.text, "", {}, {Operand::variable(name)}});
  return temp;
}

Instruction &MIRBuilder::emit(Instruction instruction) {
  block().instructions.push_back(std::move(instruction));
  return block().instructions.back();
}

void MIRBuilder::addSuccessor(std::size_t from, std::size_t to) {
  auto &succs = function().blocks[from].successors;
  const std::string &target = function().blocks[to].name;
  if (std::find(succs.begin(), succs.end(), target) == succs.end()) succs.push_back(target);
}

void MIRBuilder::emitJump(const SourceLocation &location, std::size_t target) {
  if (block().terminated) return;
  emit({InstKind::Jump, location, "", "", {}, {}, {function().blocks[target].name}});
  block().terminated = true;
  addSuccessor(m_currentBlock, target);
}

void MIRBuilder::emitBranch(const SourceLocation &location, const Operand &condition,
                            std::size_t trueTarget, std::size_t falseTarget) {
  if (block().terminated) return;
  emit({InstKind::Branch, location, "", "", {}, {condition},
        {function().blocks[trueTarget].name, function().blocks[falseTarget].name}});
  block().terminated = true;
  addSuccessor(m_currentBlock, trueTarget);
  addSuccessor(m_currentBlock, falseTarget);
}

void MIRBuilder::emitReturn(const SourceLocation &location, const Operand &value) {
  if (block().terminated) return;
  Instruction inst;
  inst.kind = InstKind::Return;
  inst.location = location;
  if (value.kind != OperandKind::None) inst.operands.push_back(value);
  emit(std::move(inst));
  block().terminated = true;
}

std::string MIRBuilder::typeText(const ASTNode *node) const {
  if (node == nullptr || node->type != ASTNodeType::TypeSpec) return "unknown";
  const auto *typeSpec = static_cast<const TypeSpecNode *>(node);
  std::string text = typeSpec->typeName;
  if (!typeSpec->genericArgs.empty()) {
    text += "<";
    for (std::size_t i = 0; i < typeSpec->genericArgs.size(); ++i) {
      if (i != 0) text += ", ";
      text += typeText(typeSpec->genericArgs[i].get());
    }
    text += ">";
  }
  return text;
}

std::string MIRBuilder::describeNode(const ASTNode *node) const {
  if (node == nullptr) return "null";
  switch (node->type) {
  case ASTNodeType::TryStmt: return "try";
  case ASTNodeType::ThrowStmt: return "throw";
  case ASTNodeType::CanvasStmt: return "canvas";
  case ASTNodeType::ShaderDecl: return "shader";
  case ASTNodeType::InputExpr: return "input";
  default: return std::to_string(static_cast<int>(node->type));
  }
}

void MIRBuilder::unsupportedStmt(const ASTNode *node, std::string reason) {
  Instruction inst;
  inst.kind = InstKind::Unsupported;
  inst.location = node != nullptr ? node->location : SourceLocation{};
  inst.note = std::move(reason);
  emit(std::move(inst));
}

Operand MIRBuilder::unsupportedExpr(const ASTNode *node, std::string reason) {
  Operand temp = nextTemp();
  Instruction inst;
  inst.kind = InstKind::Unsupported;
  inst.location = node != nullptr ? node->location : SourceLocation{};
  inst.result = temp.text;
  inst.note = std::move(reason);
  emit(std::move(inst));
  return temp;
}

void MIRBuilder::buildProgram(ProgramNode *program) {
  for (auto &decl : program->declarations) {
    if (decl == nullptr) continue;
    if (decl->type == ASTNodeType::MethodDecl) {
      buildMethod(static_cast<MethodDeclNode *>(decl.get()),
                  static_cast<MethodDeclNode *>(decl.get())->name);
    } else if (decl->type == ASTNodeType::ClassDecl) {
      auto *klass = static_cast<ClassDeclNode *>(decl.get());
      for (auto &member : klass->members) {
        if (member != nullptr && member->type == ASTNodeType::MethodDecl) {
          buildMethod(static_cast<MethodDeclNode *>(member.get()),
                      klass->name + "." + static_cast<MethodDeclNode *>(member.get())->name);
        }
      }
    }
  }
}

void MIRBuilder::buildMethod(MethodDeclNode *method, const std::string &qualifiedName) {
  m_module->functions.push_back(Function{qualifiedName});
  m_currentFunction = m_module->functions.size() - 1;
  m_scopes.clear();
  pushScope();
  function().parameters.reserve(method->parameters.size());
  for (const auto &parameter : method->parameters) {
    function().parameters.push_back(
        declare(parameter.name, parameter.location, true));
  }
  switchTo(createBlock("entry"));
  lowerBlock(method->body.get(), false);
  if (!block().terminated) emitReturn(method->location, {});
  popScope();
}

void MIRBuilder::lowerBlock(ASTNode *node, bool scoped) {
  if (node == nullptr || block().terminated) return;
  if (node->type != ASTNodeType::Block) {
    lowerStatement(node);
    return;
  }
  if (scoped) pushScope();
  for (auto &statement : static_cast<BlockNode *>(node)->statements) {
    if (statement == nullptr || block().terminated) break;
    lowerStatement(statement.get());
  }
  if (scoped) popScope();
}

} // namespace neuron::mir
