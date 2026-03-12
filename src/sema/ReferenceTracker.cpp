#include "ReferenceTracker.h"

#include <algorithm>

namespace neuron::sema_detail {

void ReferenceTracker::reset() {
  m_inferredTypes.clear();
  m_documentDeclarations.clear();
  m_symbolIndex.clear();
}

NTypePtr ReferenceTracker::rememberType(const ASTNode *node, NTypePtr type) {
  if (node != nullptr && type != nullptr) {
    m_inferredTypes[node] = type;
  }
  return type;
}

NTypePtr ReferenceTracker::inferredType(const ASTNode *node) const {
  auto it = m_inferredTypes.find(node);
  if (it == m_inferredTypes.end()) {
    return nullptr;
  }
  return it->second;
}

void ReferenceTracker::setDocumentDeclarations(
    const std::vector<ASTNode *> &declarations) {
  m_documentDeclarations = declarations;
}

const std::vector<ASTNode *> &ReferenceTracker::documentDeclarations() const {
  return m_documentDeclarations;
}

void ReferenceTracker::recordDefinition(Symbol *symbol, const SourceLocation &loc,
                                        int length) {
  if (symbol == nullptr || loc.file.empty()) {
    return;
  }
  SymbolLocation definition;
  definition.location = loc;
  definition.length = std::max(1, length);
  symbol->definition = definition;
  m_symbolIndex[makeLocationKey(loc)] = symbol;
}

void ReferenceTracker::recordReference(Symbol *symbol, const SourceLocation &loc,
                                       int length) {
  if (symbol == nullptr || loc.file.empty()) {
    return;
  }

  const std::string key = makeLocationKey(loc);
  auto existing = m_symbolIndex.find(key);
  if (existing != m_symbolIndex.end()) {
    return;
  }

  SymbolLocation reference;
  reference.location = loc;
  reference.length = std::max(1, length);
  symbol->references.push_back(reference);
  m_symbolIndex.emplace(key, symbol);
}

std::optional<SymbolLocation>
ReferenceTracker::definitionLocation(const SourceLocation &referenceLocation) const {
  const Symbol *symbol = findSymbol(referenceLocation);
  if (symbol == nullptr || !symbol->definition.has_value()) {
    return std::nullopt;
  }
  return symbol->definition;
}

std::vector<SymbolLocation>
ReferenceTracker::referenceLocations(const SourceLocation &referenceLocation) const {
  const Symbol *symbol = findSymbol(referenceLocation);
  if (symbol == nullptr) {
    return {};
  }
  return symbol->references;
}

std::vector<DocumentSymbolInfo> ReferenceTracker::documentSymbols() const {
  std::vector<DocumentSymbolInfo> symbols;
  symbols.reserve(m_documentDeclarations.size());
  for (const ASTNode *decl : m_documentDeclarations) {
    if (auto symbol = buildDocumentSymbol(decl)) {
      symbols.push_back(std::move(*symbol));
    }
  }
  return symbols;
}

std::optional<VisibleSymbolInfo>
ReferenceTracker::resolvedSymbol(const SourceLocation &location) const {
  const Symbol *symbol = findSymbol(location);
  if (symbol == nullptr) {
    return std::nullopt;
  }

  VisibleSymbolInfo info;
  info.name = symbol->name;
  info.kind = symbol->kind;
  info.type = symbol->type;
  info.signatureKey = symbol->signatureKey;
  info.isPublic = symbol->isPublic;
  info.isConst = symbol->isConst;
  info.definition = symbol->definition;
  return info;
}

std::string ReferenceTracker::makeLocationKey(const SourceLocation &loc) {
  return loc.file + ":" + std::to_string(loc.line) + ":" +
         std::to_string(loc.column);
}

SymbolRange ReferenceTracker::makePointRange(const SourceLocation &loc,
                                             int length) {
  SymbolRange range;
  range.start = loc;
  range.end = loc;
  range.end.column = loc.column + std::max(1, length);
  return range;
}

const Symbol *ReferenceTracker::findSymbol(const SourceLocation &loc) const {
  auto it = m_symbolIndex.find(makeLocationKey(loc));
  if (it == m_symbolIndex.end()) {
    return nullptr;
  }
  return it->second;
}

std::optional<DocumentSymbolInfo>
ReferenceTracker::buildDocumentSymbol(const ASTNode *node,
                                      bool classMember) const {
  if (node == nullptr) {
    return std::nullopt;
  }

  const auto makeRange = [](const SourceLocation &loc,
                            std::string_view name) -> SymbolRange {
    return makePointRange(loc, static_cast<int>(name.size()));
  };

  DocumentSymbolInfo info;
  switch (node->type) {
  case ASTNodeType::ClassDecl: {
    const auto *classDecl = static_cast<const ClassDeclNode *>(node);
    info.name = classDecl->name;
    info.kind = SymbolKind::Class;
    info.range = makeRange(classDecl->location, classDecl->name);
    info.selectionRange = info.range;
    for (const auto &member : classDecl->members) {
      if (auto child = buildDocumentSymbol(member.get(), true)) {
        info.children.push_back(std::move(*child));
      }
    }
    return info;
  }
  case ASTNodeType::MethodDecl: {
    const auto *method = static_cast<const MethodDeclNode *>(node);
    info.name = method->name;
    info.kind = classMember && method->name == "constructor"
                    ? SymbolKind::Constructor
                    : SymbolKind::Method;
    info.range = makeRange(method->location, method->name);
    info.selectionRange = info.range;
    return info;
  }
  case ASTNodeType::EnumDecl: {
    const auto *enumDecl = static_cast<const EnumDeclNode *>(node);
    info.name = enumDecl->name;
    info.kind = SymbolKind::Enum;
    info.range = makeRange(enumDecl->location, enumDecl->name);
    info.selectionRange = info.range;
    return info;
  }
  case ASTNodeType::ShaderDecl: {
    const auto *shader = static_cast<const ShaderDeclNode *>(node);
    info.name = shader->name;
    info.kind = SymbolKind::Shader;
    info.range = makeRange(shader->location, shader->name);
    info.selectionRange = info.range;
    return info;
  }
  case ASTNodeType::ModuleDecl: {
    const auto *moduleDecl = static_cast<const ModuleDeclNode *>(node);
    info.name = moduleDecl->moduleName;
    info.kind = SymbolKind::Module;
    info.range = makeRange(moduleDecl->location, moduleDecl->moduleName);
    info.selectionRange = info.range;
    return info;
  }
  case ASTNodeType::BindingDecl: {
    const auto *binding = static_cast<const BindingDeclNode *>(node);
    if (binding->name == "__assign__" || binding->name == "__deref__") {
      return std::nullopt;
    }
    info.name = binding->name;
    info.kind = classMember ? SymbolKind::Field : SymbolKind::Variable;
    info.range = makeRange(binding->location, binding->name);
    info.selectionRange = info.range;
    return info;
  }
  default:
    return std::nullopt;
  }
}

} // namespace neuron::sema_detail
