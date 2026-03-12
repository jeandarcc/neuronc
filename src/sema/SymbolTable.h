#pragma once

#include "neuronc/sema/SemanticAnalyzer.h"

namespace neuron::sema_detail {

class SymbolTable {
public:
  using ScopeId = std::size_t;
  static constexpr ScopeId kInvalidScope = static_cast<ScopeId>(-1);

  SymbolTable();

  void reset();

  ScopeId globalScope() const;
  ScopeId currentScope() const;
  ScopeId createScope(ScopeId parent, const std::string &name);
  void setCurrentScope(ScopeId scopeId);
  void pushScope(const std::string &name = "");
  void popScope();
  ScopeId parentScope(ScopeId scopeId) const;
  bool isAtGlobalScope() const;

  Symbol *defineInScope(ScopeId scopeId, const std::string &name, Symbol symbol);
  Symbol *defineInCurrentScope(const std::string &name, Symbol symbol);

  Symbol *lookupVisible(const std::string &name);
  const Symbol *lookupVisible(const std::string &name) const;
  Symbol *lookupVisible(ScopeId scopeId, const std::string &name);
  const Symbol *lookupVisible(ScopeId scopeId, const std::string &name) const;
  Symbol *lookupGlobal(const std::string &name);
  const Symbol *lookupGlobal(const std::string &name) const;
  Symbol *lookupLocalCurrent(const std::string &name);
  const Symbol *lookupLocalCurrent(const std::string &name) const;
  Symbol *lookupLocal(ScopeId scopeId, const std::string &name);
  const Symbol *lookupLocal(ScopeId scopeId, const std::string &name) const;

  std::vector<VisibleSymbolInfo> snapshotVisibleSymbols() const;
  std::vector<VisibleSymbolInfo> snapshotVisibleSymbols(ScopeId scopeId) const;
  std::vector<VisibleSymbolInfo> localSymbols(ScopeId scopeId) const;

private:
  struct ScopeEntry {
    std::shared_ptr<Scope> scope;
    ScopeId parent = kInvalidScope;
  };

  static VisibleSymbolInfo toVisibleSymbol(const Symbol &symbol);

  std::vector<ScopeEntry> m_scopes;
  ScopeId m_globalScope = kInvalidScope;
  ScopeId m_currentScope = kInvalidScope;
};

} // namespace neuron::sema_detail
