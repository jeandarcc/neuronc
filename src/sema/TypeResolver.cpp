#include "TypeResolver.h"

#include <algorithm>
#include <functional>
#include <sstream>

namespace neuron::sema_detail {

namespace {

NTypePtr unwrapNullableType(const NTypePtr &type) {
  if (type && type->isNullable() && type->nullableBase()) {
    return type->nullableBase();
  }
  return type;
}

int symbolNameLength(std::string_view name) {
  return std::max(1, static_cast<int>(name.size()));
}

std::string typeDisplayName(const NTypePtr &type) {
  return type ? type->toString() : std::string("<unknown>");
}

} // namespace

void TypeResolver::reset() {
  m_typeRegistry.clear();
  m_classes.clear();
  m_enums.clear();
  m_callableParamNames.clear();
  m_callableSignatures.clear();
  m_moduleMembers.clear();
}

void TypeResolver::setAvailableModules(
    const std::unordered_set<std::string> &modules, bool enforceResolution) {
  m_availableModules.clear();
  for (const auto &module : modules) {
    m_availableModules.insert(normalizeModuleName(module));
  }
  m_enforceModuleResolution = enforceResolution;
}

void TypeResolver::setModuleCppModules(
    const std::unordered_map<std::string, NativeModuleInfo> &modules) {
  m_moduleCppModules.clear();
  for (const auto &entry : modules) {
    std::string normalized = normalizeModuleName(entry.first);
    NativeModuleInfo info = entry.second;
    if (info.name.empty()) {
      info.name = entry.first;
    }
    m_moduleCppModules[normalized] = std::move(info);
  }
}

void TypeResolver::declareBuiltinTypes() {
  m_typeRegistry["void"] = NType::makeVoid();
  m_typeRegistry["int"] = NType::makeInt();
  m_typeRegistry["float"] = NType::makeFloat();
  m_typeRegistry["double"] = NType::makeDouble();
  m_typeRegistry["bool"] = NType::makeBool();
  m_typeRegistry["string"] = NType::makeString();
  m_typeRegistry["char"] = NType::makeString();
  m_typeRegistry["dynamic"] = NType::makeDynamic();
  m_typeRegistry["Color"] = NType::makeClass("Color");
  m_typeRegistry["Vector2"] = NType::makeClass("Vector2");
  m_typeRegistry["Vector3"] = NType::makeClass("Vector3");
  m_typeRegistry["Vector4"] = NType::makeClass("Vector4");
  m_typeRegistry["Matrix4"] = NType::makeClass("Matrix4");
  m_typeRegistry["Dictionary"] =
      NType::makeDictionary(NType::makeUnknown(), NType::makeUnknown());
}

void TypeResolver::registerClass(const std::string &name, bool isPublic,
                                 std::vector<std::string> baseClasses,
                                 SymbolTable::ScopeId scopeId) {
  ClassRecord record;
  record.name = name;
  record.type = NType::makeClass(name);
  record.baseClasses = std::move(baseClasses);
  record.scopeId = scopeId;
  record.isPublic = isPublic;
  m_classes[name] = std::move(record);
}

void TypeResolver::registerEnum(const std::string &name,
                                std::unordered_set<std::string> members) {
  m_typeRegistry[name] = NType::makeEnum(name);
  m_enums[name] = std::move(members);
}

void TypeResolver::registerCallableParamNames(
    const std::string &callableName, std::vector<std::string> parameterNames) {
  if (callableName.empty()) {
    return;
  }
  m_callableParamNames[callableName].push_back(std::move(parameterNames));
}

void TypeResolver::registerCallableSignature(
    const std::string &callableKey, std::vector<CallableParameterInfo> parameters,
    const std::string &returnType) {
  if (callableKey.empty()) {
    return;
  }

  CallableSignatureInfo info;
  info.key = callableKey;
  info.returnType = returnType.empty() ? "void" : returnType;
  info.parameters = std::move(parameters);

  std::ostringstream label;
  label << callableKey << "(";
  for (std::size_t i = 0; i < info.parameters.size(); ++i) {
    if (i > 0) {
      label << ", ";
    }
    label << info.parameters[i].name;
    if (!info.parameters[i].typeName.empty()) {
      label << " as " << info.parameters[i].typeName;
    }
  }
  label << ")";
  if (!info.returnType.empty()) {
    label << " as " << info.returnType;
  }
  info.label = label.str();

  m_callableSignatures[callableKey].push_back(std::move(info));
}

NTypePtr TypeResolver::resolveType(const std::string &name,
                                   const SymbolTable &symbols) const {
  auto typeIt = m_typeRegistry.find(name);
  if (typeIt != m_typeRegistry.end()) {
    return typeIt->second;
  }

  auto classIt = m_classes.find(name);
  if (classIt != m_classes.end()) {
    return classIt->second.type;
  }

  if (const Symbol *sym = symbols.lookupVisible(name)) {
    if (sym->kind == SymbolKind::GenericParameter) {
      return sym->type;
    }
  }

  return NType::makeError();
}

NTypePtr TypeResolver::resolveType(ASTNode *node, const SymbolTable &symbols,
                                   ReferenceTracker &references,
                                   DiagnosticEmitter &diagnostics) const {
  if (node == nullptr) {
    return NType::makeAuto();
  }
  if (node->type == ASTNodeType::Identifier) {
    auto *identifier = static_cast<IdentifierNode *>(node);
    NTypePtr resolved = resolveType(identifier->name, symbols);
    if (resolved->isError()) {
      diagnostics.emit(identifier->location, "Unknown type: " + identifier->name);
      return resolved;
    }

    const Symbol *sym = symbols.lookupVisible(identifier->name);
    if (sym != nullptr && sym->kind != SymbolKind::GenericParameter) {
      sym = nullptr;
    }
    if (sym == nullptr) {
      sym = symbols.lookupGlobal(identifier->name);
      if (sym != nullptr && sym->kind != SymbolKind::Class &&
          sym->kind != SymbolKind::Enum) {
        sym = nullptr;
      }
    }
    if (sym != nullptr) {
      references.recordReference(const_cast<Symbol *>(sym), identifier->location,
                                 symbolNameLength(identifier->name));
    }
    return resolved;
  }

  if (node->type != ASTNodeType::TypeSpec) {
    return NType::makeError();
  }
  auto *typeSpec = static_cast<TypeSpecNode *>(node);

  if (typeSpec->typeName == "dynamic") {
    return NType::makeDynamic();
  }

  if (typeSpec->typeName == "Array" || typeSpec->typeName == "Tensor") {
    if (typeSpec->genericArgs.empty()) {
      diagnostics.emit(typeSpec->location,
                       "Missing generic argument for " + typeSpec->typeName);
      return NType::makeError();
    }

    NTypePtr elementType =
        resolveType(typeSpec->genericArgs[0].get(), symbols, references,
                    diagnostics);
    if (elementType->isError()) {
      return NType::makeError();
    }
    return typeSpec->typeName == "Array" ? NType::makeArray(elementType)
                                          : NType::makeTensor(elementType);
  }

  if (typeSpec->typeName == "Dictionary") {
    if (typeSpec->genericArgs.size() != 2) {
      diagnostics.emit(typeSpec->location,
                       "Dictionary requires two generic arguments: Dictionary<K, V>");
      return NType::makeError();
    }

    auto resolveGenericType = [&](ASTNode *argNode) -> NTypePtr {
      if (argNode == nullptr) {
        return NType::makeUnknown();
      }
      if (argNode->type == ASTNodeType::Identifier ||
          argNode->type == ASTNodeType::TypeSpec) {
        return resolveType(argNode, symbols, references, diagnostics);
      }
      return NType::makeUnknown();
    };

    NTypePtr keyType = resolveGenericType(typeSpec->genericArgs[0].get());
    NTypePtr valueType = resolveGenericType(typeSpec->genericArgs[1].get());
    if (keyType->isError() || valueType->isError()) {
      return NType::makeError();
    }
    return NType::makeDictionary(keyType, valueType);
  }

  NTypePtr resolved = resolveType(typeSpec->typeName, symbols);
  if (resolved->isError()) {
    diagnostics.emit(typeSpec->location, "Unknown type: " + typeSpec->typeName);
    return resolved;
  }

  const Symbol *sym = symbols.lookupVisible(typeSpec->typeName);
  if (sym != nullptr && sym->kind != SymbolKind::GenericParameter) {
    sym = nullptr;
  }
  if (sym == nullptr) {
    sym = symbols.lookupGlobal(typeSpec->typeName);
    if (sym != nullptr && sym->kind != SymbolKind::Class &&
        sym->kind != SymbolKind::Enum) {
      sym = nullptr;
    }
  }
  if (sym != nullptr) {
    references.recordReference(const_cast<Symbol *>(sym), typeSpec->location,
                               symbolNameLength(typeSpec->typeName));
  }
  return resolved;
}

const TypeResolver::ClassRecord *TypeResolver::findClass(
    const std::string &name) const {
  auto it = m_classes.find(name);
  return it != m_classes.end() ? &it->second : nullptr;
}

bool TypeResolver::enumContains(const std::string &enumName,
                                const std::string &memberName) const {
  auto it = m_enums.find(enumName);
  return it != m_enums.end() && it->second.find(memberName) != it->second.end();
}

bool TypeResolver::moduleResolutionEnabled() const {
  return m_enforceModuleResolution;
}

bool TypeResolver::isModuleAvailable(const std::string &name) const {
  return m_availableModules.find(normalizeModuleName(name)) !=
         m_availableModules.end();
}

bool TypeResolver::isModuleCppModuleAvailable(const std::string &name) const {
  return m_moduleCppModules.find(normalizeModuleName(name)) !=
         m_moduleCppModules.end();
}

void TypeResolver::registerModuleMember(const std::string &moduleName,
                                        const std::string &memberName,
                                        NTypePtr type, SymbolKind kind,
                                        const SourceLocation &location,
                                        bool isExpanded) {
  if (moduleName.empty() || memberName.empty()) {
    return;
  }
  const std::string normalized = normalizeModuleName(moduleName);
  ModuleMemberRecord record;
  record.name = memberName;
  record.type = std::move(type);
  record.kind = kind;
  record.location = location;
  record.isExpanded = isExpanded;
  m_moduleMembers[normalized][memberName] = std::move(record);
}

std::optional<TypeResolver::ModuleMemberRecord>
TypeResolver::findModuleMember(const std::string &moduleName,
                               const std::string &memberName) const {
  const std::string normalized = normalizeModuleName(moduleName);
  const auto moduleIt = m_moduleMembers.find(normalized);
  if (moduleIt == m_moduleMembers.end()) {
    return std::nullopt;
  }
  const auto memberIt = moduleIt->second.find(memberName);
  if (memberIt == moduleIt->second.end()) {
    return std::nullopt;
  }
  return memberIt->second;
}

std::vector<TypeResolver::ModuleMemberRecord>
TypeResolver::expandedModuleMembers(const std::string &moduleName) const {
  std::vector<ModuleMemberRecord> members;
  const std::string normalized = normalizeModuleName(moduleName);
  const auto moduleIt = m_moduleMembers.find(normalized);
  if (moduleIt == m_moduleMembers.end()) {
    return members;
  }
  for (const auto &entry : moduleIt->second) {
    if (entry.second.isExpanded) {
      members.push_back(entry.second);
    }
  }
  std::sort(members.begin(), members.end(),
            [](const ModuleMemberRecord &lhs, const ModuleMemberRecord &rhs) {
              return lhs.name < rhs.name;
            });
  return members;
}

std::vector<VisibleSymbolInfo>
TypeResolver::getTypeMembers(const NTypePtr &type,
                             const SymbolTable &symbols) const {
  std::vector<VisibleSymbolInfo> members;
  std::unordered_set<std::string> seenNames;
  NTypePtr resolved = unwrapNullableType(type);
  if (!resolved) {
    return members;
  }

  const auto appendMember = [&](VisibleSymbolInfo info) {
    if (!seenNames.insert(info.name).second) {
      return;
    }
    members.push_back(std::move(info));
  };

  if (resolved->kind == TypeKind::Class) {
    std::function<void(const std::string &)> collectClassMembers =
        [&](const std::string &className) {
          auto classIt = m_classes.find(className);
          if (classIt == m_classes.end() ||
              classIt->second.scopeId == SymbolTable::kInvalidScope) {
            return;
          }
          for (const auto &entry : symbols.localSymbols(classIt->second.scopeId)) {
            if (entry.name == "this" ||
                entry.kind == SymbolKind::GenericParameter) {
              continue;
            }
            appendMember(entry);
          }
          for (const auto &baseClass : classIt->second.baseClasses) {
            collectClassMembers(baseClass);
          }
        };
    collectClassMembers(resolved->name);
  } else if (resolved->kind == TypeKind::Enum) {
    auto enumIt = m_enums.find(resolved->name);
    if (enumIt != m_enums.end()) {
      std::vector<std::string> names(enumIt->second.begin(), enumIt->second.end());
      std::sort(names.begin(), names.end());
      for (const auto &memberName : names) {
        VisibleSymbolInfo info;
        info.name = memberName;
        info.kind = SymbolKind::Field;
        info.type = resolved;
        appendMember(std::move(info));
      }
    }
  } else if (resolved->kind == TypeKind::Module) {
    const std::string normalized = normalizeModuleName(resolved->name);
    auto moduleMemberIt = m_moduleMembers.find(normalized);
    if (moduleMemberIt != m_moduleMembers.end()) {
      std::vector<std::string> memberNames;
      memberNames.reserve(moduleMemberIt->second.size());
      for (const auto &entry : moduleMemberIt->second) {
        memberNames.push_back(entry.first);
      }
      std::sort(memberNames.begin(), memberNames.end());
      for (const auto &memberName : memberNames) {
        const auto &record = moduleMemberIt->second.at(memberName);
        VisibleSymbolInfo info;
        info.name = record.name;
        info.kind = record.kind;
        info.type = record.type;
        appendMember(std::move(info));
      }
    }

    auto moduleIt = m_moduleCppModules.find(normalized);
    if (moduleIt != m_moduleCppModules.end()) {
      std::vector<std::string> exportNames;
      exportNames.reserve(moduleIt->second.exports.size());
      for (const auto &entry : moduleIt->second.exports) {
        exportNames.push_back(entry.first);
      }
      std::sort(exportNames.begin(), exportNames.end());
      for (const auto &exportName : exportNames) {
        const auto &signature = moduleIt->second.exports.at(exportName);
        std::vector<NTypePtr> params;
        params.reserve(signature.parameterTypeNames.size());
        for (const auto &paramTypeName : signature.parameterTypeNames) {
          params.push_back(resolveType(paramTypeName, symbols));
        }
        NTypePtr returnType = resolveType(signature.returnTypeName, symbols);
        VisibleSymbolInfo info;
        info.name = exportName;
        info.kind = SymbolKind::Method;
        info.type = NType::makeMethod(returnType, std::move(params));
        info.signatureKey = resolved->name + "." + exportName;
        appendMember(std::move(info));
      }
    }
  } else if (resolved->kind == TypeKind::Array ||
             resolved->kind == TypeKind::Tensor) {
    VisibleSymbolInfo lengthMember;
    lengthMember.name = "Length";
    lengthMember.kind = SymbolKind::Field;
    lengthMember.type = NType::makeInt();
    appendMember(std::move(lengthMember));

    if (resolved->kind == TypeKind::Tensor) {
      for (const std::string methodName :
           {"Random", "Zeros", "Ones", "Identity"}) {
        VisibleSymbolInfo methodMember;
        methodMember.name = methodName;
        methodMember.kind = SymbolKind::Method;
        methodMember.type = resolved;
        appendMember(std::move(methodMember));
      }
    }
  }

  std::sort(members.begin(), members.end(),
            [](const VisibleSymbolInfo &lhs, const VisibleSymbolInfo &rhs) {
              return lhs.name < rhs.name;
            });
  return members;
}

std::vector<CallableSignatureInfo>
TypeResolver::getCallableSignatures(std::string_view callableKey) const {
  auto found = m_callableSignatures.find(std::string(callableKey));
  if (found != m_callableSignatures.end()) {
    return found->second;
  }

  auto classIt = m_classes.find(std::string(callableKey));
  if (classIt != m_classes.end()) {
    auto ctorIt =
        m_callableSignatures.find(std::string(callableKey) + ".constructor");
    if (ctorIt != m_callableSignatures.end()) {
      return ctorIt->second;
    }
  }

  const std::string key(callableKey);
  const std::size_t dotPos = key.find('.');
  if (dotPos != std::string::npos) {
    const std::string moduleName = key.substr(0, dotPos);
    const std::string memberName = key.substr(dotPos + 1);
    const std::string normalized = normalizeModuleName(moduleName);
    auto moduleIt = m_moduleCppModules.find(normalized);
    if (moduleIt != m_moduleCppModules.end()) {
      auto exportIt = moduleIt->second.exports.find(memberName);
      if (exportIt != moduleIt->second.exports.end()) {
        CallableSignatureInfo info;
        info.key = key;
        info.returnType = exportIt->second.returnTypeName.empty()
                              ? "void"
                              : exportIt->second.returnTypeName;
        for (std::size_t i = 0; i < exportIt->second.parameterTypeNames.size();
             ++i) {
          info.parameters.push_back(
              {"arg" + std::to_string(i + 1),
               exportIt->second.parameterTypeNames[i]});
        }
        std::ostringstream label;
        label << key << "(";
        for (std::size_t i = 0; i < info.parameters.size(); ++i) {
          if (i > 0) {
            label << ", ";
          }
          label << info.parameters[i].name << " as "
                << info.parameters[i].typeName;
        }
        label << ") as " << info.returnType;
        info.label = label.str();
        return {std::move(info)};
      }
    }
  }

  return {};
}

TypeResolver::CallableParamMatch TypeResolver::findNamedCallableSignature(
    const std::string &callableName, std::size_t argumentCount) const {
  auto it = m_callableParamNames.find(callableName);
  if (it == m_callableParamNames.end()) {
    return {CallableParamMatchStatus::MissingCallable, nullptr};
  }

  const std::vector<std::string> *match = nullptr;
  std::size_t matchCount = 0;
  for (const auto &signature : it->second) {
    if (signature.size() == argumentCount) {
      match = &signature;
      ++matchCount;
    }
  }

  if (matchCount == 0) {
    return {CallableParamMatchStatus::NoArityMatch, nullptr};
  }
  if (matchCount > 1) {
    return {CallableParamMatchStatus::Ambiguous, nullptr};
  }
  return {CallableParamMatchStatus::Ok, match};
}

NTypePtr TypeResolver::moduleCppMemberType(const std::string &moduleName,
                                           const std::string &memberName,
                                           const SymbolTable &symbols,
                                           DiagnosticEmitter &diagnostics,
                                           const SourceLocation &loc) const {
  const std::string normalized = normalizeModuleName(moduleName);
  auto moduleIt = m_moduleCppModules.find(normalized);
  if (moduleIt == m_moduleCppModules.end()) {
    return NType::makeUnknown();
  }

  auto exportIt = moduleIt->second.exports.find(memberName);
  if (exportIt == moduleIt->second.exports.end()) {
    diagnostics.emit(loc, "modulecpp '" + moduleIt->second.name +
                              "' has no export named '" + memberName + "'");
    return NType::makeError();
  }

  const NativeModuleExportSignature &signature = exportIt->second;
  std::vector<NTypePtr> params;
  params.reserve(signature.parameterTypeNames.size());
  for (const auto &paramType : signature.parameterTypeNames) {
    params.push_back(nativeBoundaryType(paramType, diagnostics, loc, symbols));
  }
  NTypePtr returnType =
      signature.returnTypeName.empty()
          ? NType::makeVoid()
          : nativeBoundaryType(signature.returnTypeName, diagnostics, loc,
                               symbols);
  return NType::makeMethod(returnType, std::move(params));
}

NTypePtr TypeResolver::nativeBoundaryType(const std::string &typeName,
                                          DiagnosticEmitter &diagnostics,
                                          const SourceLocation &loc,
                                          const SymbolTable &symbols) const {
  NTypePtr resolved = resolveType(typeName, symbols);
  if (!resolved->isError()) {
    return resolved;
  }

  diagnostics.emit(loc, "Unsupported modulecpp boundary type: " + typeName);
  return NType::makeError();
}

Symbol *TypeResolver::lookupClassMemberRecursive(
    const std::string &className, const std::string &memberName,
    SymbolTable &symbols, std::unordered_set<std::string> *visiting) const {
  if (visiting == nullptr) {
    return nullptr;
  }
  if (visiting->find(className) != visiting->end()) {
    return nullptr;
  }
  visiting->insert(className);

  auto classIt = m_classes.find(className);
  if (classIt == m_classes.end()) {
    return nullptr;
  }

  if (auto *local = symbols.lookupLocal(classIt->second.scopeId, memberName)) {
    return local;
  }

  for (const auto &baseName : classIt->second.baseClasses) {
    if (auto *sym =
            lookupClassMemberRecursive(baseName, memberName, symbols, visiting)) {
      return sym;
    }
  }

  return nullptr;
}

std::string TypeResolver::normalizeModuleName(const std::string &name) {
  std::string out = name;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

} // namespace neuron::sema_detail
