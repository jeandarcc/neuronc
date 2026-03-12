#pragma once

#include "DiagnosticEmitter.h"
#include "ReferenceTracker.h"
#include "SymbolTable.h"

namespace neuron::sema_detail {

class TypeResolver {
public:
  enum class CallableParamMatchStatus {
    MissingCallable,
    NoArityMatch,
    Ambiguous,
    Ok,
  };

  struct CallableParamMatch {
    CallableParamMatchStatus status = CallableParamMatchStatus::MissingCallable;
    const std::vector<std::string> *parameters = nullptr;
  };

  struct ClassRecord {
    std::string name;
    NTypePtr type;
    std::vector<std::string> baseClasses;
    SymbolTable::ScopeId scopeId = SymbolTable::kInvalidScope;
    bool isPublic = false;
  };

  struct ModuleMemberRecord {
    std::string name;
    NTypePtr type;
    SymbolKind kind = SymbolKind::Method;
    SourceLocation location;
    bool isExpanded = false;
  };

  void reset();

  void setAvailableModules(const std::unordered_set<std::string> &modules,
                           bool enforceResolution);
  void setModuleCppModules(
      const std::unordered_map<std::string, NativeModuleInfo> &modules);

  void declareBuiltinTypes();
  void registerClass(const std::string &name, bool isPublic,
                     std::vector<std::string> baseClasses,
                     SymbolTable::ScopeId scopeId);
  void registerEnum(const std::string &name,
                    std::unordered_set<std::string> members);
  void registerCallableParamNames(const std::string &callableName,
                                  std::vector<std::string> parameterNames);
  void registerCallableSignature(
      const std::string &callableKey,
      std::vector<CallableParameterInfo> parameters,
      const std::string &returnType);

  NTypePtr resolveType(const std::string &name,
                       const SymbolTable &symbols) const;
  NTypePtr resolveType(ASTNode *node, const SymbolTable &symbols,
                       ReferenceTracker &references,
                       DiagnosticEmitter &diagnostics) const;

  const ClassRecord *findClass(const std::string &name) const;
  bool enumContains(const std::string &enumName,
                    const std::string &memberName) const;
  bool moduleResolutionEnabled() const;
  bool isModuleAvailable(const std::string &name) const;
  bool isModuleCppModuleAvailable(const std::string &name) const;

  std::vector<VisibleSymbolInfo> getTypeMembers(const NTypePtr &type,
                                                const SymbolTable &symbols) const;
  std::vector<CallableSignatureInfo>
  getCallableSignatures(std::string_view callableKey) const;
  CallableParamMatch
  findNamedCallableSignature(const std::string &callableName,
                             std::size_t argumentCount) const;

  NTypePtr moduleCppMemberType(const std::string &moduleName,
                               const std::string &memberName,
                               const SymbolTable &symbols,
                               DiagnosticEmitter &diagnostics,
                               const SourceLocation &loc) const;
  NTypePtr nativeBoundaryType(const std::string &typeName,
                              DiagnosticEmitter &diagnostics,
                              const SourceLocation &loc,
                              const SymbolTable &symbols) const;
  Symbol *lookupClassMemberRecursive(const std::string &className,
                                     const std::string &memberName,
                                     SymbolTable &symbols,
                                     std::unordered_set<std::string> *visiting) const;

  void registerModuleMember(const std::string &moduleName,
                            const std::string &memberName, NTypePtr type,
                            SymbolKind kind, const SourceLocation &location,
                            bool isExpanded = false);
  std::optional<ModuleMemberRecord>
  findModuleMember(const std::string &moduleName,
                   const std::string &memberName) const;
  std::vector<ModuleMemberRecord>
  expandedModuleMembers(const std::string &moduleName) const;

private:
  static std::string normalizeModuleName(const std::string &name);

  std::unordered_map<std::string, NTypePtr> m_typeRegistry;
  std::unordered_map<std::string, ClassRecord> m_classes;
  std::unordered_map<std::string, std::unordered_set<std::string>> m_enums;
  std::unordered_map<std::string, std::vector<std::vector<std::string>>>
      m_callableParamNames;
  std::unordered_map<std::string, std::vector<CallableSignatureInfo>>
      m_callableSignatures;
  std::unordered_set<std::string> m_availableModules = {
      normalizeModuleName("System"),   normalizeModuleName("Math"),
      normalizeModuleName("IO"),       normalizeModuleName("Time"),
      normalizeModuleName("Random"),   normalizeModuleName("Logger"),
      normalizeModuleName("Tensor"),   normalizeModuleName("NN"),
      normalizeModuleName("Resource"), normalizeModuleName("Graphics"),
  };
  std::unordered_map<std::string, std::unordered_map<std::string, ModuleMemberRecord>>
      m_moduleMembers;
  std::unordered_map<std::string, NativeModuleInfo> m_moduleCppModules;
  bool m_enforceModuleResolution = false;
};

} // namespace neuron::sema_detail
