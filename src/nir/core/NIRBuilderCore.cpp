#include "neuronc/nir/NIRBuilder.h"

#include <iostream>
#include <sstream>

namespace neuron::nir {

NIRBuilder::NIRBuilder() {
  m_scopes.emplace_back();
  m_materialShaderScopes.emplace_back();
}

void NIRBuilder::setModuleCppModules(
    const std::unordered_map<std::string, NativeModuleInfo> &modules) {
  m_moduleCppModules = modules;
}

std::unique_ptr<Module> NIRBuilder::build(ASTNode *root,
                                          const std::string &moduleName) {
  m_errors.clear();
  m_hadError = false;
  m_scopes.clear();
  m_materialShaderScopes.clear();
  m_scopes.emplace_back();
  m_materialShaderScopes.emplace_back();
  m_module = std::make_unique<Module>(moduleName);
  m_enumMembers.clear();

  if (root->type == ASTNodeType::Program) {
    visitProgram(static_cast<ProgramNode *>(root));
  } else {
    reportError(root ? root->location : SourceLocation{},
                "NIRBuilder requires a ProgramNode at the root.");
  }

  return std::move(m_module);
}

void NIRBuilder::enterScope() {
  m_scopes.emplace_back();
  m_materialShaderScopes.emplace_back();
}

void NIRBuilder::leaveScope() {
  if (m_scopes.size() > 1) {
    m_scopes.pop_back();
    m_materialShaderScopes.pop_back();
  }
}

void NIRBuilder::defineSymbol(const std::string &name, Value *val) {
  m_scopes.back()[name] = val;
}

Value *NIRBuilder::lookupSymbol(const std::string &name) {
  for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
    if (it->count(name)) {
      return (*it)[name];
    }
  }
  return nullptr;
}

void NIRBuilder::defineMaterialShader(const std::string &name,
                                      std::string shaderName) {
  if (name.empty() || shaderName.empty()) {
    return;
  }
  m_materialShaderScopes.back()[name] = std::move(shaderName);
}

std::string NIRBuilder::lookupMaterialShader(const std::string &name) const {
  if (name.empty()) {
    return {};
  }
  for (auto it = m_materialShaderScopes.rbegin();
       it != m_materialShaderScopes.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return {};
}

std::string NIRBuilder::nextValName() {
  return std::to_string(m_valDefCounter++);
}

std::string NIRBuilder::nextBlockName() {
  return "bb" + std::to_string(m_blockCounter++);
}

Instruction *NIRBuilder::createInst(InstKind kind, NTypePtr type,
                                    const std::string &name) {
  std::string instName =
      name.empty() && type->kind != TypeKind::Void ? nextValName() : name;
  auto inst = std::make_unique<Instruction>(kind, type, instName);
  Instruction *ptr = inst.get();
  applyExecutionHint(ptr);
  insertInst(std::move(inst));
  return ptr;
}

void NIRBuilder::insertInst(std::unique_ptr<Instruction> inst) {
  if (m_currentBlock) {
    m_currentBlock->addInstruction(std::move(inst));
  }
}

void NIRBuilder::reportError(const SourceLocation &location,
                             const std::string &message) {
  m_hadError = true;
  std::ostringstream oss;
  if (!location.file.empty()) {
    oss << location.file << ":" << location.line << ":" << location.column
        << ": error: ";
  }
  oss << message;
  m_errors.push_back(oss.str());
  std::cerr << "NIRBuilder: " << oss.str() << std::endl;
}

} // namespace neuron::nir

