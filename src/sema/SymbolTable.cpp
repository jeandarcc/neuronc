#include "SymbolTable.h"

#include <algorithm>
#include <unordered_set>

namespace neuron::sema_detail {

namespace {

std::shared_ptr<Scope> makeScope(const std::shared_ptr<Scope> &parent,
                                 const std::string &name) {
  return std::make_shared<Scope>(parent, name);
}

} // namespace

SymbolTable::SymbolTable() { reset(); }

void SymbolTable::reset() {
  m_scopes.clear();
  ScopeEntry global;
  global.scope = makeScope(nullptr, "global");
  global.parent = kInvalidScope;
  m_scopes.push_back(std::move(global));
  m_globalScope = 0;
  m_currentScope = 0;
}

SymbolTable::ScopeId SymbolTable::globalScope() const { return m_globalScope; }

SymbolTable::ScopeId SymbolTable::currentScope() const { return m_currentScope; }

SymbolTable::ScopeId SymbolTable::createScope(ScopeId parent,
                                              const std::string &name) {
  const std::shared_ptr<Scope> parentScope =
      parent != kInvalidScope ? m_scopes[parent].scope : nullptr;
  ScopeEntry entry;
  entry.scope = makeScope(parentScope, name);
  entry.parent = parent;
  m_scopes.push_back(std::move(entry));
  return m_scopes.size() - 1;
}

void SymbolTable::setCurrentScope(ScopeId scopeId) { m_currentScope = scopeId; }

void SymbolTable::pushScope(const std::string &name) {
  m_currentScope = createScope(m_currentScope, name);
}

void SymbolTable::popScope() {
  if (m_currentScope != kInvalidScope &&
      m_scopes[m_currentScope].parent != kInvalidScope) {
    m_currentScope = m_scopes[m_currentScope].parent;
  }
}

SymbolTable::ScopeId SymbolTable::parentScope(ScopeId scopeId) const {
  if (scopeId == kInvalidScope || scopeId >= m_scopes.size()) {
    return kInvalidScope;
  }
  return m_scopes[scopeId].parent;
}

bool SymbolTable::isAtGlobalScope() const { return m_currentScope == m_globalScope; }

Symbol *SymbolTable::defineInScope(ScopeId scopeId, const std::string &name,
                                   Symbol symbol) {
  if (scopeId == kInvalidScope || scopeId >= m_scopes.size()) {
    return nullptr;
  }
  if (!m_scopes[scopeId].scope->define(name, std::move(symbol))) {
    return nullptr;
  }
  return m_scopes[scopeId].scope->lookupLocal(name);
}

Symbol *SymbolTable::defineInCurrentScope(const std::string &name, Symbol symbol) {
  return defineInScope(m_currentScope, name, std::move(symbol));
}

Symbol *SymbolTable::lookupVisible(const std::string &name) {
  return lookupVisible(m_currentScope, name);
}

const Symbol *SymbolTable::lookupVisible(const std::string &name) const {
  return lookupVisible(m_currentScope, name);
}

Symbol *SymbolTable::lookupVisible(ScopeId scopeId, const std::string &name) {
  if (scopeId == kInvalidScope || scopeId >= m_scopes.size()) {
    return nullptr;
  }
  return m_scopes[scopeId].scope->lookup(name);
}

const Symbol *SymbolTable::lookupVisible(ScopeId scopeId,
                                         const std::string &name) const {
  if (scopeId == kInvalidScope || scopeId >= m_scopes.size()) {
    return nullptr;
  }
  return m_scopes[scopeId].scope->lookup(name);
}

Symbol *SymbolTable::lookupGlobal(const std::string &name) {
  if (m_globalScope == kInvalidScope) {
    return nullptr;
  }
  return m_scopes[m_globalScope].scope->lookup(name);
}

const Symbol *SymbolTable::lookupGlobal(const std::string &name) const {
  if (m_globalScope == kInvalidScope) {
    return nullptr;
  }
  return m_scopes[m_globalScope].scope->lookup(name);
}

Symbol *SymbolTable::lookupLocalCurrent(const std::string &name) {
  return lookupLocal(m_currentScope, name);
}

const Symbol *SymbolTable::lookupLocalCurrent(const std::string &name) const {
  return lookupLocal(m_currentScope, name);
}

Symbol *SymbolTable::lookupLocal(ScopeId scopeId, const std::string &name) {
  if (scopeId == kInvalidScope || scopeId >= m_scopes.size()) {
    return nullptr;
  }
  return m_scopes[scopeId].scope->lookupLocal(name);
}

const Symbol *SymbolTable::lookupLocal(ScopeId scopeId,
                                       const std::string &name) const {
  if (scopeId == kInvalidScope || scopeId >= m_scopes.size()) {
    return nullptr;
  }
  return m_scopes[scopeId].scope->lookupLocal(name);
}

std::vector<VisibleSymbolInfo> SymbolTable::snapshotVisibleSymbols() const {
  return snapshotVisibleSymbols(m_currentScope);
}

std::vector<VisibleSymbolInfo>
SymbolTable::snapshotVisibleSymbols(ScopeId scopeId) const {
  std::vector<VisibleSymbolInfo> symbols;
  std::unordered_set<std::string> seenNames;

  while (scopeId != kInvalidScope && scopeId < m_scopes.size()) {
    for (const auto &entry : m_scopes[scopeId].scope->symbols()) {
      if (!seenNames.insert(entry.first).second) {
        continue;
      }
      symbols.push_back(toVisibleSymbol(entry.second));
    }
    scopeId = m_scopes[scopeId].parent;
  }

  std::sort(symbols.begin(), symbols.end(),
            [](const VisibleSymbolInfo &lhs, const VisibleSymbolInfo &rhs) {
              return lhs.name < rhs.name;
            });
  return symbols;
}

std::vector<VisibleSymbolInfo> SymbolTable::localSymbols(ScopeId scopeId) const {
  std::vector<VisibleSymbolInfo> symbols;
  if (scopeId == kInvalidScope || scopeId >= m_scopes.size()) {
    return symbols;
  }

  for (const auto &entry : m_scopes[scopeId].scope->symbols()) {
    symbols.push_back(toVisibleSymbol(entry.second));
  }
  std::sort(symbols.begin(), symbols.end(),
            [](const VisibleSymbolInfo &lhs, const VisibleSymbolInfo &rhs) {
              return lhs.name < rhs.name;
            });
  return symbols;
}

VisibleSymbolInfo SymbolTable::toVisibleSymbol(const Symbol &symbol) {
  VisibleSymbolInfo info;
  info.name = symbol.name;
  info.kind = symbol.kind;
  info.type = symbol.type;
  info.signatureKey = symbol.signatureKey;
  info.isPublic = symbol.isPublic;
  info.isConst = symbol.isConst;
  info.definition = symbol.definition;
  return info;
}

} // namespace neuron::sema_detail
