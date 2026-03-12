#pragma once

#include "AnalysisOptions.h"
#include "DiagnosticEmitter.h"
#include "ReferenceTracker.h"
#include "ScopeManager.h"
#include "SymbolTable.h"
#include "TypeResolver.h"

#include <unordered_map>

namespace neuron::sema_detail {

class AnalysisContext {
public:
  struct ShaderBindingInfo {
    std::string typeName;
  };

  struct EntityBindingInfo {
    std::unordered_map<std::string, bool> components;
    std::string parentEntityName;
  };

  class ScopeHandle {
  public:
    ScopeHandle() = default;
    ScopeHandle(AnalysisContext *owner, SymbolTable::ScopeId scopeId);

    const ScopeHandle *operator->() const;
    ScopeHandle *operator->();

    Symbol *lookup(const std::string &name) const;
    Symbol *lookupLocal(const std::string &name) const;
    ScopeHandle parent() const;

    explicit operator bool() const;
    bool operator==(const ScopeHandle &other) const;
    bool operator!=(const ScopeHandle &other) const;

    SymbolTable::ScopeId id() const;

  private:
    AnalysisContext *m_owner = nullptr;
    SymbolTable::ScopeId m_scopeId = SymbolTable::kInvalidScope;
  };

  AnalysisContext(const AnalysisOptions &options, SymbolTable &symbols,
                  TypeResolver &types, ReferenceTracker &references,
                  ScopeManager &scopes, DiagnosticEmitter &diagnostics);

  void reset();
  void setDocumentDeclarations(const std::vector<ASTNode *> &declarations);

  const AnalysisOptions &options() const;

  SymbolTable &symbols();
  const SymbolTable &symbols() const;
  TypeResolver &types();
  const TypeResolver &types() const;
  ReferenceTracker &references();
  ScopeManager &scopes();
  DiagnosticEmitter &diagnostics();
  const DiagnosticEmitter &diagnostics() const;

  ScopeHandle globalScope() const;
  ScopeHandle currentScope() const;
  void setCurrentScope(const ScopeHandle &scope);
  void enterScope(const std::string &name = "");
  void leaveScope();

  int enterControl();
  void leaveControl();
  int controlDepth() const;
  void enterGraphicsFrame();
  void leaveGraphicsFrame();
  bool isInGraphicsFrame() const;

  NTypePtr resolveType(ASTNode *node);
  NTypePtr resolveType(const std::string &name);
  NTypePtr rememberType(const ASTNode *node, NTypePtr type);

  void recordScopeSnapshot(const SourceLocation &location);
  Symbol *defineSymbol(const ScopeHandle &scope, const std::string &name,
                       Symbol symbol,
                       const SourceLocation *definitionLocation = nullptr,
                       int definitionLength = 0);
  void recordReference(Symbol *symbol, const SourceLocation &loc, int length);

  void error(const SourceLocation &loc, const std::string &message);
  void error(const SourceLocation &loc, const std::string &code,
             diagnostics::DiagnosticArguments arguments,
             const std::string &message);
  void errorWithAgentHint(const SourceLocation &loc,
                          const std::string &message, std::string_view hint);

  void registerCallableParamNames(const std::string &callableName,
                                  std::vector<std::string> parameterNames);
  void registerCallableSignature(
      const std::string &callableKey,
      std::vector<CallableParameterInfo> parameters,
      const std::string &returnType);
  void registerShaderBinding(const std::string &shaderName,
                             const std::string &bindingName,
                             const std::string &typeName);
  const ShaderBindingInfo *
  findShaderBinding(const std::string &shaderName,
                    const std::string &bindingName) const;
  void registerMaterialShader(const ScopeHandle &scope,
                              const std::string &materialName,
                              const std::string &shaderName);
  std::string findMaterialShader(const ScopeHandle &scope,
                                 const std::string &materialName) const;
  void registerEntityBinding(const ScopeHandle &scope,
                             const std::string &entityName);
  bool registerEntityComponent(const ScopeHandle &scope,
                               const std::string &entityName,
                               const std::string &componentName);
  bool hasEntityComponent(const ScopeHandle &scope, const std::string &entityName,
                          const std::string &componentName) const;
  void registerTransformBinding(const ScopeHandle &scope,
                                const std::string &transformName,
                                const std::string &entityName);
  std::string findTransformEntity(const ScopeHandle &scope,
                                  const std::string &transformName) const;
  bool setEntityParent(const ScopeHandle &scope, const std::string &entityName,
                       const std::string &parentEntityName);
  const std::vector<std::string> *
  findNamedCallableSignature(const std::string &callableName,
                             std::size_t argumentCount,
                             const SourceLocation &loc);

private:
  void declareBuiltinFunctions();
  ScopeHandle makeScopeHandle(SymbolTable::ScopeId scopeId) const;

  const AnalysisOptions &m_options;
  SymbolTable &m_symbols;
  TypeResolver &m_types;
  ReferenceTracker &m_references;
  ScopeManager &m_scopes;
  DiagnosticEmitter &m_diagnostics;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, ShaderBindingInfo>>
      m_shaderBindings;
  std::unordered_map<std::string, std::string> m_materialShaders;
  std::unordered_map<std::string, EntityBindingInfo> m_entityBindings;
  std::unordered_map<std::string, std::string> m_transformEntities;
  ScopeHandle m_globalScope;
  ScopeHandle m_currentScope;
  int m_controlDepth = 0;
  int m_graphicsFrameDepth = 0;
};

} // namespace neuron::sema_detail
